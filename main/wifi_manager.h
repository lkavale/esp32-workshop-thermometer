#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <esp_err.h>

/**
 * @brief Initialize WiFi in station mode and connect to the configured network.
 *
 * @return ESP_OK if WiFi initialized and started, error code otherwise.
 */
esp_err_t wifi_init(void);

#endif // WIFI_MANAGER_H