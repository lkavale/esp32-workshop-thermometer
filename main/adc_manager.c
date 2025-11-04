#include <stdio.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "adc_manager.h"

static const char *TAG = "adc_manager";

// ADC Configuration for ESP32-C3
// GPIO 0 = ADC1_CH0

#define ADC_UNIT        ADC_UNIT_1
#define ADC_CHANNEL     ADC_CHANNEL_0
#define ADC_ATTEN       ADC_ATTEN_DB_12     // 0-3.3V range
#define ADC_WIDTH       ADC_BITWIDTH_12     // 12-bit resolution (0-4095)

// Voltage divider ratio (1:1 means multiply by 2)
#define VOLTAGE_DIVIDER_RATIO 2

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool calibration_enabled = false;

esp_err_t adc_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC on GPIO %d", CONFIG_ADC_GPIO);
    
    // Configure ADC oneshot unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize ADC calibration
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .chan = ADC_CHANNEL,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        calibration_enabled = true;
        ESP_LOGI(TAG, "ADC calibration enabled (curve fitting)");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed, using raw values: %s", esp_err_to_name(ret));
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        calibration_enabled = true;
        ESP_LOGI(TAG, "ADC calibration enabled (line fitting)");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed, using raw values: %s", esp_err_to_name(ret));
    }
#else
    ESP_LOGW(TAG, "ADC calibration not supported on this chip");
#endif
    
    ESP_LOGI(TAG, "ADC initialized successfully on GPIO %d (ADC1_CH%d)", CONFIG_ADC_GPIO, ADC_CHANNEL);
    ESP_LOGI(TAG, "Voltage range: 0-3.3V (with 1:1 divider = 0-6.6V actual)");
    
    return ESP_OK;
}

esp_err_t adc_manager_read_voltage(int *voltage_mv)
{
    if (voltage_mv == NULL) {
        ESP_LOGE(TAG, "Voltage pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (adc_handle == NULL) {
        ESP_LOGE(TAG, "ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Read raw ADC value
    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Convert to voltage
    int voltage = 0;
    if (calibration_enabled && adc_cali_handle != NULL) {
        // Use calibration
        ret = adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to convert to voltage: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        // Manual conversion (approximate for 12-bit ADC with 3.3V reference)
        // voltage_mv = (adc_raw / 4095) * 3300
        voltage = (adc_raw * 3300) / 4095;
    }
    
    // Compensate for 1:1 voltage divider (multiply by 2)
    *voltage_mv = voltage * VOLTAGE_DIVIDER_RATIO;
    
    ESP_LOGD(TAG, "ADC raw: %d, voltage: %d mV (actual: %d mV)", adc_raw, voltage, *voltage_mv);
    
    return ESP_OK;
}

esp_err_t adc_manager_read_raw(int *raw_value)
{
    if (raw_value == NULL) {
        ESP_LOGE(TAG, "Raw value pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (adc_handle == NULL) {
        ESP_LOGE(TAG, "ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL, raw_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

int adc_manager_get_gpio(void)
{
    return CONFIG_ADC_GPIO;
}
