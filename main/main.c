#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "ds18b20_manager.h"
#include "dht22_manager.h"
#include "ssd1306_manager.h"
#include "button_manager.h"
#include "adc_manager.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "system.h"
#include "messages/message_formatter.h"
#include <stdlib.h>

static const char *TAG = "example";

/**
 * @brief Callback function for button events
 */
static void button_event_handler(gpio_num_t gpio, button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_PRESSED:
            ESP_LOGI(TAG, ">>> Button on GPIO %d PRESSED", gpio);
            
            if (gpio == button_manager_get_button0_gpio()) {
                // Button 0 (GPIO 1) - Previous screen
                ESP_LOGI(TAG, "Button 0: Previous screen");
                ssd1306_manager_prev_screen();
            } else if (gpio == button_manager_get_button1_gpio()) {
                // Button 1 (GPIO 2) - Next screen
                ESP_LOGI(TAG, "Button 1: Next screen");
                ssd1306_manager_next_screen();
            }

            ssd1306_manager_update_display();
            break;
            
        case BUTTON_EVENT_RELEASED:
            ESP_LOGD(TAG, ">>> Button on GPIO %d RELEASED", gpio);
            break;
            
        case BUTTON_EVENT_LONG_PRESS:
            ESP_LOGI(TAG, ">>> Button on GPIO %d LONG PRESS (2s)", gpio);
            
            if (gpio == button_manager_get_button0_gpio()) {
                // Button 0 long press - Clear display
                ESP_LOGI(TAG, "Button 0 long press: Clear display");
                ssd1306_manager_clear();
                vTaskDelay(pdMS_TO_TICKS(1000));
            } else if (gpio == button_manager_get_button1_gpio()) {
                // Button 1 long press - Restart ESP32
                ESP_LOGW(TAG, "Button 1 long press: Restarting in 2 seconds...");
                ssd1306_manager_clear();
                vTaskDelay(pdMS_TO_TICKS(2000));
                esp_restart();
            }
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    check_psram();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init();
    mqtt_init(NULL, 0);

    ds18b20_manager_init();
    dht22_manager_init();
    adc_manager_init();
    button_manager_init(button_event_handler);
    ssd1306_manager_init();
    
    // Give sensors time to stabilize
    ESP_LOGI(TAG, "Waiting for sensors to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        // Read DS18B20 temperature sensor
        float ds_temperature = 0.0f;
        char rom_code_s[17];

        ds18b20_manager_get_device_address(0, rom_code_s);
        ds18b20_manager_read_temperature(0, &ds_temperature);
        /* format_temperature_message returns an allocated string; use it and free it */
        char *json_msg = format_message(rom_code_s, "DS18B20", &ds_temperature, NULL, NULL);
        if (json_msg) {
            ESP_LOGI(TAG, "DS18B20 message: %s", json_msg);
            mqtt_manager_publish("test/sensors/temperature", json_msg, 1, false);
            free(json_msg);
        }

        // Read DHT22 temperature and humidity sensor
        float dht_temperature = 0.0f;
        float dht_humidity = 0.0f;
        
        esp_err_t dht_status = dht22_manager_read_data(&dht_temperature, &dht_humidity);
        if (dht_status == ESP_OK) {
            ESP_LOGI(TAG, "DHT22 - Temperature: %.1f°C, Humidity: %.1f%%", dht_temperature, dht_humidity);
            
            // Display temperatures on SSD1306 OLED (only if DHT22 is working)
            set_temp_values(ds_temperature, dht_temperature, dht_humidity);
        } else {
            ESP_LOGW(TAG, "Failed to read DHT22 sensor, skipping display update");
        }
        
        /* format_temperature_message returns an allocated string; use it and free it */
        char *json_msg2 = format_message("T01", "DHT22", &dht_temperature, &dht_humidity, NULL);
        if (json_msg2) {
            ESP_LOGI(TAG, "DHT22 message: %s", json_msg2);
            mqtt_manager_publish("test/sensors/temperature", json_msg2, 1, false);
            free(json_msg2);
        }

        // Read ADC voltage (with 1:1 voltage divider)
        int voltage_mv = 0;
        float voltage_v = 0.0f;
        esp_err_t adc_status = adc_manager_read_voltage(&voltage_mv);
        if (adc_status == ESP_OK) {
            set_voltage_value(voltage_mv);
            ESP_LOGI(TAG, "ADC - Voltage: %d mV (%.2f V)", voltage_mv, voltage_mv / 1000.0f);
        } else {
            ESP_LOGW(TAG, "Failed to read ADC voltage");
        }

        voltage_v = voltage_mv / 1000.0f;
    
        /* format_temperature_message returns an allocated string; use it and free it */
        char *json_msg3 = format_message("T01", "V", NULL, NULL, &voltage_v);
        if (json_msg3) {
            ESP_LOGI(TAG, "ADC message: %s", json_msg3);
            mqtt_manager_publish("test/sensors/voltage", json_msg3, 1, false);
            free(json_msg3);
        }

        // Update display with all data (will show current screen)
        ssd1306_manager_update_display();

        vTaskDelay(10000/portTICK_PERIOD_MS);
    }
}
