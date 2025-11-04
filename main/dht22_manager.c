#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "dht.h"
#include "dht22_manager.h"

static const char *TAG = "dht22_manager";

// Minimum time between readings (2 seconds for DHT22)
#define DHT22_MIN_INTERVAL_MS 2000

static TickType_t last_read_time = 0;

esp_err_t dht22_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing DHT22 sensor on GPIO %d", CONFIG_DHT22_GPIO);
    
    // Configure GPIO as INPUT with internal pull-up (matching Arduino pinMode(DHT_PIN, INPUT))
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_DHT22_GPIO),
        .mode = GPIO_MODE_INPUT,           // INPUT mode like Arduino
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Enable internal pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", CONFIG_DHT22_GPIO, esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for sensor to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "DHT22 ready on GPIO %d", CONFIG_DHT22_GPIO);
    return ESP_OK;
}

esp_err_t dht22_manager_read_data(float *temperature, float *humidity)
{
    if (temperature == NULL && humidity == NULL) {
        ESP_LOGE(TAG, "Both temperature and humidity pointers are NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check minimum interval between readings
    TickType_t current_time = xTaskGetTickCount();
    TickType_t time_since_last_read = (current_time - last_read_time) * portTICK_PERIOD_MS;
    
    if (last_read_time != 0 && time_since_last_read < DHT22_MIN_INTERVAL_MS) {
        ESP_LOGW(TAG, "Too soon to read again, wait %lu ms", 
                 DHT22_MIN_INTERVAL_MS - time_since_last_read);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Read data from DHT22 sensor
    esp_err_t status = dht_read_float_data(DHT_TYPE_AM2301, CONFIG_DHT22_GPIO, humidity, temperature);

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read DHT22 sensor data: %s", esp_err_to_name(status));
        return status;
    }
    
    // Update last read time
    last_read_time = current_time;
    
    // Log the readings
    ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", *temperature, *humidity);
    
    return ESP_OK;
}

gpio_num_t dht22_manager_get_gpio(void)
{
    return (gpio_num_t)CONFIG_DHT22_GPIO;
}
