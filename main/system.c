#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_heap_caps.h"
#include "system.h"


#ifdef CONFIG_SPIRAM_SUPPORT
#include "esp_psram.h"
#endif

void check_psram(void)
{
    #ifdef CONFIG_SPIRAM_SUPPORT
        if (esp_psram_is_initialized()) {
            ESP_LOGI("system.psram", "PSRAM initialized, size: %zu bytes", esp_psram_get_size());
        } else {
            ESP_LOGW("system.psram", "PSRAM support enabled but not initialized");
        }
    #else
        ESP_LOGI("system.psram", "PSRAM support not enabled");
    #endif
}

void check_wifi_rssi()
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ESP_LOGI("wifi", "RSSI: %d dBm", ap_info.rssi);
    } else {
        ESP_LOGW("wifi", "No network is connected!");
    }
}

void check_free_ram()
{
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    ESP_LOGI("heap", "Free DRAM: %d kB", free_heap / 1024);
    ESP_LOGI("heap", "Free DRAM block: %d kB", largest_block / 1024);

#if CONFIG_SPIRAM
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t largest_psram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    ESP_LOGI("heap", "Free PSRAM: %d kB", free_psram / 1024);
    ESP_LOGI("heap", "Free PSRAM block: %d kB", largest_psram / 1024);
#endif
}

void print_ip_info(void)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif;

    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI("net", "IP: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI("net", "Gateway: " IPSTR, IP2STR(&ip_info.gw));
        ESP_LOGI("net", "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
    } else {
        ESP_LOGW("net", "IP not available");
    }
}