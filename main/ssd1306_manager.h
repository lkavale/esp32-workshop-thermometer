#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Screen identifiers
 */
typedef enum {
    SCREEN_TEMPERATURES = 0,  // DS18B20 + DHT22 temperatures
    SCREEN_ADC,               // ADC voltage measurement
    SCREEN_SYSTEM,            // System info (memory, etc)
    SCREEN_NETWORK,           // Network info (WiFi, IP)
    SCREEN_COUNT              // Total number of screens
} screen_id_t;

/**
 * @brief Initialize the SSD1306 OLED display
 * 
 * This function initializes the I2C bus and SSD1306 display
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ssd1306_manager_init(void);

/**
 * @brief Set current screen
 * 
 * @param screen Screen to display
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ssd1306_manager_set_screen(screen_id_t screen);

/**
 * @brief Get current screen
 * 
 * @return Current screen ID
 */
screen_id_t ssd1306_manager_get_screen(void);

/**
 * @brief Switch to next screen
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ssd1306_manager_next_screen(void);

/**
 * @brief Switch to previous screen
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ssd1306_manager_prev_screen(void);

void set_temp_values(float ds_temp, float dht_temp, float dht_humidity);
void set_voltage_value(int voltage);
esp_err_t ssd1306_manager_update_display();
/**
 * @brief Clear the OLED display
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ssd1306_manager_clear(void);

#ifdef __cplusplus
}
#endif
