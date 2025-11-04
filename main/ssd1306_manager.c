#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "ssd1306.h"
#include "ssd1306_manager.h"
#include "esp_wifi.h"
#include "esp_netif.h"

static const char *TAG = "ssd1306_manager";

#define I2C_MASTER_SCL_IO           9       /*!< GPIO number for I2C master clock */
#define I2C_MASTER_SDA_IO           8       /*!< GPIO number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          400000  /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000
#define ACK_CHECK_EN                0x1     /*!< I2C master will check ack from slave*/
#define ACK_VAL                     0x0     /*!< I2C ack value */
#define NACK_VAL                    0x1     /*!< I2C nack value */

static ssd1306_handle_t ssd1306_dev = NULL;
static bool initialized = false;
static screen_id_t current_screen = SCREEN_TEMPERATURES;

// Cache for sensor data
static float cached_ds_temp = 0.0f;
static float cached_dht_temp = 0.0f;
static float cached_dht_humidity = 0.0f;
static int cached_voltage = 0.0f;

esp_err_t ssd1306_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing SSD1306 OLED display");
    ESP_LOGI(TAG, "I2C pins: SDA=GPIO%d, SCL=GPIO%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    
    // Configure I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Continuing without display...");
        return ret;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 
                            I2C_MASTER_RX_BUF_DISABLE, 
                            I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Continuing without display...");
        return ret;
    }
    
    ESP_LOGI(TAG, "I2C driver installed successfully");
    
    // Longer delay for I2C to stabilize
    vTaskDelay(pdMS_TO_TICKS(500));

    ssd1306_dev = ssd1306_create(I2C_MASTER_NUM, 0x3C);
    if (ssd1306_dev == NULL) {
        ESP_LOGW(TAG, "Failed to create device handle for address 0x%02X", 0x3C);
    }
    
    ret = ssd1306_init(ssd1306_dev);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SSD1306 found at address 0x%02X!", 0x3C);
    } else {
        ESP_LOGW(TAG, "No response from address 0x%02X", 0x3C);
        ssd1306_delete(ssd1306_dev);
        ssd1306_dev = NULL;
    }
    
    // Clear the screen
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    ssd1306_refresh_gram(ssd1306_dev);
    
    initialized = true;
    ESP_LOGI(TAG, "SSD1306 initialized successfully at 0x%02X (SDA=%d, SCL=%d)",
             0x3C, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    
    return ESP_OK;
}

void set_temp_values(float ds_temp, float dht_temp, float dht_humidity)
{
    cached_ds_temp = ds_temp;
    cached_dht_temp = dht_temp;
    cached_dht_humidity = dht_humidity;
}

void set_voltage_value(int voltage)
{
    cached_voltage = voltage;
}

esp_err_t ssd1306_manager_clear(void)
{
    if (!initialized || ssd1306_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    return ssd1306_refresh_gram(ssd1306_dev);
}

// ============= Multi-Screen Functions =============

static void draw_screen_temperatures(void)
{
    char line[32];
    
    // Title
    ssd1306_draw_string(ssd1306_dev, 0, 0, (uint8_t *)"== SENSORS ==", 12, 1);
    
    // DS18B20
    snprintf(line, sizeof(line), "DS18B20: %.1f *C", cached_ds_temp);
    ssd1306_draw_string(ssd1306_dev, 0, 16, (uint8_t *)line, 12, 1);
    
    // DHT22 Temp
    snprintf(line, sizeof(line), "T DHT22: %.1f *C", cached_dht_temp);
    ssd1306_draw_string(ssd1306_dev, 0, 28, (uint8_t *)line, 12, 1);
    
    // DHT22 Humidity
    snprintf(line, sizeof(line), "H DHT22: %.1f %%", cached_dht_humidity);
    ssd1306_draw_string(ssd1306_dev, 0, 40, (uint8_t *)line, 12, 1);
}

static void draw_screen_adc(void)
{
    char line[32];
    
    // Title
    ssd1306_draw_string(ssd1306_dev, 0, 0, (uint8_t *)"== POWER ==", 12, 1);
    
    // Voltage in V
    snprintf(line, sizeof(line), "Voltage: %.2f V", cached_voltage / 1000.0f);
    ssd1306_draw_string(ssd1306_dev, 0, 16, (uint8_t *)line, 12, 1);
}

static void draw_screen_system(void)
{
    char line[32];
    
    // Title
    ssd1306_draw_string(ssd1306_dev, 0, 0, (uint8_t *)"== MEMORY ==", 12, 1);
    
    // Free heap
    uint32_t free_heap = esp_get_free_heap_size();
    snprintf(line, sizeof(line), "Heap: %lu kB", free_heap / 1024);
    ssd1306_draw_string(ssd1306_dev, 0, 16, (uint8_t *)line, 12, 1);
    
    // Minimum free heap
    uint32_t min_heap = esp_get_minimum_free_heap_size();
    snprintf(line, sizeof(line), "Min: %lu kB", min_heap / 1024);
    ssd1306_draw_string(ssd1306_dev, 0, 28, (uint8_t *)line, 12, 1);
    
    // Uptime
    uint32_t uptime_sec = esp_log_timestamp() / 1000;
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;
    snprintf(line, sizeof(line), "Up: %luh %lum", hours, minutes);
    ssd1306_draw_string(ssd1306_dev, 0, 40, (uint8_t *)line, 12, 1);
}

static void draw_screen_network(void)
{
    char line[32];
    
    // Title
    ssd1306_draw_string(ssd1306_dev, 0, 0, (uint8_t *)"== NETWORK ==", 12, 1);
    
    // WiFi status
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret == ESP_OK) {
        // Connected
        snprintf(line, sizeof(line), "WiFi: Connected");
        ssd1306_draw_string(ssd1306_dev, 0, 16, (uint8_t *)line, 12, 1);
        
        // SSID (truncate if too long)
        char ssid[17];
        strncpy(ssid, (char *)ap_info.ssid, 16);
        ssid[16] = '\0';
        snprintf(line, sizeof(line), "SSID: %s", ssid);
        ssd1306_draw_string(ssd1306_dev, 0, 28, (uint8_t *)line, 12, 1);
        
        // RSSI
        snprintf(line, sizeof(line), "RSSI: %d dBm", ap_info.rssi);
        ssd1306_draw_string(ssd1306_dev, 0, 40, (uint8_t *)line, 12, 1);
        
        // IP address
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                snprintf(line, sizeof(line), "IP: " IPSTR, IP2STR(&ip_info.ip));
                ssd1306_draw_string(ssd1306_dev, 0, 52, (uint8_t *)line, 12, 1);
            }
        }
    } else {
        // Not connected
        snprintf(line, sizeof(line), "WiFi: Disconnected");
        ssd1306_draw_string(ssd1306_dev, 0, 16, (uint8_t *)line, 12, 1);
        
        snprintf(line, sizeof(line), "Connecting...");
        ssd1306_draw_string(ssd1306_dev, 0, 28, (uint8_t *)line, 12, 1);
    }
}

esp_err_t ssd1306_manager_set_screen(screen_id_t screen)
{
    if (screen >= SCREEN_COUNT) {
        ESP_LOGE(TAG, "Invalid screen ID: %d", screen);
        return ESP_ERR_INVALID_ARG;
    }
    
    current_screen = screen;
    ESP_LOGI(TAG, "Switched to screen %d", screen);
    
    return ESP_OK;
}

screen_id_t ssd1306_manager_get_screen(void)
{
    return current_screen;
}

esp_err_t ssd1306_manager_next_screen(void)
{
    current_screen = (current_screen + 1) % SCREEN_COUNT;
    ESP_LOGI(TAG, "Next screen: %d", current_screen);
    return ESP_OK;
}

esp_err_t ssd1306_manager_prev_screen(void)
{
    current_screen = (current_screen - 1 + SCREEN_COUNT) % SCREEN_COUNT;
    ESP_LOGI(TAG, "Previous screen: %d", current_screen);
    return ESP_OK;
}

esp_err_t ssd1306_manager_update_display()
{
    if (!initialized || ssd1306_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear screen
    ssd1306_clear_screen(ssd1306_dev, 0x00);
    
    // Draw current screen
    switch (current_screen) {
        case SCREEN_TEMPERATURES:
            draw_screen_temperatures();
            break;
            
        case SCREEN_ADC:
            draw_screen_adc();
            break;
            
        case SCREEN_SYSTEM:
            draw_screen_system();
            break;
            
        case SCREEN_NETWORK:
            draw_screen_network();
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown screen: %d", current_screen);
            break;
    }
    
    // Refresh display
    esp_err_t ret = ssd1306_refresh_gram(ssd1306_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh display: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}
