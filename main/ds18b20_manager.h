#pragma once

#include "esp_err.h"
#include "onewire_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the DS18B20 manager
 * 
 * This function initializes the 1-wire bus and searches for DS18B20 devices
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ds18b20_manager_init(void);

/**
 * @brief Search for DS18B20 devices on the 1-wire bus
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ds18b20_manager_search_devices(void);

/**
 * @brief Read temperature from a DS18B20 device
 * 
 * @param device_index Index of the device (0 to device_count-1)
 * @param temperature Pointer to store the temperature value in Celsius
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ds18b20_manager_read_temperature(int device_index, float *temperature);

/**
 * @brief Get the number of DS18B20 devices found
 * 
 * @return Number of DS18B20 devices
 */
int ds18b20_manager_get_device_count(void);

/**
 * @brief Get the address of a DS18B20 device
 * 
 * @param device_index Index of the device (0 to device_count-1)
 * @param address Pointer to store the device address
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ds18b20_manager_get_device_address(int device_index, const char *rom_code);

/**
 * @brief Deinitialize the DS18B20 manager
 * 
 * This function cleans up all resources used by the DS18B20 manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ds18b20_manager_deinit(void);

#ifdef __cplusplus
}
#endif
