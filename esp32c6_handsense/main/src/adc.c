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
#include "adc.h"
#include "oled.h"

static const char *TAG = "ADC";

// Default configuration
#define DEFAULT_ADC_UNIT ADC_UNIT_1
#define DEFAULT_ADC_ATTEN ADC_ATTEN_DB_12
#define DEFAULT_FLEX_STRAIGHT_ANGLE 0.0f
#define DEFAULT_FLEX_BENT_ANGLE 90.0f

#define ADC_UNIT ADC_UNIT_1
#define ADC_ATTEN ADC_ATTEN_DB_12

typedef struct
{
    const char *name;
    bool is_touch_sensor;
    bool configured;
    float raw_straight;
    float raw_bent;
    float voltage_straight;
    float voltage_bent;
    adc_cali_handle_t cali_handle;
    bool calibrated;
} adc_sensor_t;

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_sensor_t sensors[SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)] = {0};
static bool calibration_enabled = false;
static bool component_initialized = false;

// Forward declarations
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);
static float calculate_bend_angle_normalized(float normalized_value);
static float calculate_bend_angle_from_voltage(int voltage_mv, float voltage_straight, float voltage_bent);
static float calculate_bend_angle_from_raw(int raw_adc, float raw_straight, float raw_bent);

esp_err_t adc_sensor_init(const adc_sensor_config_t *configs, size_t count)
{
    if (component_initialized)
    {
        ESP_LOGW(TAG, "Component already initialized");
        return ESP_OK;
    }

    // Initialize ADC unit
    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = DEFAULT_ADC_UNIT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&adc_init_config, &adc_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize default calibration values for all channels
    for (int i = 0; i < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1); i++)
    {
        sensors[i].configured = false;
        sensors[i].calibrated = false;
        sensors[i].cali_handle = NULL;
        sensors[i].name = "unknown";
    }

    // Configure specified sensors
    for (int i = 0; i < count; i++)
    {
        adc_channel_t channel = configs[i].channel;

        if (channel >= SOC_ADC_CHANNEL_NUM(ADC_UNIT_1))
        {
            ESP_LOGW(TAG, "Invalid channel %d, skipping", channel);
            continue;
        }

        // Configure ADC channel
        adc_oneshot_chan_cfg_t adc_chan_config = {
            .atten = DEFAULT_ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH_12,
        };

        ret = adc_oneshot_config_channel(adc_handle, channel, &adc_chan_config);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to configure ADC channel %d: %s", channel, esp_err_to_name(ret));
            continue;
        }

        // Initialize calibration for this channel
        bool cali_success = adc_calibration_init(DEFAULT_ADC_UNIT, channel, DEFAULT_ADC_ATTEN, &sensors[channel].cali_handle);
        if (cali_success)
        {
            calibration_enabled = true;
        }

        // Set sensor properties
        sensors[channel].name = configs[i].name;
        sensors[channel].is_touch_sensor = configs[i].is_touch_sensor;
        sensors[channel].configured = true;

        // Set default calibration values for flex sensors
        if (!configs[i].is_touch_sensor)
        {
            sensors[channel].raw_straight = 450.0f;
            sensors[channel].raw_bent = 1250.0f;
            sensors[channel].voltage_straight = 1300.0f;
            sensors[channel].voltage_bent = 500.0f;
        }

        ESP_LOGI(TAG, "Configured %s sensor on channel %d (%s)",
                 configs[i].is_touch_sensor ? "touch" : "flex",
                 channel, configs[i].name);
    }

    component_initialized = true;
    ESP_LOGI(TAG, "ADC sensor component initialized with %d sensors", count);
    return ESP_OK;
}

void adc_sensor_deinit(void)
{
    if (!component_initialized)
    {
        return;
    }

    // Deinitialize calibration for all channels
    for (int i = 0; i < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1); i++)
    {
        if (sensors[i].cali_handle != NULL)
        {
            adc_calibration_deinit(sensors[i].cali_handle);
        }
    }

    // Deinitialize ADC unit
    if (adc_handle != NULL)
    {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }

    component_initialized = false;
    ESP_LOGI(TAG, "ADC sensor component deinitialized");
}

esp_err_t adc_sensor_add_channel(adc_channel_t channel)
{
    if (!component_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (channel >= SOC_ADC_CHANNEL_NUM(ADC_UNIT_1))
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Configure ADC channel
    adc_oneshot_chan_cfg_t adc_chan_config = {
        .atten = DEFAULT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };

    esp_err_t ret = adc_oneshot_config_channel(adc_handle, channel, &adc_chan_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure ADC channel %d: %s", channel, esp_err_to_name(ret));
        return ret;
    }

    // Initialize calibration for this channel
    bool cali_success = adc_calibration_init(DEFAULT_ADC_UNIT, channel, DEFAULT_ADC_ATTEN, &sensors[channel].cali_handle);
    if (cali_success)
    {
        calibration_enabled = true;
    }

    ESP_LOGI(TAG, "Added adc sensor on channel %d", channel);
    return ESP_OK;
}

int adc_sensor_read_raw(adc_channel_t channel)
{
    if (!component_initialized || adc_handle == NULL)
    {
        ESP_LOGE(TAG, "Component not initialized");
        return -1;
    }

    if (!sensors[channel].configured)
    {
        ESP_LOGE(TAG, "Channel %d not configured", channel);
        return -1;
    }

    int raw_value;
    esp_err_t ret = adc_oneshot_read(adc_handle, channel, &raw_value);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read ADC channel %d: %s", channel, esp_err_to_name(ret));
        return -1;
    }

    return raw_value;
}

int adc_sensor_read_voltage(adc_channel_t channel)
{
    if (!sensors[channel].configured)
    {
        return -1;
    }

    if (!calibration_enabled || sensors[channel].cali_handle == NULL)
    {
        return -1;
    }

    int raw_value = adc_sensor_read_raw(channel);
    if (raw_value == -1)
    {
        return -1;
    }

    int voltage = 0;
    esp_err_t ret = adc_cali_raw_to_voltage(sensors[channel].cali_handle, raw_value, &voltage);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Voltage conversion failed for channel %d: %s", channel, esp_err_to_name(ret));
        return -1;
    }

    return voltage;
}

float flex_sensor_get_angle(adc_channel_t channel)
{
    if (!component_initialized || !sensors[channel].configured)
    {
        return -1.0f;
    }

    // Don't calculate angle for touch sensors
    if (sensors[channel].is_touch_sensor)
    {
        ESP_LOGW(TAG, "Channel %d is a touch sensor, no angle calculation", channel);
        return -1.0f;
    }

    int raw_value = adc_sensor_read_raw(channel);
    if (raw_value == -1)
    {
        return -1.0f;
    }

    if (calibration_enabled && sensors[channel].cali_handle != NULL)
    {
        int voltage = adc_sensor_read_voltage(channel);
        if (voltage != -1)
        {
            return calculate_bend_angle_from_voltage(voltage,
                                                     sensors[channel].voltage_straight,
                                                     sensors[channel].voltage_bent);
        }
    }

    // Fallback to raw ADC values
    return calculate_bend_angle_from_raw(raw_value,
                                         sensors[channel].raw_straight,
                                         sensors[channel].raw_bent);
}

bool adc_sensor_is_touch_sensor(adc_channel_t channel)
{
    if (!sensors[channel].configured)
    {
        return false;
    }
    return sensors[channel].is_touch_sensor;
}

const char *adc_sensor_get_name(adc_channel_t channel)
{
    if (!sensors[channel].configured)
    {
        return "unknown";
    }
    return sensors[channel].name;
}

void calibrate_all_flex_sensors(void)
{
    if (!component_initialized)
    {
        ESP_LOGE(TAG, "ADC component not initialized");
        return;
    }

    char line[22];
    char temp_buf[25];

    snprintf(temp_buf, sizeof(temp_buf), "Status: Calibrating");
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(4, 0, line);

    snprintf(temp_buf, sizeof(temp_buf), "flex sensors");
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(5, 0, line);

    ESP_LOGI(TAG, "Starting flex sensor calibration");

    // Find all flex sensors
    adc_channel_t flex_channels[5];
    const char *flex_names[5];
    int flex_count = 0;

    // Collect all configured flex sensors
    for (int i = 0; i < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1); i++)
    {
        if (sensors[i].configured && !sensors[i].is_touch_sensor)
        {
            if (flex_count < 5)
            {
                flex_channels[flex_count] = i;
                flex_names[flex_count] = sensors[i].name;
                flex_count++;
            }
        }
    }

    if (flex_count == 0)
    {
        ESP_LOGE(TAG, "No flex sensors found for calibration");
        return;
    }

    ESP_LOGI(TAG, "Found %d flex sensors to calibrate:", flex_count);
    for (int i = 0; i < flex_count; i++)
    {
        ESP_LOGI(TAG, "  - %s (Channel %d)", flex_names[i], flex_channels[i]);
    }

    snprintf(temp_buf, sizeof(temp_buf), "Status: Keep your");
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(4, 0, line);

    snprintf(temp_buf, sizeof(temp_buf), "fingers straight");
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(5, 0, line);

    ESP_LOGI(TAG, "Keep all %d flex sensors straight", flex_count);
    ESP_LOGI(TAG, "Waiting 3 seconds");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Read straight position for all sensors
    ESP_LOGI(TAG, "Recording straight position values");
    int straight_raw[5] = {0};
    int straight_voltage[5] = {0};

    for (int i = 0; i < flex_count; i++)
    {
        adc_channel_t channel = flex_channels[i];

        // Take multiple samples and average for stability
        int sum_raw = 0;
        int sum_voltage = 0;
        int samples = 10;

        for (int s = 0; s < samples; s++)
        {
            int raw = adc_sensor_read_raw(channel);
            int voltage = adc_sensor_read_voltage(channel);

            if (raw != -1)
                sum_raw += raw;
            if (voltage != -1)
                sum_voltage += voltage;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        straight_raw[i] = sum_raw / samples;
        straight_voltage[i] = sum_voltage / samples;

        ESP_LOGI(TAG, "  %s: Raw=%d, Voltage=%dmV",
                 flex_names[i], straight_raw[i], straight_voltage[i]);
    }

    snprintf(temp_buf, sizeof(temp_buf), "Status: Bent your");
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(4, 0, line);

    snprintf(temp_buf, sizeof(temp_buf), "fingers fully");
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(5, 0, line);

    ESP_LOGI(TAG, "Bend all %d flex sensors fully", flex_count);
    ESP_LOGI(TAG, "Waiting 3 seconds");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Read bent position for all sensors
    ESP_LOGI(TAG, "Recording bent position values");
    int bent_raw[5] = {0};
    int bent_voltage[5] = {0};

    for (int i = 0; i < flex_count; i++)
    {
        adc_channel_t channel = flex_channels[i];

        // Take multiple samples and average for stability
        int sum_raw = 0;
        int sum_voltage = 0;
        int samples = 10;

        for (int s = 0; s < samples; s++)
        {
            int raw = adc_sensor_read_raw(channel);
            int voltage = adc_sensor_read_voltage(channel);

            if (raw != -1)
                sum_raw += raw;
            if (voltage != -1)
                sum_voltage += voltage;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        bent_raw[i] = sum_raw / samples;
        bent_voltage[i] = sum_voltage / samples;

        ESP_LOGI(TAG, "  %s: Raw=%d, Voltage=%dmV",
                 flex_names[i], bent_raw[i], bent_voltage[i]);
    }

    ESP_LOGI(TAG, "Applying calibration values to all sensors");
    for (int i = 0; i < flex_count; i++)
    {
        adc_channel_t channel = flex_channels[i];

        flex_sensor_set_calibration(channel,
                                    straight_raw[i], straight_voltage[i],
                                    bent_raw[i], bent_voltage[i]);
    }

    snprintf(temp_buf, sizeof(temp_buf), "Status: Flex sensors");
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(4, 0, line);

    snprintf(temp_buf, sizeof(temp_buf), "calibrated");
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(5, 0, line);

    ESP_LOGI(TAG, "Calibration complete for all %d flex sensors!", flex_count);
}

void flex_sensor_set_calibration(adc_channel_t channel,
                                 int straight_raw, int straight_voltage,
                                 int bent_raw, int bent_voltage)
{
    if (!sensors[channel].configured || sensors[channel].is_touch_sensor)
    {
        return;
    }

    sensors[channel].raw_straight = (float)straight_raw;
    sensors[channel].raw_bent = (float)bent_raw;
    sensors[channel].voltage_straight = (float)straight_voltage;
    sensors[channel].voltage_bent = (float)bent_voltage;
    sensors[channel].calibrated = true;

    ESP_LOGI(TAG, "Set calibration for %s: straight=%d/%dmV, bent=%d/%dmV",
             sensors[channel].name, straight_raw, straight_voltage, bent_raw, bent_voltage);
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

// Function to calculate bend angle from normalized value (0.0 - 1.0)
static float calculate_bend_angle_normalized(float normalized_value)
{
    // normalized_value should be between 0.0 (straight) and 1.0 (fully bent)
    float angle = normalized_value * (DEFAULT_FLEX_BENT_ANGLE - DEFAULT_FLEX_STRAIGHT_ANGLE);

    // Constrain the angle
    if (angle < DEFAULT_FLEX_STRAIGHT_ANGLE)
        angle = DEFAULT_FLEX_STRAIGHT_ANGLE;
    if (angle > DEFAULT_FLEX_BENT_ANGLE)
        angle = DEFAULT_FLEX_BENT_ANGLE;

    return angle;
}

// Function to calculate bend angle from voltage
static float calculate_bend_angle_from_voltage(int voltage_mv, float voltage_straight, float voltage_bent)
{
    if (voltage_bent == voltage_straight)
    {
        return DEFAULT_FLEX_BENT_ANGLE; // Avoid division by zero
    }

    float normalized = (float)abs((voltage_mv - voltage_straight) /
                                  (voltage_bent - voltage_straight));
    return calculate_bend_angle_normalized(normalized);
}

// Function to calculate bend angle from raw ADC
static float calculate_bend_angle_from_raw(int raw_adc, float raw_straight, float raw_bent)
{
    if (raw_bent == raw_straight)
    {
        return DEFAULT_FLEX_BENT_ANGLE; // Avoid division by zero
    }
    float normalized = (float)abs((raw_adc - raw_straight) /
                                  (raw_bent - raw_straight));
    return calculate_bend_angle_normalized(normalized);
}