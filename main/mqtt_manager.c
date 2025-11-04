#include "mqtt_manager.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "esp_err.h"
#include "cert/cert.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include "esp_netif_types.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "mqtt_manager";
static EventGroupHandle_t network_event_group = NULL;
static const int CONNECTED_BIT = BIT0;
static const int subscribe_qos = 1;

static const char **subscribe_topics = NULL;
static size_t subscribe_topic_count = 0;

/* store mqtt client handle for publish / subscribe helpers */
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

/* subscribe single topic  */
static esp_err_t mqtt_manager_subscribe(esp_mqtt_client_handle_t client, const char *topic)
{
    if (!client || !topic) {
        ESP_LOGE(TAG, "Invalid arguments, client or topic is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    int status = esp_mqtt_client_subscribe(client, topic, subscribe_qos);
    if (status >= 0) {
        ESP_LOGI(TAG, "Subscribed to %s, msg_id=%d", topic, status);
    } else {
        ESP_LOGE(TAG, "Failed to subscribe to %s", topic);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* subscribe many topics (topics are used as provided) */
static esp_err_t mqtt_manager_subscribe_many(esp_mqtt_client_handle_t client, const char *topics[], size_t count)
{
    if (!client || !topics) {
        ESP_LOGE(TAG, "Invalid arguments, client or topics is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Subscribing %zu topics", count);
    uint8_t success_count = 0;
    for (size_t i = 0; i < count; ++i) {
        esp_err_t status = mqtt_manager_subscribe(client, topics[i]);
        if (status == ESP_OK)
            success_count++;
    }

    ESP_LOGI(TAG, "Subscribed %u of %zu topics successfully", success_count, count);

    return ESP_OK;
}

/**
 * @brief Handle IP events.
 *
 * @param arg User argument (unused).
 * @param event_base Event base (should be IP_EVENT).
 * @param event_id Event ID.
 * @param event_data Event data pointer.
 */
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base != IP_EVENT) {
        ESP_LOGE(TAG, "IP handler received non-IP_EVENT: %s", event_base);
        return;
    }

    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            xEventGroupSetBits(network_event_group, CONNECTED_BIT);
            break;
        case IP_EVENT_STA_LOST_IP:
            xEventGroupClearBits(network_event_group, CONNECTED_BIT);
            break;
        default:
            ESP_LOGI(TAG, "Unhandled IP event: %" PRId32, event_id);
            break;
    }
}

/**
 * @brief MQTT event handler.
 *
 * @param handler_args User argument (unused).
 * @param base Event base.
 * @param event_id Event ID.
 * @param event_data Event data pointer.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            if (subscribe_topics && subscribe_topic_count > 0)
                mqtt_manager_subscribe_many(client, subscribe_topics, subscribe_topic_count);

            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA: TOPIC=%.*s DATA=%.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            esp_restart();
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT disconnected with return code %d", event->error_handle->connect_return_code);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                if (event->error_handle->esp_tls_last_esp_err)
                    ESP_LOGE(TAG, "esp_tls reported error: 0x%x", event->error_handle->esp_tls_last_esp_err);
                if (event->error_handle->esp_tls_stack_err)
                    ESP_LOGE(TAG, "esp_tls stack reported error: 0x%x", event->error_handle->esp_tls_stack_err);   
                if (event->error_handle->esp_transport_sock_errno)
                    ESP_LOGE(TAG, "socket transport reported error: 0x%x", event->error_handle->esp_transport_sock_errno);
            }
            esp_restart();
        default:
            ESP_LOGI(TAG, "Other event id: %d", event->event_id);
            break;
    }
}

/**
 * @brief Publish wrapper using stored client handle.
 */
int mqtt_manager_publish(const char *topic, const char *payload, int qos, bool retain)
{
    if (!s_mqtt_client || !topic || !payload) {
        ESP_LOGE(TAG, "mqtt_manager_publish: client not ready or args NULL");
        return -1;
    }
    int len = (int)strlen(payload);
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, len, qos, retain);
    if (msg_id >= 0) {
        ESP_LOGD(TAG, "Published to %s (len=%d) msg_id=%d", topic, len, msg_id);
    } else {
        ESP_LOGE(TAG, "Publish failed for %s", topic);
    }
    return msg_id;
}

/**
 * @brief Initialize and start MQTT client.
 * Waits for WiFi connection before starting MQTT.
 * @param topics Array of topic strings to subscribe.
 * @param topic_count Number of topics in the array.
 */
void mqtt_init(const char *topics[], size_t topic_count)
{
    /* store references */
    subscribe_topics = topics;
    subscribe_topic_count = topic_count;

    if (network_event_group == NULL) {
        network_event_group = xEventGroupCreate();
    }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));

    /* Wait for WiFi connection */
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    xEventGroupWaitBits(network_event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    /* prepare last will payload (DISCONNECTED) */
    static char last_will_payload[128];
    /*heating_status_message_t lw = {
        .status = DEVICE_DISCONNECTED
    };

    if (!heating_status_message_format(&lw, last_will_payload, sizeof(last_will_payload))) {
        ESP_LOGW(TAG, "Failed to format last will payload");
        last_will_payload[0] = '\0';
    }*/

    esp_mqtt_client_config_t mqtt5_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .broker.verification.certificate = (const char *)ca_root_cert_start,
        .session.protocol_ver = MQTT_PROTOCOL_V_5,
        .network.disable_auto_reconnect = false,
        .credentials.authentication.certificate = (const char *)client_cert_start,
        .credentials.authentication.key = (const char *)client_key_start,
        .credentials.client_id = "thermometer",
        .session.keepalive = 60,
        .buffer.size = 1024,
        .buffer.out_size = 1024,
        /*.session.last_will.topic = "/stochov/controller/heating",
        .session.last_will.msg = last_will_payload,
        .session.last_will.msg_len = (uint32_t)strlen(last_will_payload),
        .session.last_will.qos = 1,
        .session.last_will.retain = true*/
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt5_cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    /* store client handle for publish helpers */
    s_mqtt_client = client;

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %d", err);
        s_mqtt_client = NULL;
        return;
    }
    ESP_LOGI(TAG, "MQTT client started");
}