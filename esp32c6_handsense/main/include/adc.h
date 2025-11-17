#pragma once
#include <stdbool.h>
#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief ADC sensor configuration structure
     */
    typedef struct
    {
        adc_channel_t channel;
        const char *name;
        bool is_touch_sensor; // true for touch sensor, false for flex sensor
    } adc_sensor_config_t;

    /**
     * @brief Initialize the adc sensor component
     *
     * @param configs Array of sensor configurations
     * @param count Number of sensors to configure
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t adc_sensor_init(const adc_sensor_config_t *configs, size_t count);

    /**
     * @brief Calibrate all flex sensor (not for touch sensors)
     */
    void calibrate_all_flex_sensors(void);

    /**
     * @brief Get raw ADC value for a specific channel
     *
     * @param channel ADC channel to read
     * @return int Raw ADC value, or -1 if error
     */
    int adc_sensor_read_raw(adc_channel_t channel);

    /**
     * @brief Get voltage for a specific channel
     *
     * @param channel ADC channel to read
     * @return int Voltage in mV, or -1 if calibration not available
     */
    int adc_sensor_read_voltage(adc_channel_t channel);

    /**
     * @brief Get bend angle for a specific flex sensor
     *
     * @param channel ADC channel of the flex sensor
     * @return float Bend angle in degrees
     */
    float flex_sensor_get_angle(adc_channel_t channel);

    /**
     * @brief Check if a channel is configured as a touch sensor
     *
     * @param channel ADC channel to check
     * @return true if touch sensor, false if flex sensor
     */
    bool adc_sensor_is_touch_sensor(adc_channel_t channel);

    /**
     * @brief Set custom calibration values for a flex sensor
     *
     * @param channel ADC channel
     * @param straight_raw Raw value when straight
     * @param bent_raw Raw value when bent
     * @param straight_voltage Voltage when straight (mV)
     * @param bent_voltage Voltage when bent (mV)
     */
    void flex_sensor_set_calibration(adc_channel_t channel,
                                     int straight_raw, int bent_raw,
                                     int straight_voltage, int bent_voltage);

    /**
     * @brief Get sensor name
     *
     * @param channel ADC channel
     * @return const char* Sensor name, or "unknown" if not configured
     */
    const char *adc_sensor_get_name(adc_channel_t channel);

    /**
     * @brief Deinitialize the adc sensor component
     */
    void adc_sensor_deinit(void);

#ifdef __cplusplus
}
#endif