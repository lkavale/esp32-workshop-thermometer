#ifndef ADC_MANAGER_H
#define ADC_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ADC manager
 * 
 * Configures the ADC hardware for voltage measurements with the following settings:
 * - 12-bit resolution (0-4095)
 * - 0-3.3V input range (with attenuation)
 * - Automatic calibration (if supported by chip)
 * - Voltage divider compensation (1:1 ratio = multiply by 2)
 * 
 * Must be called before adc_manager_read_voltage().
 * 
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if already initialized
 *         Other error codes on failure
 */
esp_err_t adc_manager_init(void);

/**
 * @brief Read voltage from ADC with voltage divider compensation
 * 
 * Performs a single ADC measurement and converts it to voltage using:
 * 1. Raw ADC reading (0-4095)
 * 2. Calibration (if available) or manual conversion
 * 3. Voltage divider compensation (multiply by 2 for 1:1 divider)
 * 
 * @param voltage_mv Pointer to store the measured voltage in millivolts (required)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if voltage_mv is NULL
 *         ESP_ERR_INVALID_STATE if ADC not initialized
 *         Other error codes on ADC read failure
 * 
 * @note The returned voltage accounts for the external voltage divider.
 * @note For example: if ADC reads 1650mV (half of 3.3V), the function returns 3300mV.
 */
esp_err_t adc_manager_read_voltage(int *voltage_mv);

#ifdef __cplusplus
}
#endif

#endif // ADC_MANAGER_H
