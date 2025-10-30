#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"

const static char *TAG = "ADC";

// ADC Configuration
#define ADC_UNIT ADC_UNIT_1
#define FLEX_1_ADC_CHANNEL ADC_CHANNEL_4
#define FLEX_2_ADC_CHANNEL ADC_CHANNEL_5
#define FLEX_3_ADC_CHANNEL ADC_CHANNEL_6
#define FLEX_4_ADC_CHANNEL ADC_CHANNEL_0
#define FLEX_5_ADC_CHANNEL ADC_CHANNEL_1
#define TOUCH_ADC_CHANNEL ADC_CHANNEL_2
#define ADC_ATTEN ADC_ATTEN_DB_12

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);

// Flex Sensor Calibration Values (You'll need to adjust these)
// #define FLEX_STRAIGHT_ADC_VALUE [5] = {715, 715, 715, 715, 715} // ADC value when sensor is straight
// #define FLEX_BENT_ADC_VALUE [5] = {225, 225, 225, 225, 225}     // ADC value when sensor is fully bent
float flex_1_straight_adc_value = 715; // ADC value when sensor is straight
float flex_1_bent_adc_value = 225;     // ADC value when sensor is fully bent
float flex_1_voltage_straight = 500;   // mV when straight
float flex_1_voltage_bent = 2500;      // mV when bent
float flex_straight_angle = 0;         // 0 degrees when straight
float flex_bent_angle = 90;            // 90 degrees when fully bent

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t flex_1_cali_handle;
static adc_cali_handle_t touch_cali_handle;
static bool calibration_enabled = false;

// Function to initialize ADC
static void flex_sensor_init(void)
{
    // ADC Unit Init
    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_config, &adc_handle));

    // ADC Channel Config
    adc_oneshot_chan_cfg_t adc_chan_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, FLEX_1_ADC_CHANNEL, &adc_chan_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, FLEX_2_ADC_CHANNEL, &adc_chan_config));

    // ADC Calibration Init for both channels
    calibration_enabled = adc_calibration_init(ADC_UNIT, FLEX_1_ADC_CHANNEL, ADC_ATTEN, &flex_1_cali_handle);
    bool calibration_enabled_2 = adc_calibration_init(ADC_UNIT, FLEX_2_ADC_CHANNEL, ADC_ATTEN, &touch_cali_handle);

    // Both need to be enabled for full calibration
    calibration_enabled = calibration_enabled && calibration_enabled_2;
}

// Function to read raw ADC value
static int read_adc_raw_channel(adc_channel_t channel)
{
    int raw_value;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &raw_value));
    return raw_value;
}

// Generic function to read voltage from any channel
static int read_adc_voltage_channel(adc_cali_handle_t cali_handle, int raw_value)
{
    int voltage = 0;
    if (calibration_enabled && cali_handle != NULL)
    {
        esp_err_t ret = adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Voltage conversion failed: %s", esp_err_to_name(ret));
        }
    }
    return voltage;
}

// Function to calculate bend angle from normalized value (0.0 - 1.0)
static float calculate_bend_angle_normalized(float normalized_value)
{
    // normalized_value should be between 0.0 (straight) and 1.0 (fully bent)
    float angle = normalized_value * (flex_bent_angle - flex_straight_angle);

    // Constrain the angle
    if (angle < flex_straight_angle)
        angle = flex_straight_angle;
    if (angle > flex_bent_angle)
        angle = flex_bent_angle;

    return angle;
}

// Function to calculate bend angle from voltage
static float calculate_bend_angle_from_voltage(int voltage_mv, float voltage_straight, float voltage_bent)
{
    float normalized = (float)(voltage_mv - voltage_straight) /
                       (voltage_bent - voltage_straight);
    return calculate_bend_angle_normalized(normalized);
}

// Function to calculate bend angle from raw ADC
static float calculate_bend_angle_from_raw(int raw_adc, float adc_straight, float adc_bent)
{
    float normalized = (float)(raw_adc - adc_straight) /
                       (adc_bent - adc_straight);
    return calculate_bend_angle_normalized(normalized);
}

// Function to calibrate the flex sensor
static void flex_sensor_calibration(void)
{
    ESP_LOGI(TAG, "Starting Flex Sensor Calibration");

    ESP_LOGI(TAG, "Place sensor STRAIGHT");
    vTaskDelay(pdMS_TO_TICKS(3000));
    int straight_raw = read_adc_raw_channel(ADC_CHANNEL_4);
    int straight_voltage = read_adc_voltage_channel(flex_1_cali_handle, straight_raw);

    ESP_LOGI(TAG, "BEND sensor fully");
    vTaskDelay(pdMS_TO_TICKS(3000));
    int bent_raw = read_adc_raw_channel(ADC_CHANNEL_4);
    int bent_voltage = read_adc_voltage_channel(flex_1_cali_handle, bent_raw);

    flex_1_straight_adc_value = (float)straight_raw;
    flex_1_bent_adc_value = (float)bent_raw;
    flex_1_voltage_straight = (float)straight_voltage;
    flex_1_voltage_bent = (float)bent_voltage;

    ESP_LOGI(TAG, "Calibration Results:");
    ESP_LOGI(TAG, "Straight - Raw: %d, Voltage: %d mV", straight_raw, straight_voltage);
    ESP_LOGI(TAG, "Bent    - Raw: %d, Voltage: %d mV", bent_raw, bent_voltage);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Flex Sensor...");
    flex_sensor_init();

    // Calibrate flex sensor readings
    flex_sensor_calibration();

    ESP_LOGI(TAG, "Starting flex sensor readings...");

    while (1)
    {
        // Read raw ADC value
        int flex_raw_value = read_adc_raw_channel(ADC_CHANNEL_4);
        int touch_raw_value = read_adc_raw_channel(ADC_CHANNEL_5);
        float bend_angle;

        if (calibration_enabled)
        {
            int flex_voltage = read_adc_voltage_channel(flex_1_cali_handle, flex_raw_value);
            int touch_voltage = read_adc_voltage_channel(touch_cali_handle, touch_raw_value);
            bend_angle = calculate_bend_angle_from_voltage(flex_voltage, flex_1_voltage_straight, flex_1_voltage_bent);
            ESP_LOGI(TAG, "FLEX: Raw: %d, Voltage: %d mV, Angle: %.1f°",
                     flex_raw_value, flex_voltage, bend_angle);
            ESP_LOGI(TAG, "TOUCH: Raw: %d, Voltage: %d mV",
                     touch_raw_value, touch_voltage);
            // ESP_LOGI(TAG, "Raw: %d, Voltage: %d mV",
            //          raw_value, voltage);
        }
        else
        {
            // Fallback to raw ADC (less accurate)
            bend_angle = calculate_bend_angle_from_raw(flex_raw_value, flex_1_straight_adc_value, flex_1_bent_adc_value);
            ESP_LOGI(TAG, "FLEX: Raw: %d, Angle: %.1f°", flex_raw_value, bend_angle);
            ESP_LOGI(TAG, "TOUCH: Raw: %d", touch_raw_value);
            // ESP_LOGI(TAG, "Raw: %d", raw_value);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Read every 500ms
    }
}