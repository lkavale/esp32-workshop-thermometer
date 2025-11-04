#ifndef TEMPERATURE_MESSAGE_H
#define TEMPERATURE_MESSAGE_H

/**
 * @brief Format temperature message as JSON
 *
 * Returns dynamically allocated string containing JSON message.
 *
 * @param hex_id      NUL-terminated device ID as hex string (e.g. "28AB3E6B00000098")
 * @param sensor      NUL-terminated sensor type string (e.g. "DS18B20")
 * @param temperature Temperature value to include
 * @param humidity    Humidity value to include
 * @param voltage     Voltage value to include
 * @return Allocated JSON string on success (caller must free()),
 *         NULL on error (invalid arguments or allocation failure).
 */
char *format_temperature_message(const char *id, const char *sensor, float *temperature, float *humidity, float *voltage);


#endif // TEMPERATURE_MESSAGE_H