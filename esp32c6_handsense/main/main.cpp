extern "C"
{
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#include "imu_frame.h"
#include "Lite_LSTM_Test4.h"
#include "mpu6050.h"
#include "oled.h"
#include "drv2605.h"
#include "adc.h"
#include "ble.h"
}

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
// #include "tensorflow/lite/micro/micro_error_reporter.h"

static const char *TAG = "MAIN";

// IMU variables
static calibration_data_t calib_imu1 = {0};
static calibration_data_t calib_imu2 = {0};

// Model variables
constexpr int kTensorArenaSize = 64 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
constexpr int WINDOW_SIZE = 20;

tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

const char *gesture_names[] = {
    "rest",
    "tilt_left",
    "tilt_right",
    "tilt_forward",
    "tilt_backward",
};

float gyro_window[WINDOW_SIZE][3];
int curr_index = 0;
bool window_filled = false;

// const char *model_modes[] = {
//     "locked",
//     "navigational",
//     "text"};

// I2C Configuration
#define I2C_MASTER_SCL_IO 21
#define I2C_MASTER_SDA_IO 22
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000000
#define PULLUP GPIO_PULLUP_ENABLE
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

// ADC Configuration
#define FLEX_1_ADC_CHANNEL ADC_CHANNEL_5
#define FLEX_2_ADC_CHANNEL ADC_CHANNEL_3
#define FLEX_3_ADC_CHANNEL ADC_CHANNEL_2
#define FLEX_4_ADC_CHANNEL ADC_CHANNEL_1
#define FLEX_5_ADC_CHANNEL ADC_CHANNEL_4
#define TOUCH_ADC_CHANNEL ADC_CHANNEL_0

static const adc_sensor_config_t sensor_configs[] = {
    {FLEX_1_ADC_CHANNEL, "Thumb", false},
    {FLEX_2_ADC_CHANNEL, "Pointer", false},
    {FLEX_3_ADC_CHANNEL, "Middle", false},
    {FLEX_4_ADC_CHANNEL, "Index", false},
    {FLEX_5_ADC_CHANNEL, "Pinky", false},
    {TOUCH_ADC_CHANNEL, "Touch", true},
};

// Initialize I2C master
esp_err_t i2c_master_init(void)
{
    i2c_config_t conf;
    memset(&conf, 0, sizeof(i2c_config_t));
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = PULLUP;
    conf.scl_pullup_en = PULLUP;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

void i2c_scanner(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Found device at address: 0x%02X", addr);
        }
    }
    ESP_LOGI(TAG, "I2C scan complete");
}

extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "Starting HandSense");

    const tflite::Model *model = tflite::GetModel(Lite_LSTM_Test4_tflite);

    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        ESP_LOGI(TAG, "Model schema version mismatch!");
        while (1)
            ;
    }

    tflite::MicroMutableOpResolver<15> resolver;
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddUnidirectionalSequenceLSTM();
    resolver.AddReshape();
    resolver.AddShape();
    resolver.AddStridedSlice();
    resolver.AddTranspose();
    resolver.AddUnpack();
    resolver.AddPack();
    resolver.AddFill();
    resolver.AddAdd();
    resolver.AddSplit();
    resolver.AddLogistic();
    resolver.AddMul();
    resolver.AddTanh();

    // static tflite::MicroErrorReporter micro_error_reporter;
    // tflite::ErrorReporter *error_reporter = &micro_error_reporter;

    static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);

    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        ESP_LOGI(TAG, "Tensor allocation failed!");
        while (1)
            ;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    ESP_LOGI(TAG, "LSTM Model initialized successfully");

    // Initialize Bluetooth
    ble_init();

    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");
    // Test I2C communication by scanning for devices
    i2c_scanner();

    // Initialize MPU6050 sensor
    if (mpu6050_init_both_advanced(DLPF_CFG_4, ACCEL_FS_SEL_4G, GYRO_FS_SEL_500DPS) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize MPU6050 sensors");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Calibrate both sensors
    mpu6050_calibrate_both_sensor(&calib_imu1, &calib_imu2);

    oled_init_simple();

    // Configure and initialize DRV2605
    drv2605_config_t drv_config = DRV2605_DEFAULT_CONFIG();
    drv_config.i2c_port = I2C_MASTER_NUM;
    drv_config.enable_pin = GPIO_NUM_5;

    drv2605_handle_t drv = drv2605_init(&drv_config);
    if (drv == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize DRV2605");
        return;
    }

    ESP_ERROR_CHECK(adc_sensor_init(sensor_configs, 6));

    calibrate_all_flex_sensors();

    ESP_LOGI(TAG, "All devices initialized, starting readings...");

    // Data variables
    imu_data_t imu_data_1, imu_data_2;

    while (1)
    {

        bool imu1_ok = (mpu6050_read_all_data(MPU6050_I2C_ADDR_1,
                                              &imu_data_1.ax, &imu_data_1.ay, &imu_data_1.az,
                                              &imu_data_1.gx, &imu_data_1.gy, &imu_data_1.gz,
                                              &imu_data_1.temperature) == ESP_OK);

        bool imu2_ok = (mpu6050_read_all_data(MPU6050_I2C_ADDR_2,
                                              &imu_data_2.ax, &imu_data_2.ay, &imu_data_2.az,
                                              &imu_data_2.gx, &imu_data_2.gy, &imu_data_2.gz,
                                              &imu_data_2.temperature) == ESP_OK);

        if (imu1_ok && imu2_ok)
        {
            apply_calibration(&imu_data_1, &calib_imu1);
            apply_calibration(&imu_data_2, &calib_imu2);

            // Log to serial
            // ESP_LOGI(TAG, "IMU1 Accel: X=%.2f, Y=%.2f, Z=%.2f m/s^2", accel1_x, accel1_y, accel1_z);
            // ESP_LOGI(TAG, "IMU1 Gyro:  X=%.2f, Y=%.2f, Z=%.2f deg/s", gyro1_x, gyro1_y, gyro1_z);
            // ESP_LOGI(TAG, "Temps: IMU1=%.1f°F", temp1_f);
            // ESP_LOGI(TAG, "IMU2 Accel: X=%.2f, Y=%.2f, Z=%.2f m/s^2", accel2_x, accel2_y, accel2_z);
            // ESP_LOGI(TAG, "IMU2 Gyro:  X=%.2f, Y=%.2f, Z=%.2f deg/s", gyro2_x, gyro2_y, gyro2_z);
            // ESP_LOGI(TAG, "Temps: IMU2=%.1f°F", temp2_f);

            oled_update_display(imu_data_1.ax, imu_data_1.ay, imu_data_1.az,
                                imu_data_2.ax, imu_data_2.ay, imu_data_2.az,
                                imu_data_1.gx, imu_data_1.gy, imu_data_1.gz,
                                imu_data_2.gx, imu_data_2.gy, imu_data_2.gz,
                                imu_data_1.temperature, imu_data_2.temperature);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read IMU");
        }

        // Data variable
        float flex_angles[5] = {0};

        // Read all sensors
        // for (int i = 0; i < sizeof(sensor_configs) / sizeof(sensor_configs[0]); i++)
        // {
        //     adc_channel_t channel = sensor_configs[i].channel;
        //     const char *name = sensor_configs[i].name;
        //     bool is_touch = sensor_configs[i].is_touch_sensor;

        //     int raw = adc_sensor_read_raw(channel);
        //     int voltage = adc_sensor_read_voltage(channel);

        // if (adc_sensor_read_voltage(TOUCH_ADC_CHANNEL) > 500)
        // {
        //     // toggle model modes
        // }
        //         ESP_LOGI(TAG, "%s: Raw: %d, Voltage: %d mV", name, raw, voltage);
        //     }
        //     else
        //     {
        //         flex_angle[i] = flex_sensor_get_angle(channel);
        //         ESP_LOGI(TAG, "%s: Raw: %d, Voltage: %d mV, Angle: %.1f°",
        //                  name, raw, voltage, angle);
        //     }
        // }

        // Read senor data frame - 20 values of 3-axis gyro

        gyro_window[curr_index][0] = imu_data_1.ax;
        gyro_window[curr_index][1] = imu_data_1.ay;
        gyro_window[curr_index][2] = imu_data_1.az;

        curr_index = (curr_index + 1) % WINDOW_SIZE;
        if (curr_index == 0)
        {
            window_filled = true;
        }

        if (window_filled)
        {
            for (int t = 0; t < WINDOW_SIZE; t++)
            {
                int i = (curr_index + t) % WINDOW_SIZE;
                input->data.f[t * 3 + 0] = gyro_window[i][0];
                input->data.f[t * 3 + 1] = gyro_window[i][1];
                input->data.f[t * 3 + 2] = gyro_window[i][2];
            }

            // Run model - If failed print and continue
            if (interpreter->Invoke() != kTfLiteOk)
            {
                ESP_LOGI(TAG, "Model inference failed!");
                continue;
            }

            // Locate most probable gesture
            int max_index = 0;
            float max_value = output->data.f[0];
            for (int i = 1; i < output->dims->data[1]; i++)
            {
                if (output->data.f[i] > max_value)
                {
                    max_value = output->data.f[i];
                    max_index = i;
                }
            }

            // Take index and convert to gesture name
            const char *predicted_gesture = gesture_names[max_index];
            ESP_LOGI(TAG, "Predicted gesture: %s (confidence=%.2f)", predicted_gesture, max_value);

            if (ble_is_connected() && ble_notifications_enabled())
            {
                // Send a message and wait for acknowledgment
                if (send_string_and_wait_ack(predicted_gesture))
                {
                    // ESP_LOGI(TAG, "Playing long buzz");
                    // drv2605_play_effect(drv, DRV2605_EFFECT_LONG_BUZZ);
                    // OLED update gesture field
                    ESP_LOGI("MAIN", "Message delivered successfully");
                }
                else
                {
                    ESP_LOGE("MAIN", "Message delivery failed");
                    // ESP_LOGI(TAG, "Playing long buzz");
                    // drv2605_play_effect(drv, DRV2605_EFFECT_DOUBLE_CLICK);
                }
            }
            else
            {
                ESP_LOGI("MAIN", "Waiting for BLE connection and notifications enabled");
            }

            ESP_LOGI(TAG, "---");
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    // drv2605_deinit(drv);
    // adc_sensor_deinit();
}