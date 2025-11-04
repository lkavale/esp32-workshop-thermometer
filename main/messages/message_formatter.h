#ifndef MESSAGE_FORMATTER_H
#define MESSAGE_FORMATTER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format sensor data message as JSON
 *
 * Creates a JSON message with sensor data in the following format:
 * {
 *   "id": "device_id",
 *   "sensor": "sensor_type",  // optional
 *   "data": {
 *     "temperature": {"value": "23.4", "unit": "C"},  // optional
 *     "humidity": {"value": "65.2", "unit": "%"},     // optional
 *     "voltage": {"value": "3.14", "unit": "V"}       // optional
 *   }
 * }
 *
 * Returns dynamically allocated string containing JSON message.
 *
 * @param id          NUL-terminated device ID string (required, e.g. "28AB3E6B00000098")
 * @param sensor      NUL-terminated sensor type string (optional, can be NULL, e.g. "DS18B20")
 * @param temperature Temperature value in Celsius (optional, can be NULL)
 * @param humidity    Humidity value in percent (optional, can be NULL)
 * @param voltage     Voltage value in Volts (optional, can be NULL)
 * @return Allocated JSON string on success (caller MUST free()),
 *         NULL on error (invalid arguments or allocation failure).
 *
 * @note At least one measurement (temperature, humidity, or voltage) should be provided.
 * @note The returned string must be freed by the caller using free().
 */
char *format_message(const char *id, const char *sensor, float *temperature, float *humidity, float *voltage);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_FORMATTER_H