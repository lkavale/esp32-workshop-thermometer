#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "ds18b20_manager.h"

static const char *TAG = "ds18b20_manager";

#define ONEWIRE_MAX_DS18B20 2

static onewire_bus_handle_t bus = NULL;
static int ds18b20_device_num = 0;
static ds18b20_device_handle_t ds18b20s[ONEWIRE_MAX_DS18B20];

esp_err_t ds18b20_manager_init(void)
{
    // install 1-wire bus
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = CONFIG_ONEWIRE_BUS_GPIO,
        .flags = {
            .en_pull_up = false, // enable the internal pull-up resistor in case the external device didn't have one
        }
    };

    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10, // 1byte ROM command + 8byte ROM number + 1byte device command
    };
    
    esp_err_t status = onewire_new_bus_rmt(&bus_config, &rmt_config, &bus);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create 1-wire bus, with error %s", esp_err_to_name(status));
        return status;
    }

    status = ds18b20_manager_search_devices();
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to search for DS18B20 devices, with error %s", esp_err_to_name(status));
        return status;
    }

    return ESP_OK;
}

esp_err_t ds18b20_manager_search_devices(void)
{
    if (bus == NULL) {
        ESP_LOGE(TAG, "1-wire bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Reset device count
    ds18b20_device_num = 0;
    
    // create 1-wire device iterator, which is used for device search
    onewire_device_iter_handle_t iterator = NULL;
    esp_err_t status = onewire_new_device_iter(bus, &iterator);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create device iterator, with error %s", esp_err_to_name(status));
        return status;
    }
    
    ESP_LOGD(TAG, "Device iterator created, start searching devices");

    onewire_device_t next_onewire_device;
    do {
        status = onewire_device_iter_get_next(iterator, &next_onewire_device);
        if (status == ESP_OK) // found a new device
        {
            ds18b20_config_t ds18b20_cfg = {};
            onewire_device_address_t address;
            
            // check if the device is a DS18B20, if so, return the ds18b20 handle
            status = ds18b20_new_device_from_enumeration(&next_onewire_device, &ds18b20_cfg, &ds18b20s[ds18b20_device_num]);
            if (status != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create DS18B20 device from enumeration, with error %s", esp_err_to_name(status));
                break;
            }

            status = ds18b20_get_device_address(ds18b20s[ds18b20_device_num], &address);
            if (status != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get DS18B20 device address, with error %s", esp_err_to_name(status));
                break;
            }

            ds18b20_device_num++;
            ESP_LOGI(TAG, "Found a DS18B20[%d] with device address %016llX", ds18b20_device_num, address);
                
            if (ds18b20_device_num >= ONEWIRE_MAX_DS18B20) {
                ESP_LOGW(TAG, "Maximum number of DS18B20 devices reached");
                break;
            } 
        }

    } while (status != ESP_ERR_NOT_FOUND);

    status = onewire_del_device_iter(iterator);
    if (status != ESP_OK) {
        ESP_LOGW(TAG, "Failed to delete device iterator, with error %s", esp_err_to_name(status));
    }
    
    ESP_LOGI(TAG, "Searching done, %d DS18B20 device(s) found", ds18b20_device_num);
    
    return ESP_OK;
}

esp_err_t ds18b20_manager_read_temperature(int device_index, float *temperature)
{
    if (device_index >= ds18b20_device_num || device_index < 0) {
        ESP_LOGE(TAG, "Invalid device index %d", device_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (temperature == NULL) {
        ESP_LOGE(TAG, "Temperature pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t status = ds18b20_trigger_temperature_conversion(ds18b20s[device_index]);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to trigger temperature conversion on device index %d, with error: %s", device_index, esp_err_to_name(status));
        return status;
    }
    
    // Wait for conversion to complete
    vTaskDelay(pdMS_TO_TICKS(1000)); // DS18B20 conversion time is up to 750ms
    
    status = ds18b20_get_temperature(ds18b20s[device_index], temperature);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get temperature on device index %d, with error: %s", device_index, esp_err_to_name(status));
        return status;
    }
    
    return ESP_OK;
}

static void rom64_to_hex(onewire_device_address_t addr, char *out)
{
    // Print as 16 hex digits, uppercase
    snprintf(out, 17, "%016" PRIX64, (uint64_t)addr);
}

int ds18b20_manager_get_device_count(void)
{
    return ds18b20_device_num;
}

esp_err_t ds18b20_manager_get_device_address(int device_index, const char *rom_code)
{
    if (device_index >= ds18b20_device_num || device_index < 0) {
        ESP_LOGE(TAG, "Invalid device index: %d", device_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    onewire_device_address_t address;
    esp_err_t status = ds18b20_get_device_address(ds18b20s[device_index], &address);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device address for device index %d, with error: %s", device_index, esp_err_to_name(status));
        return status;
    }

    rom64_to_hex(address, rom_code);

    return ESP_OK;
}

esp_err_t ds18b20_manager_deinit(void)
{
    esp_err_t ret = ESP_OK;
    
    // Delete all DS18B20 devices
    for (int i = 0; i < ds18b20_device_num; i++) {
        if (ds18b20s[i] != NULL) {
            esp_err_t del_ret = ds18b20_del_device(ds18b20s[i]);
            if (del_ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to delete DS18B20 device %d: %s", i, esp_err_to_name(del_ret));
                ret = del_ret;
            }
            ds18b20s[i] = NULL;
        }
    }
    ds18b20_device_num = 0;
    
    // Delete the 1-wire bus
    if (bus != NULL) {
        esp_err_t del_ret = onewire_bus_del(bus);
        if (del_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to delete 1-wire bus: %s", esp_err_to_name(del_ret));
            ret = del_ret;
        }
        bus = NULL;
    }
    
    return ret;
}