#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ADC manager
 * 
 * @return ESP_OK on success
 */
esp_err_t adc_manager_init(void);

/**
 * @brief Read voltage from ADC (with 1:1 voltage divider compensation)
 * 
 * @param voltage_mv Pointer to store voltage in millivolts
 * @return ESP_OK on success
 */
esp_err_t adc_manager_read_voltage(int *voltage_mv);

/**
 * @brief Read raw ADC value
 * 
 * @param raw_value Pointer to store raw ADC value (0-4095)
 * @return ESP_OK on success
 */
esp_err_t adc_manager_read_raw(int *raw_value);

/**
 * @brief Get ADC GPIO number
 */
int adc_manager_get_gpio(void);

#ifdef __cplusplus
}
#endif
