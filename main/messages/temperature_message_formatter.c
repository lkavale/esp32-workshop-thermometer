#include "temperature_message_formatter.h"
#include "cJSON.h"
#include <stddef.h>

char *format_temperature_message(const char *id, const char *sensor, float *temperature, float *humidity, float *voltage)
{
    if (!id) {
        return NULL;
    }

    // Create JSON object
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    // Add root fields
    cJSON_AddStringToObject(root, "id", id);
    
    if(sensor)
        cJSON_AddStringToObject(root, "sensor", sensor);
    
    // Create data object
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        cJSON_Delete(root);
        return NULL;
    }
    
    if(temperature)
    {
        cJSON *temperature_json = cJSON_CreateObject();

        cJSON_AddNumberToObject(temperature_json, "value", *temperature);
        cJSON_AddStringToObject(temperature_json, "unit", "C");

        cJSON_AddItemToObject(data, "temperature", temperature_json);
    }

    if(humidity)
    {
        cJSON *humidity_json = cJSON_CreateObject();

        cJSON_AddNumberToObject(humidity_json, "value", *humidity);
        cJSON_AddStringToObject(humidity_json, "unit", "%");

        cJSON_AddItemToObject(data, "humidity", humidity_json);
    }

    if(voltage)
    {
        cJSON *voltage_json = cJSON_CreateObject();

        cJSON_AddNumberToObject(voltage_json, "value", *voltage);
        cJSON_AddStringToObject(voltage_json, "unit", "V");

        cJSON_AddItemToObject(data, "voltage", voltage_json);
    }

    cJSON_AddItemToObject(root, "data", data);

    // Print formatted JSON
    char *json_str = cJSON_Print(root);
    
    // Cleanup cJSON object
    cJSON_Delete(root);

    return json_str;
}