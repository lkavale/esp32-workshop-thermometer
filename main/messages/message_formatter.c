#include "message_formatter.h"
#include "cJSON.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

#define VALUE_BUFFER_SIZE 16

/**
 * @brief Helper function to add a measurement value with unit to a data object
 * 
 * @param data_obj      Parent cJSON object to add the measurement to
 * @param field_name    Name of the field (e.g., "temperature", "humidity")
 * @param value         Pointer to the float value
 * @param unit          Unit string (e.g., "C", "%", "V")
 * @param precision     Number of decimal places (0-3)
 * @return true on success, false on allocation failure
 */
static bool add_measurement(cJSON *data_obj, const char *field_name, float *value, 
                           const char *unit, int precision)
{
    if (!value) {
        return true;  // NULL value is not an error, just skip
    }

    cJSON *measurement = cJSON_CreateObject();
    if (!measurement) {
        return false;
    }

    // Format value with specified precision
    char value_str[VALUE_BUFFER_SIZE];
    snprintf(value_str, sizeof(value_str), "%.*f", precision, *value);
    
    if (!cJSON_AddStringToObject(measurement, "value", value_str)) {
        cJSON_Delete(measurement);
        return false;
    }
    
    if (!cJSON_AddStringToObject(measurement, "unit", unit)) {
        cJSON_Delete(measurement);
        return false;
    }

    if (!cJSON_AddItemToObject(data_obj, field_name, measurement)) {
        cJSON_Delete(measurement);
        return false;
    }

    return true;
}

/**
 * @brief Format sensor data message as JSON
 *
 * Creates a JSON message with sensor data.
 * At least one measurement value should be provided for a meaningful message.
 *
 * @param id          NUL-terminated device ID string (required)
 * @param sensor      NUL-terminated sensor type string (optional, can be NULL)
 * @param temperature Temperature value in Celsius (optional, can be NULL)
 * @param humidity    Humidity value in percent (optional, can be NULL)
 * @param voltage     Voltage value in Volts (optional, can be NULL)
 * @return Allocated JSON string on success (caller MUST free()),
 *         NULL on error (invalid arguments or allocation failure).
 */
char *format_message(const char *id, const char *sensor, float *temperature, float *humidity, float *voltage)
{
    if (!id) {
        return NULL;
    }

    // Create root JSON object
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    // Add device ID (required field)
    if (!cJSON_AddStringToObject(root, "id", id)) {
        cJSON_Delete(root);
        return NULL;
    }
    
    // Add sensor type (optional field)
    if (sensor && !cJSON_AddStringToObject(root, "sensor", sensor)) {
        cJSON_Delete(root);
        return NULL;
    }
    
    // Create data object for measurements
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        cJSON_Delete(root);
        return NULL;
    }
    
    // Add measurements with specified precision:
    // - temperature: 1 decimal place (e.g., "23.4")
    // - humidity: 1 decimal place (e.g., "65.2")
    // - voltage: 2 decimal places (e.g., "3.14")
    if (!add_measurement(data, "temperature", temperature, "C", 1) ||
        !add_measurement(data, "humidity", humidity, "%", 1) ||
        !add_measurement(data, "voltage", voltage, "V", 2)) {
        cJSON_Delete(data);
        cJSON_Delete(root);
        return NULL;
    }

    // Attach data object to root
    if (!cJSON_AddItemToObject(root, "data", data)) {
        cJSON_Delete(data);
        cJSON_Delete(root);
        return NULL;
    }

    // Generate unformatted (compact) JSON string
    char *json_str = cJSON_PrintUnformatted(root);
    
    // Cleanup cJSON object tree
    cJSON_Delete(root);

    return json_str;
}