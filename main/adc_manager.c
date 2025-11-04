#include "adc_manager.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <stdio.h>
#include <stdbool.h>

static const char *TAG = "adc_manager";

// ADC Hardware Configuration
#define ADC_UNIT        ADC_UNIT_1
#define ADC_CHANNEL     ADC_CHANNEL_0       // GPIO 0
#define ADC_ATTEN       ADC_ATTEN_DB_12     // 0-3.3V range
#define ADC_WIDTH       ADC_BITWIDTH_12     // 12-bit resolution (0-4095)

// Voltage divider configuration (1:1 ratio = multiply by 2)
#define VOLTAGE_DIVIDER_RATIO   2

// ADC reference voltage in millivolts
#define ADC_VREF_MV             3300

// ADC maximum raw value (12-bit)
#define ADC_MAX_RAW_VALUE       4095

// Module state
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool calibration_enabled = false;

/**
 * @brief Initialize ADC calibration scheme
 * 
 * Attempts to initialize ADC calibration using curve fitting or line fitting
 * depending on chip support. Falls back to raw values if calibration fails.
 * 
 * @return ESP_OK on successful calibration, error code otherwise (non-fatal)
 */
static esp_err_t init_adc_calibration(void)
{
    esp_err_t ret = ESP_FAIL;

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
        ESP_LOGI(TAG, "ADC calibration initialized (curve fitting)");
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Curve fitting calibration failed: %s", esp_err_to_name(ret));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        calibration_enabled = true;
        ESP_LOGI(TAG, "ADC calibration initialized (line fitting)");
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Line fitting calibration failed: %s", esp_err_to_name(ret));

#else
    ESP_LOGW(TAG, "ADC calibration not supported on this chip");
    ret = ESP_ERR_NOT_SUPPORTED;
#endif

    ESP_LOGW(TAG, "ADC will use uncalibrated raw-to-voltage conversion");
    calibration_enabled = false;
    return ret;
}

/**
 * @brief Convert raw ADC value to voltage in millivolts
 * 
 * Uses calibration if available, otherwise performs manual calculation.
 * 
 * @param adc_raw Raw ADC reading (0-4095 for 12-bit)
 * @param voltage_mv Pointer to store converted voltage in millivolts
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t convert_raw_to_voltage(int adc_raw, int *voltage_mv)
{
    if (!voltage_mv) {
        return ESP_ERR_INVALID_ARG;
    }

    if (calibration_enabled && adc_cali_handle) {
        // Use hardware calibration
        esp_err_t ret = adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, voltage_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Calibration conversion failed: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        // Manual conversion: voltage_mv = (adc_raw / ADC_MAX) * VREF
        *voltage_mv = (adc_raw * ADC_VREF_MV) / ADC_MAX_RAW_VALUE;
    }

    return ESP_OK;
}

/**
 * @brief Initialize ADC manager
 * 
 * Configures ADC hardware, initializes calibration if supported,
 * and prepares the module for voltage measurements.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC manager...");
    ESP_LOGI(TAG, "GPIO: %d, Unit: ADC%d, Channel: %d", CONFIG_ADC_GPIO, ADC_UNIT + 1, ADC_CHANNEL);
    
    // Prevent double initialization
    if (adc_handle != NULL) {
        ESP_LOGW(TAG, "ADC already initialized");
        return ESP_OK;
    }
    
    // Configure ADC oneshot unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
        return ret;
    }
    
    // Initialize calibration (non-fatal if it fails)
    init_adc_calibration();
    
    ESP_LOGI(TAG, "ADC initialized successfully");
    ESP_LOGI(TAG, "Voltage range: 0-%d.%dV (ADC) -> 0-%d.%dV (actual with 1:%d divider)", 
             ADC_VREF_MV / 1000, (ADC_VREF_MV % 1000) / 100,
             (ADC_VREF_MV * VOLTAGE_DIVIDER_RATIO) / 1000, 
             ((ADC_VREF_MV * VOLTAGE_DIVIDER_RATIO) % 1000) / 100,
             VOLTAGE_DIVIDER_RATIO);
    
    return ESP_OK;
}

/**
 * @brief Read voltage from ADC with voltage divider compensation
 * 
 * Reads the ADC, converts to voltage using calibration (if available),
 * and compensates for the external voltage divider.
 * 
 * @param voltage_mv Pointer to store the measured voltage in millivolts
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_manager_read_voltage(int *voltage_mv)
{
    // Validate input parameter
    if (!voltage_mv) {
        ESP_LOGE(TAG, "Voltage pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check initialization state
    if (!adc_handle) {
        ESP_LOGE(TAG, "ADC not initialized - call adc_manager_init() first");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Read raw ADC value
    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Convert to voltage in millivolts
    int voltage_uncorrected = 0;
    ret = convert_raw_to_voltage(adc_raw, &voltage_uncorrected);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Apply voltage divider correction
    *voltage_mv = voltage_uncorrected * VOLTAGE_DIVIDER_RATIO;
    
    ESP_LOGD(TAG, "ADC read: raw=%d, uncorrected=%dmV, corrected=%dmV", 
             adc_raw, voltage_uncorrected, *voltage_mv);
    
    return ESP_OK;
}