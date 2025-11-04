#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "button_manager.h"

static const char *TAG = "button_manager";

// Debounce settings
#define DEBOUNCE_TIME_MS 50
#define LONG_PRESS_TIME_MS 2000

// Button state tracking
typedef struct {
    gpio_num_t gpio;
    bool last_state;
    TickType_t press_time;
    bool long_press_triggered;
} button_state_t;

static button_state_t buttons[2];
static button_callback_t user_callback = NULL;
static QueueHandle_t button_event_queue = NULL;
static TaskHandle_t button_task_handle = NULL;

/**
 * @brief ISR handler for button interrupts
 */
static void IRAM_ATTR button_isr_handler(void *arg)
{
    gpio_num_t gpio = (gpio_num_t)(int)arg;
    xQueueSendFromISR(button_event_queue, &gpio, NULL);
}

/**
 * @brief Task to handle button events
 */
static void button_task(void *arg)
{
    gpio_num_t gpio;
    
    while (1) {
        if (xQueueReceive(button_event_queue, &gpio, portMAX_DELAY)) {
            // Find button state
            button_state_t *btn = NULL;
            for (int i = 0; i < 2; i++) {
                if (buttons[i].gpio == gpio) {
                    btn = &buttons[i];
                    break;
                }
            }
            
            if (btn == NULL) continue;
            
            // Debounce delay
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
            
            // Read current state (active HIGH - pressed = 1, inverted polarity)
            int level = gpio_get_level(gpio);
            bool pressed = (level == 1);
            
            // Check if state changed
            if (pressed != btn->last_state) {
                btn->last_state = pressed;
                
                if (pressed) {
                    // Button pressed
                    btn->press_time = xTaskGetTickCount();
                    btn->long_press_triggered = false;
                    
                    ESP_LOGI(TAG, "Button GPIO %d pressed", gpio);
                    
                    if (user_callback) {
                        user_callback(gpio, BUTTON_EVENT_PRESSED);
                    }
                } else {
                    // Button released
                    TickType_t press_duration = xTaskGetTickCount() - btn->press_time;
                    uint32_t press_duration_ms = press_duration * portTICK_PERIOD_MS;
                    
                    ESP_LOGI(TAG, "Button GPIO %d released (held for %lu ms)", gpio, press_duration_ms);
                    
                    if (user_callback) {
                        user_callback(gpio, BUTTON_EVENT_RELEASED);
                    }
                }
            }
            
            // Check for long press (while button is still held)
            if (pressed && !btn->long_press_triggered) {
                TickType_t current_time = xTaskGetTickCount();
                TickType_t press_duration = current_time - btn->press_time;
                uint32_t press_duration_ms = press_duration * portTICK_PERIOD_MS;
                
                if (press_duration_ms >= LONG_PRESS_TIME_MS) {
                    btn->long_press_triggered = true;
                    ESP_LOGI(TAG, "Button GPIO %d long press detected", gpio);
                    
                    if (user_callback) {
                        user_callback(gpio, BUTTON_EVENT_LONG_PRESS);
                    }
                }
            }
        }
    }
}

esp_err_t button_manager_init(button_callback_t callback)
{
    ESP_LOGI(TAG, "Initializing button manager");
    
    user_callback = callback;
    
    // Initialize button states
    buttons[0].gpio = CONFIG_BUTTON0_GPIO;
    buttons[0].last_state = false; // Not pressed (inverted - LOW when not pressed)
    buttons[0].press_time = 0;
    buttons[0].long_press_triggered = false;
    
    buttons[1].gpio = CONFIG_BUTTON1_GPIO;
    buttons[1].last_state = false; // Not pressed (inverted - LOW when not pressed)
    buttons[1].press_time = 0;
    buttons[1].long_press_triggered = false;
    
    // Create event queue
    button_event_queue = xQueueCreate(10, sizeof(gpio_num_t));
    if (button_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button event queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Configure GPIO for buttons (inverted polarity - pull-down, active HIGH)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_BUTTON0_GPIO) | (1ULL << CONFIG_BUTTON1_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   // Pull-down for inverted polarity
        .intr_type = GPIO_INTR_ANYEDGE,         // Trigger on both edges
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Install GPIO ISR service
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Add ISR handlers for both buttons
    gpio_isr_handler_add(CONFIG_BUTTON0_GPIO, button_isr_handler, (void *)CONFIG_BUTTON0_GPIO);
    gpio_isr_handler_add(CONFIG_BUTTON1_GPIO, button_isr_handler, (void *)CONFIG_BUTTON1_GPIO);
    
    // Create button handling task (increased stack size for display operations)
    BaseType_t task_created = xTaskCreate(
        button_task,
        "button_task",
        4096,  // Increased from 2048 to 4096 bytes
        NULL,
        5,
        &button_task_handle
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Button manager initialized (GPIO %d and %d)", CONFIG_BUTTON0_GPIO, CONFIG_BUTTON1_GPIO);
    return ESP_OK;
}

gpio_num_t button_manager_get_button0_gpio(void)
{
    return (gpio_num_t)CONFIG_BUTTON0_GPIO;
}

gpio_num_t button_manager_get_button1_gpio(void)
{
    return (gpio_num_t)CONFIG_BUTTON1_GPIO;
}
