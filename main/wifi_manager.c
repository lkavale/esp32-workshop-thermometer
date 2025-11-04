#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include "sdkconfig.h"

static const char *TAG = "wifi_manager";

/**
 * @brief Connect to the configured WiFi network.
 *
 * @return ESP_OK if connection started, error code otherwise.
 */
static void wifi_connect() {
    esp_err_t status = esp_wifi_connect();
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to SSID %s, error: %d", CONFIG_WIFI_SSID, status);
    }
}

/**
 * @brief Print current WiFi RSSI (signal strength).
 *
 * @return ESP_OK if RSSI printed, error code otherwise.
 */
static void print_wifi_rssi(void) {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ESP_LOGI(TAG, "Current RSSI: %d dBm", ap_info.rssi);
    } else {
        ESP_LOGE(TAG, "Failed to get AP info");
    }
}

/**
 * @brief Handle WiFi events.
 *
 * @param arg User argument (unused).
 * @param event_base Event base (should be WIFI_EVENT).
 * @param event_id Event ID.
 * @param event_data Event data pointer.
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base != WIFI_EVENT) {
        ESP_LOGE(TAG, "WiFi handler received non-WIFI_EVENT: %s", event_base);
        return;
    }

    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi started, connecting to %s", CONFIG_WIFI_SSID);
            wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGE(TAG, "WiFi disconnected, retrying connection to %s", CONFIG_WIFI_SSID);
            wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected to %s", CONFIG_WIFI_SSID);
            print_wifi_rssi();
            break;
        case WIFI_EVENT_STA_BSS_RSSI_LOW:
            ESP_LOGW(TAG, "WiFi RSSI low on %s", CONFIG_WIFI_SSID);
            print_wifi_rssi();
            break;
        default:
            ESP_LOGI(TAG, "Unhandled WiFi event: %" PRId32, event_id);
            break;
    }
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
                            int32_t event_id, void* event_data) {
    if (event_base != IP_EVENT) {
        ESP_LOGE(TAG, "IP handler received non-IP_EVENT: %s", event_base);
        return;
    }

    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "WiFi got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGE(TAG, "WiFi lost IP, reconnecting to %s", CONFIG_WIFI_SSID);
            wifi_connect();
            break;
        default:
            ESP_LOGI(TAG, "Unhandled IP event: %" PRId32, event_id);
            break;
    }
}

/**
 * @brief Initialize WiFi in station mode and connect to the configured network.
 *
 * @return ESP_OK if WiFi initialized and started, error code otherwise.
 */
esp_err_t wifi_init(void) {
    ESP_LOGI(TAG, "WiFi initialization started");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP stack and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));

    // Configure WiFi connection
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization finished");

    return ESP_OK;
}