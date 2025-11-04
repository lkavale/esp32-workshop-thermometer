#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stddef.h>
#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief Initialize and start the MQTT manager.
 *
 * Creates and starts the MQTT client, waits for network if needed and
 * subscribes to topics provided in the topics array (the manager stores
 * the pointer for later use).
 *
 * @param topics Array of topic strings to subscribe to.
 * @param topic_count Number of topics in the array.
 */
void mqtt_init(const char *topics[], size_t topic_count);

/**
 * @brief Publish payload to topic.
 *
 * Wrapper around esp_mqtt_client_publish using the mqtt_manager's client handle.
 *
 * @param topic Full topic string to publish to (e.g. "/stochov/1.1/heating/state").
 * @param payload NUL-terminated payload string.
 * @param qos MQTT qos.
 * @param retain retain flag.
 * @return message id (>=0) on success, -1 on error (e.g. client not started).
 */
int mqtt_manager_publish(const char *topic, const char *payload, int qos, bool retain);

#endif // MQTT_MANAGER_H