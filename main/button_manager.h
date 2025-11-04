#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Button event types
 */
typedef enum {
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED,
    BUTTON_EVENT_LONG_PRESS,
} button_event_t;

/**
 * @brief Button callback function type
 * 
 * @param gpio GPIO number of the button
 * @param event Type of button event
 */
typedef void (*button_callback_t)(gpio_num_t gpio, button_event_t event);

/**
 * @brief Initialize button manager
 * 
 * @param callback Function to call when button event occurs
 * @return ESP_OK on success
 */
esp_err_t button_manager_init(button_callback_t callback);

/**
 * @brief Get button 0 GPIO number
 */
gpio_num_t button_manager_get_button0_gpio(void);

/**
 * @brief Get button 1 GPIO number
 */
gpio_num_t button_manager_get_button1_gpio(void);

#ifdef __cplusplus
}
#endif
