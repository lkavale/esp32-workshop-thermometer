#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize DHT22 sensor
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t dht22_manager_init(void);

/**
 * @brief Read temperature and humidity from the DHT22 sensor
 * 
 * @param temperature Pointer to store the temperature value in Celsius
 * @param humidity Pointer to store the humidity value in percent
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t dht22_manager_read_data(float *temperature, float *humidity);

/**
 * @brief Get the GPIO pin number used for DHT22
 * 
 * @return GPIO pin number
 */
gpio_num_t dht22_manager_get_gpio(void);

#ifdef __cplusplus
}
#endif
