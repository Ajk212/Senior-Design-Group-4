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
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_system.h"

#include "imu_frame.h"
#include "final_model_30SamplesV2.h"
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
constexpr int kTensorArenaSize = 128 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
constexpr int WINDOW_SIZE = 20;

tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

const char *gesture_names[] = {
    "ccw_circle",
    "curl_wave",
    "cw_circle",
    "grip_hand",
    "mouse_down",
    "mouse_left",
    "mouse_right",
    "mouse_up",
    "swipe_down",
    "swipe_left",
    "swipe_right",
    "thumb_down",
    "thumb_up"
};
constexpr int NUM_GESTURES = 13;
constexpr int NUM_FEATURES = 17;
float sensor_window[WINDOW_SIZE][NUM_FEATURES];
int curr_index = 0;
bool window_filled = false;
bool wait_for_double_click = false;
bool mode_toggle_triggered = false;
constexpr uint32_t DOUBLE_CLICK_TIMEOUT_MS = 400;
constexpr uint32_t LONG_PRESS_MS = 3000;

const char *model_modes[] = {
    "Locked",
    "Navigation"};

int current_mode = 0; // 0=locked, 1=navigational

uint32_t touch_pressed_start = 0;
uint32_t last_touch_release_time = 0;
bool touch_was_pressed = false;

uint32_t gesture_detected_time_us = 0;
uint32_t ack_received_time_us = 0;


// I2C Configuration
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000000
#define PULLUP GPIO_PULLUP_DISABLE
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

// ADC Configuration
#define THUMB_ADC_CHANNEL ADC_CHANNEL_2
#define POINTER_ADC_CHANNEL ADC_CHANNEL_3
#define MIDDLE_ADC_CHANNEL ADC_CHANNEL_4
#define RING_ADC_CHANNEL ADC_CHANNEL_5
#define PINKY_ADC_CHANNEL ADC_CHANNEL_6
#define TOUCH_ADC_CHANNEL ADC_CHANNEL_1

static const adc_sensor_config_t sensor_configs[] = {
    {THUMB_ADC_CHANNEL, "Thumb", false},
    {POINTER_ADC_CHANNEL, "Pointer", false},
    {MIDDLE_ADC_CHANNEL, "Middle", false},
    {RING_ADC_CHANNEL, "Ring", false},
    {PINKY_ADC_CHANNEL, "Pinky", false},
    {TOUCH_ADC_CHANNEL, "Touch", true},
};

// Battery variables
static constexpr int BATTERY_CAPACITY_MAH = 1000;
static constexpr int AVERAGE_CURRENT_MA = 100;    // measure or estimate
static constexpr int PERSIST_INTERVAL_MS = 30000; // store every 30s

struct BatteryRuntime
{
    uint64_t boot_offset_ms = 0;
    uint64_t charge_time_ms = 0;
};

static BatteryRuntime batt;

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

// Battery and nvs functions
static void nvs_load_runtime()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("battery", NVS_READONLY, &h);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "First boot or NVS missing.");
        batt.boot_offset_ms = 0;
        batt.charge_time_ms = esp_timer_get_time() / 1000ULL;
        return;
    }
    nvs_get_u64(h, "boot_off", &batt.boot_offset_ms);
    nvs_get_u64(h, "charge_ms", &batt.charge_time_ms);
    nvs_close(h);

    ESP_LOGI(TAG, "Loaded: boot_off=%llu ms, charge=%llu ms",
             batt.boot_offset_ms, batt.charge_time_ms);
}

static void nvs_save_runtime(uint64_t now_ms)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("battery", NVS_READWRITE, &h));

    ESP_ERROR_CHECK(nvs_set_u64(h, "boot_off", batt.boot_offset_ms));
    ESP_ERROR_CHECK(nvs_set_u64(h, "charge_ms", batt.charge_time_ms));

    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
}

static float battery_get_soc()
{
    uint64_t now_ms = esp_timer_get_time() / 1000ULL;

    uint64_t uptime_ms = batt.boot_offset_ms + (now_ms - batt.charge_time_ms);
    float hours = uptime_ms / 3600000.0f;

    float used = hours * AVERAGE_CURRENT_MA;
    float soc = 100.0f * (1.0f - used / BATTERY_CAPACITY_MAH);

    if (soc < 0)
        soc = 0;
    if (soc > 100)
        soc = 100;

    return soc;
}

const uint8_t *battery_icon_from_soc(int soc)
{
    if (soc >= 95)
        return icon_battery_full_24x16;
    else if (soc >= 90)
        return icon_battery_90_24x16;
    else if (soc >= 80)
        return icon_battery_80_24x16;
    else if (soc >= 70)
        return icon_battery_70_24x16;
    else if (soc >= 60)
        return icon_battery_60_24x16;
    else if (soc >= 50)
        return icon_battery_50_24x16;
    else if (soc >= 40)
        return icon_battery_40_24x16;
    else if (soc >= 30)
        return icon_battery_30_24x16;
    else if (soc >= 20)
        return icon_battery_20_24x16;
    else if (soc >= 10)
        return icon_battery_10_24x16;
    else
        return icon_battery_5_24x16;
}

static void battery_update_task(void *)
{
    uint64_t last_save = 0;

    while (true)
    {
        float soc = battery_get_soc();
        const uint8_t *icon = battery_icon_from_soc(soc);
        oled_draw_bitmap(0, 100, icon, 24, 16, false);
        ESP_LOGI(TAG, "Estimated SoC: %.1f%%", soc);

        uint64_t now_ms = esp_timer_get_time() / 1000ULL;

        if (now_ms - last_save > PERSIST_INTERVAL_MS)
        {
            batt.boot_offset_ms += PERSIST_INTERVAL_MS;
            nvs_save_runtime(now_ms);
            last_save = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // update every 5 sec
    }
}

extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "Starting HandSense");

    const tflite::Model *model = tflite::GetModel(final_model_30SamplesV2_tflite);

    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        ESP_LOGI(TAG, "Model schema version mismatch!");
        while (1)
            ;
    }

    tflite::MicroMutableOpResolver<11> resolver;
        resolver.AddConv2D();           
        resolver.AddFullyConnected();
        resolver.AddSoftmax();
        resolver.AddReshape();
        resolver.AddMaxPool2D();           
        resolver.AddMean();                
        resolver.AddAdd();
        resolver.AddMul();
        resolver.AddPad();
        resolver.AddRelu();
        resolver.AddExpandDims();

    // static tflite::MicroErrorReporter micro_error_reporter;
    // tflite::ErrorReporter *error_reporter = &micro_error_reporter;

    static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);

    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        ESP_LOGI(TAG, "Tensor allocation failed!");
        while (1);
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    ESP_LOGI(TAG, "1D CNN Model initialized successfully");

    // Initialize Bluetooth
    ble_init();

    // Battery task and NVS
    nvs_load_runtime();

    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // Test I2C communication by scanning for devices
    i2c_scanner();

    // Set up initial OLED display
    oled_init_simple();
    oled_draw_bitmap(0, 0, icon_bluetooth_disconn_14x16, 14, 16, false);
    oled_print_text_simple(0, 20, "HandSense");
    float soc_percent = battery_get_soc();
    const uint8_t *icon = battery_icon_from_soc(soc_percent);
    oled_draw_bitmap(0, 100, icon, 24, 16, false);

    char buf[21];
    snprintf(buf, sizeof(buf), "Mode: %s", model_modes[current_mode]);
    oled_print_text(3, 0, buf);
    oled_print_text(4, 0, "Status:");
    oled_print_text(5, 0, "Advertising BT");
    oled_print_text(6, 0, "Gesture:");

    xTaskCreate(battery_update_task, "batt_task", 4096, nullptr, 5, nullptr);

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

    oled_print_text(4, 0, "Status:");
    oled_print_text(5, 0, "Ready for gestures");

    // Data variables
    imu_data_t imu_data_1, imu_data_2;
    float flex_angles[5] = {0};

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
            // ESP_LOGI(TAG, "IMU1 Accel: X=%.2f, Y=%.2f, Z=%.2f m/s^2", imu_data_1.ax, imu_data_1.ay, imu_data_1.az);
            // ESP_LOGI(TAG, "IMU1 Gyro:  X=%.2f, Y=%.2f, Z=%.2f deg/s", imu_data_1.gx, imu_data_1.gy, imu_data_1.gz);
            // ESP_LOGI(TAG, "IMU2 Accel: X=%.2f, Y=%.2f, Z=%.2f m/s^2", imu_data_2.ax, imu_data_2.ay, imu_data_2.az);
            // ESP_LOGI(TAG, "IMU2 Gyro:  X=%.2f, Y=%.2f, Z=%.2f deg/s", imu_data_2.gx, imu_data_2.gy, imu_data_2.gz);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read IMU");
        }

        // Read all adc sensors
        for (int i = 0; i < sizeof(sensor_configs) / sizeof(sensor_configs[0]); i++)
        {
            adc_channel_t channel = sensor_configs[i].channel;
            const char *name = sensor_configs[i].name;
            bool is_touch = sensor_configs[i].is_touch_sensor;

            int raw = adc_sensor_read_raw(channel);
            int voltage = adc_sensor_read_voltage(channel);

            if (is_touch)
            {
                bool is_touch_pressed = (voltage < 1350); // TRUE if touched

                uint32_t now = esp_log_timestamp(); // ms since boot

                if (is_touch_pressed)
                {
                    if (!touch_was_pressed)
                    {
                        touch_pressed_start = now;
                        touch_was_pressed = true;
                        mode_toggle_triggered = false;
                    }
                    else
                    {
                        uint32_t held_ms = now - touch_pressed_start;
                        if (held_ms >= 3000 && !mode_toggle_triggered)
                        {
                            //Toggle mode
                            current_mode = (current_mode + 1) % 2; 
                            ESP_LOGI(TAG, "Model Mode Toggled to %s", model_modes[current_mode]);

                            // Update OLED
                            snprintf(buf, sizeof(buf), "Mode: %s", model_modes[current_mode]);
                            oled_print_text(3, 0, buf);

                            //Prevent repeated toggles while still pressed
                            mode_toggle_triggered = true;
                        }
                    }
                }
                else
                {
                    if(touch_was_pressed){
                        uint32_t press_durration = now - touch_pressed_start;
                        touch_was_pressed = false;

                        if(press_durration < LONG_PRESS_MS){

                            if(wait_for_double_click){
                                uint32_t time_since_click = now - last_touch_release_time;

                                if(time_since_click <= DOUBLE_CLICK_TIMEOUT_MS){
                                    //Double click recieved
                                    if(ble_is_connected() && ble_notifications_enabled() && (current_mode == 1)){
                                        if(send_string_and_wait_ack("double_click")){
                                            ESP_LOGI(TAG, "Message sent successfully");
                                            drv2605_play_effect(drv, DRV2605_EFFECT_DOUBLE_CLICK);
                                        }
                                        else{
                                            ESP_LOGE(TAG, "Message delivery failed");
                                        }
                                    }

                                    wait_for_double_click = false;
                                    last_touch_release_time = 0;
                                }
                                else{
                                    //Treating as new single click
                                    wait_for_double_click = true;
                                    last_touch_release_time = now;
                                }
                            }
                            else{
                                //Recieved first click
                                wait_for_double_click = true;
                                last_touch_release_time = now;
                            }
                        }
                    }
                    else if(wait_for_double_click){
                        uint32_t time_since_first_click = now - last_touch_release_time;

                        if(time_since_first_click > DOUBLE_CLICK_TIMEOUT_MS){
                            //Recieved single click
                            if(ble_is_connected() && ble_notifications_enabled() && (current_mode == 1)){
                                if(send_string_and_wait_ack("click")){
                                    ESP_LOGI(TAG, "Message sent successfully");
                                    drv2605_play_effect(drv, DRV2605_EFFECT_DOUBLE_CLICK);
                                }
                                else{
                                    ESP_LOGE(TAG, "Message delivery failed");
                                }
                            }
                            //Reset flags
                            wait_for_double_click = false;
                            last_touch_release_time = 0;
                        }
                    }
                }
            }
            else
            {
                flex_angles[i] = flex_sensor_get_angle(channel);
                //ESP_LOGI(TAG, "%s: Raw: %d, Voltage: %d mV, Angle: %.1f°",
                         //name, raw, voltage, flex_angles[i]);
            }
        }

        // Read senor data frame 

        sensor_window[curr_index][0] = imu_data_1.ax;
        sensor_window[curr_index][1] = imu_data_1.ay;
        sensor_window[curr_index][2] = imu_data_1.az;
        sensor_window[curr_index][3] = imu_data_1.gx;
        sensor_window[curr_index][4] = imu_data_1.gy;
        sensor_window[curr_index][5] = imu_data_1.gz;

        sensor_window[curr_index][6] = imu_data_2.ax;
        sensor_window[curr_index][7] = imu_data_2.ay;
        sensor_window[curr_index][8] = imu_data_2.az;
        sensor_window[curr_index][9] = imu_data_2.gx;
        sensor_window[curr_index][10] = imu_data_2.gy;
        sensor_window[curr_index][11] = imu_data_2.gz;

        sensor_window[curr_index][12] = flex_angles[0];
        sensor_window[curr_index][13] = flex_angles[1];
        sensor_window[curr_index][14] = flex_angles[2];
        sensor_window[curr_index][15] = flex_angles[3];
        sensor_window[curr_index][16] = flex_angles[4];

        curr_index = (curr_index + 1) % WINDOW_SIZE;
        if (curr_index == 0)
        {
            window_filled = true;
        }

        if (window_filled && (current_mode == 1))
        {
            
            for (int t = 0; t < WINDOW_SIZE; t++)
            {
                int i = (curr_index + t) % WINDOW_SIZE;
                for (int f = 0; f < NUM_FEATURES; f++){
                    input->data.f[t * NUM_FEATURES + f] = sensor_window[i][f];
                }
            }

            /* //Training data logs
            printf("%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
                imu_data_1.ax, imu_data_1.ay, imu_data_1.az,
                imu_data_1.gx, imu_data_1.gy, imu_data_1.gz,
                imu_data_2.ax, imu_data_2.ay, imu_data_2.az,
                imu_data_2.gx, imu_data_2.gy, imu_data_2.gz,
                flex_angles[0], flex_angles[1], flex_angles[2], flex_angles[3], flex_angles[4]
            );
            
            */
            // Run model - If failed print and continue
            if (interpreter->Invoke() != kTfLiteOk)
            {
                ESP_LOGI(TAG, "Model inference failed!");
                continue;
            }

            // Locate most probable gesture
            int max_index = 0;
            float max_value = output->data.f[0];
            for (int i = 1; i < NUM_GESTURES; i++)
            {
                if (output->data.f[i] > max_value)
                {
                    max_value = output->data.f[i];
                    max_index = i;
                }
            }

            // Take index and convert to gesture name
            const char *predicted_gesture = gesture_names[max_index];
            ESP_LOGI(TAG, "=== Predictions ===");
            for (int i = 0; i < NUM_GESTURES; i++)
            {
                ESP_LOGI(TAG, "%s: %.2f%%", gesture_names[i], output->data.f[i] * 100.0f);
            }
            ESP_LOGI(TAG, "Winner: %s (%.2f%%)", predicted_gesture, max_value * 100.0f);

            if (max_value < 0.85f)
            {
                ESP_LOGI(TAG, "Confidence too low, ignoring gesture");
            }
            else{

            
                if (ble_is_connected() && ble_notifications_enabled())
                {
                    gesture_detected_time_us = esp_timer_get_time();
                    
                    // Send a message and wait for acknowledgment
                    if (send_string_and_wait_ack(predicted_gesture))
                    {
                        ack_received_time_us = esp_timer_get_time();
                        ESP_LOGI(TAG, "Latency: %.2f ms", (ack_received_time_us - gesture_detected_time_us) / 1000.0f);
                        
                        snprintf(buf, sizeof(buf), "%s", predicted_gesture);
                        oled_print_text(7, 0, buf);

                        ESP_LOGI(TAG, "Playing long buzz");
                        drv2605_play_effect(drv, DRV2605_EFFECT_LONG_BUZZ);

                        ESP_LOGI("MAIN", "Message delivered successfully");


                        window_filled = false;
                        curr_index = 0;
                        memset(sensor_window, 0, sizeof(sensor_window));
                    }
                    else
                    {
                        oled_print_text(7, 0, "");

                        ESP_LOGE("MAIN", "Message delivery failed");
                        ESP_LOGI(TAG, "Playing double click");
                        drv2605_play_effect(drv, DRV2605_EFFECT_DOUBLE_CLICK);
                    }
                }
                else
                {
                    ESP_LOGI("MAIN", "Waiting for BLE connection and notifications enabled");
                }
            }
            /**/
            ESP_LOGI(TAG, "---");
            
            
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    drv2605_deinit(drv);
    adc_sensor_deinit();
}