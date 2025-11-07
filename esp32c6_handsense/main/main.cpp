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
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "imu_frame.h"
#include "Lite_LSTM_Test.h"
#include "mpu6050.h"
#include "oled.h"
#include "drv2605.h"
#include "adc.h"
}

#include "all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
// #include "tensorflow/lite/version.h"

static const char *HAPTICTAG = "HAPTIC";
static const char *ADCTAG = "ADC";
static const char *IMUTAG = "MPU6050";

// Model variables
constexpr int kTensorArenaSize = 10 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
int WINDOW_SIZE = 20;

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

// I2C Configuration
#define I2C_MASTER_SCL_IO 21
#define I2C_MASTER_SDA_IO 22
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
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
    ESP_LOGI(IMUTAG, "Scanning I2C bus...");
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
            ESP_LOGI(IMUTAG, "Found device at address: 0x%02X", addr);
        }
    }
    ESP_LOGI(IMUTAG, "I2C scan complete");
}

// BLE Configuration
// #define GATTS_TAG "GATTS_BLE"

// #define GATTS_SERVICE_UUID 0x00FF
// #define GATTS_CHAR_UUID 0xFF01
// #define GATTS_DESCR_UUID 0x3333
// #define GATTS_NUM_HANDLE 4

// static char device_name[ESP_BLE_ADV_NAME_LEN_MAX] = "ESP32C6-HANDSENSE";

// #define PROFILE_NUM 1
// #define PROFILE_A_APP_ID 0

// // Global BLE variables
// static uint16_t current_conn_id = 0xFFFF;
// static uint16_t current_gatts_if = ESP_GATT_IF_NONE;
// static bool notifications_enabled = false;
// static uint16_t local_mtu = 23;

// // IMU data structure
// static imu_frame_t imu_data = {0};
// static uint32_t current_frame_count = 0;

// // BLE profile structure
// struct gatts_profile_inst
// {
//     esp_gatts_cb_t gatts_cb;
//     uint16_t gatts_if;
//     uint16_t app_id;
//     uint16_t conn_id;
//     uint16_t service_handle;
//     esp_gatt_srvc_id_t service_id;
//     uint16_t char_handle;
//     esp_bt_uuid_t char_uuid;
//     esp_gatt_perm_t perm;
//     esp_gatt_char_prop_t property;
//     uint16_t descr_handle;
//     esp_bt_uuid_t descr_uuid;
// };

// static uint8_t adv_service_uuid128[32] = {
//     /* LSB <--------------------------------------------------------------------------------> MSB */
//     // first uuid, 16bit, [12],[13] is the value
//     0xfb,
//     0x34,
//     0x9b,
//     0x5f,
//     0x80,
//     0x00,
//     0x00,
//     0x80,
//     0x00,
//     0x10,
//     0x00,
//     0x00,
//     0xEE,
//     0x00,
//     0x00,
//     0x00,
//     // second uuid, 32bit, [12], [13], [14], [15] is the value
//     0xfb,
//     0x34,
//     0x9b,
//     0x5f,
//     0x80,
//     0x00,
//     0x00,
//     0x80,
//     0x00,
//     0x10,
//     0x00,
//     0x00,
//     0xFF,
//     0x00,
//     0x00,
//     0x00,
// };

// static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
//     {NULL, ESP_GATT_IF_NONE, PROFILE_A_APP_ID, 0, 0, {}, 0, {}, ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ, 0, {}}};

// // ===== BLE Functions =====
// static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
// {
//     switch (event)
//     {
//     case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
//         if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
//         {
//             ESP_LOGE(GATTS_TAG, "Advertising start failed, status %d", param->adv_start_cmpl.status);
//             break;
//         }
//         ESP_LOGI(GATTS_TAG, "Advertising start successfully");
//         break;
//     case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
//         if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
//         {
//             ESP_LOGE(GATTS_TAG, "Advertising stop failed, status %d", param->adv_stop_cmpl.status);
//             break;
//         }
//         ESP_LOGI(GATTS_TAG, "Advertising stop successfully");
//         break;
//     case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
//         ESP_LOGI(GATTS_TAG, "Connection params update, status %d, conn_int %d, latency %d, timeout %d",
//                  param->update_conn_params.status,
//                  param->update_conn_params.conn_int,
//                  param->update_conn_params.latency,
//                  param->update_conn_params.timeout);
//         break;
//     default:
//         ESP_LOGD(GATTS_TAG, "Unhandled GAP event: %d", event);
//         break;
//     }
// }

// static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
// {
//     switch (event)
//     {
//     case ESP_GATTS_CONF_EVT:
//         // This event is triggered when client acknowledges the indication
//         if (param->conf.status == ESP_GATT_OK)
//         {
//             ESP_LOGI(GATTS_TAG, "Client acknowledged the indication");
//         }
//         else
//         {
//             ESP_LOGE(GATTS_TAG, "Client failed to acknowledge indication");
//         }
//         break;
//     case ESP_GATTS_REG_EVT:
//         ESP_LOGI(GATTS_TAG, "GATT server register, status %d, app_id %d, gatts_if %d",
//                  param->reg.status, param->reg.app_id, gatts_if);

//         // Set device name first
//         esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(device_name);
//         if (set_dev_name_ret)
//         {
//             ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
//         }
//         else
//         {
//             ESP_LOGI(GATTS_TAG, "Device name set to: %s", device_name);
//         }

//         // Configure advertising data
//         esp_ble_adv_data_t adv_data = {};
//         adv_data.set_scan_rsp = false;
//         adv_data.include_name = true;
//         adv_data.include_txpower = true;
//         adv_data.min_interval = 0x0006;
//         adv_data.max_interval = 0x0010;
//         adv_data.appearance = 0x00;
//         adv_data.manufacturer_len = 0;
//         adv_data.p_manufacturer_data = NULL;
//         adv_data.service_data_len = 0;
//         adv_data.p_service_data = NULL;
//         adv_data.service_uuid_len = 16;
//         adv_data.p_service_uuid = adv_service_uuid128;
//         adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

//         esp_ble_gap_config_adv_data(&adv_data);

//         // Create service
//         gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
//         gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
//         gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
//         gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;

//         esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE);
//         break;

//     case ESP_GATTS_READ_EVT:
//         ESP_LOGI(GATTS_TAG, "GATT read event");
//         break;

//     case ESP_GATTS_WRITE_EVT:
//         ESP_LOGI(GATTS_TAG, "GATT write event, handle %d", param->write.handle);
//         if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].descr_handle && param->write.len == 2)
//         {
//             uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
//             if (descr_value == 0x0001)
//             {
//                 ESP_LOGI(GATTS_TAG, "Notifications enabled");
//                 notifications_enabled = true;
//             }
//             else if (descr_value == 0x0000)
//             {
//                 ESP_LOGI(GATTS_TAG, "Notifications disabled");
//                 notifications_enabled = false;
//             }
//         }
//         break;

//     case ESP_GATTS_MTU_EVT:
//         ESP_LOGI(GATTS_TAG, "MTU exchange, MTU %d", param->mtu.mtu);
//         local_mtu = param->mtu.mtu;
//         break;

//     case ESP_GATTS_CREATE_EVT:
//         ESP_LOGI(GATTS_TAG, "Service created, handle %d", param->create.service_handle);
//         gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;

//         // Add characteristic
//         gl_profile_tab[PROFILE_A_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
//         gl_profile_tab[PROFILE_A_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID;

//         esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
//         esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
//                                &gl_profile_tab[PROFILE_A_APP_ID].char_uuid,
//                                ESP_GATT_PERM_READ,
//                                property, NULL, NULL);
//         break;

//     case ESP_GATTS_ADD_CHAR_EVT:
//         ESP_LOGI(GATTS_TAG, "Characteristic added, handle %d", param->add_char.attr_handle);
//         gl_profile_tab[PROFILE_A_APP_ID].char_handle = param->add_char.attr_handle;

//         // Add descriptor (CCCD)
//         gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
//         gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
//         esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
//                                      &gl_profile_tab[PROFILE_A_APP_ID].descr_uuid,
//                                      ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
//                                      NULL, NULL);
//         break;

//     case ESP_GATTS_ADD_CHAR_DESCR_EVT:
//         ESP_LOGI(GATTS_TAG, "Descriptor added, handle %d", param->add_char_descr.attr_handle);
//         gl_profile_tab[PROFILE_A_APP_ID].descr_handle = param->add_char_descr.attr_handle;

//         // Start service and advertising
//         esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);

//         esp_ble_adv_params_t adv_params = {
//             .adv_int_min = 0x20,
//             .adv_int_max = 0x40,
//             .adv_type = ADV_TYPE_IND,
//             .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
//             .channel_map = ADV_CHNL_ALL,
//             .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
//         };
//         esp_ble_gap_start_advertising(&adv_params);
//         break;

//     case ESP_GATTS_CONNECT_EVT:
//         ESP_LOGI(GATTS_TAG, "Client connected, conn_id %d", param->connect.conn_id);
//         current_conn_id = param->connect.conn_id;
//         current_gatts_if = gatts_if;
//         notifications_enabled = false;
//         break;

//     case ESP_GATTS_DISCONNECT_EVT:
//         ESP_LOGI(GATTS_TAG, "Client disconnected");
//         current_conn_id = 0xFFFF;
//         notifications_enabled = false;
//         // Restart advertising
//         esp_ble_adv_params_t adv_params_restart = {
//             .adv_int_min = 0x20,
//             .adv_int_max = 0x40,
//             .adv_type = ADV_TYPE_IND,
//             .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
//             .channel_map = ADV_CHNL_ALL,
//             .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
//         };
//         esp_ble_gap_start_advertising(&adv_params_restart);
//         break;

//     default:
//         ESP_LOGD(GATTS_TAG, "Unhandled GATT event: %d", event);
//         break;
//     }
// }

// static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
// {
//     if (event == ESP_GATTS_REG_EVT)
//     {
//         if (param->reg.status == ESP_GATT_OK)
//         {
//             gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
//             gl_profile_tab[param->reg.app_id].gatts_cb = gatts_profile_event_handler;
//         }
//     }

//     if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[PROFILE_A_APP_ID].gatts_if)
//     {
//         if (gl_profile_tab[PROFILE_A_APP_ID].gatts_cb)
//         {
//             gl_profile_tab[PROFILE_A_APP_ID].gatts_cb(event, gatts_if, param);
//         }
//     }
// }

// void ble_init(void)
// {
//     // Initialize NVS
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
//     {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);

//     // Initialize Bluetooth
//     ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

//     esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
//     ret = esp_bt_controller_init(&bt_cfg);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_bluedroid_init();
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_bluedroid_enable();
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
//         return;
//     }

//     // Register BLE callbacks
//     ret = esp_ble_gatts_register_callback(gatts_event_handler);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "GATTS register callback failed: %s", esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_ble_gap_register_callback(gap_event_handler);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "GAP register callback failed: %s", esp_err_to_name(ret));
//         return;
//     }

//     // Register GATT application
//     ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
//         return;
//     }

//     // Set larger MTU for IMU data
//     esp_ble_gatt_set_local_mtu(512);
// }

extern "C" void app_main(void)
{
    ESP_LOGI(IMUTAG, "Starting HandSense");

    const tflite::Model *model = tflite::GetModel(Lite_LSTM_Test_tflite);

    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        printf("Model schema version mismatch!\n");
        while (1)
            ;
    }

    static tflite::AllOpsResolver resolver;

    static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);

    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        printf("Tensor allocation failed!\n");
        while (1)
            ;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    printf("Model loaded successfully!\n");

    // ble_init();

    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(IMUTAG, "I2C initialized successfully");

    // Test I2C communication by scanning for devices
    i2c_scanner();

    // Initialize MPU6050 sensor
    if (mpu6050_init_both() != ESP_OK)
    {
        ESP_LOGE(IMUTAG, "Failed to initialize MPU6050 sensors");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    oled_init_simple();

    // Configure and initialize DRV2605
    drv2605_config_t drv_config = DRV2605_DEFAULT_CONFIG();
    drv_config.i2c_port = I2C_MASTER_NUM;
    drv_config.enable_pin = GPIO_NUM_5;

    drv2605_handle_t drv = drv2605_init(&drv_config);
    if (drv == NULL)
    {
        ESP_LOGE(HAPTICTAG, "Failed to initialize DRV2605");
        return;
    }

    ESP_ERROR_CHECK(adc_sensor_init(sensor_configs, 6));

    // Set calibration values for flex sensors
    flex_sensor_set_calibration(FLEX_1_ADC_CHANNEL, 1365, 1392, 450, 500);
    flex_sensor_set_calibration(FLEX_2_ADC_CHANNEL, 1175, 1200, 450, 500);
    flex_sensor_set_calibration(FLEX_3_ADC_CHANNEL, 1275, 1300, 450, 500);
    flex_sensor_set_calibration(FLEX_4_ADC_CHANNEL, 1250, 1265, 450, 500);
    flex_sensor_set_calibration(FLEX_5_ADC_CHANNEL, 1105, 1125, 450, 500);

    ESP_LOGI(IMUTAG, "All devices initialized, starting readings...");

    // Data variables
    float accel1_x, accel1_y, accel1_z, gyro1_x, gyro1_y, gyro1_z, temp1_f;
    float accel2_x, accel2_y, accel2_z, gyro2_x, gyro2_y, gyro2_z, temp2_f;

    while (1)
    {
        // Read IMU 1
        bool imu1_ok = (mpu6050_read_all_data(MPU6050_I2C_ADDR_1,
                                              &accel1_x, &accel1_y, &accel1_z,
                                              &gyro1_x, &gyro1_y, &gyro1_z,
                                              &temp1_f) == ESP_OK);

        // Read IMU 1
        bool imu2_ok = (mpu6050_read_all_data(MPU6050_I2C_ADDR_2,
                                              &accel2_x, &accel2_y, &accel2_z,
                                              &gyro2_x, &gyro2_y, &gyro2_z,
                                              &temp2_f) == ESP_OK);

        if (imu1_ok && imu2_ok)
        {
            // Log to serial
            ESP_LOGI(IMUTAG, "IMU1 Accel: X=%.2f, Y=%.2f, Z=%.2f m/s^2", accel1_x, accel1_y, accel1_z);
            ESP_LOGI(IMUTAG, "IMU1 Gyro:  X=%.2f, Y=%.2f, Z=%.2f deg/s", gyro1_x, gyro1_y, gyro1_z);
            ESP_LOGI(IMUTAG, "Temps: IMU1=%.1f°F", temp1_f);
            ESP_LOGI(IMUTAG, "IMU2 Accel: X=%.2f, Y=%.2f, Z=%.2f m/s^2", accel2_x, accel2_y, accel2_z);
            ESP_LOGI(IMUTAG, "IMU2 Gyro:  X=%.2f, Y=%.2f, Z=%.2f deg/s", gyro2_x, gyro2_y, gyro2_z);
            ESP_LOGI(IMUTAG, "Temps: IMU2=%.1f°F", temp2_f);
            oled_update_display(accel1_x, accel1_y, accel1_z,
                                accel2_x, accel2_y, accel2_z,
                                gyro1_x, gyro1_y, gyro1_z,
                                gyro2_x, gyro2_y, gyro2_z,
                                temp1_f, temp2_f);

            // vTaskDelay(pdMS_TO_TICKS(2000));
        }
        else
        {
            ESP_LOGE(IMUTAG, "Failed to read IMU");
        }

        // // Read all sensors
        // for (int i = 0; i < sizeof(sensor_configs) / sizeof(sensor_configs[0]); i++)
        // {
        //     adc_channel_t channel = sensor_configs[i].channel;
        //     const char *name = sensor_configs[i].name;
        //     bool is_touch = sensor_configs[i].is_touch_sensor;

        //     int raw = adc_sensor_read_raw(channel);
        //     int voltage = adc_sensor_read_voltage(channel);

        //     if (is_touch)
        //     {
        //         ESP_LOGI(ADCTAG, "%s: Raw: %d, Voltage: %d mV", name, raw, voltage);
        //     }
        //     else
        //     {
        //         float angle = flex_sensor_get_angle(channel);
        //         ESP_LOGI(ADCTAG, "%s: Raw: %d, Voltage: %d mV, Angle: %.1f°",
        //                  name, raw, voltage, angle);
        //     }
        // }

        // Read senor data frame - 20 values of 3-axis gyro
        for (int t = 0; t < WINDOW_SIZE; t++)
        {
            input->data.f[t * 3 + 0] = gyro1_x;
            input->data.f[t * 3 + 1] = gyro1_y;
            input->data.f[t * 3 + 2] = gyro1_z;
        }

        // Run model - If failed print and continue
        if (interpreter->Invoke() != kTfLiteOk)
        {
            printf("Model inference failed!\n");
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

        // // Send result to buffer
        // char ble_buffer[32];
        // memset(ble_buffer, 0, sizeof(ble_buffer));
        // strncpy(ble_buffer, predicted_gesture, sizeof(ble_buffer) - 1);

        // // Send to computer via BLE
        // if (notifications_enabled && current_conn_id != 0xFFFF)
        // {
        //     esp_err_t err = esp_ble_gatts_send_indicate(
        //         current_gatts_if,
        //         current_conn_id,
        //         gl_profile_tab[PROFILE_A_APP_ID].char_handle,
        //         strlen(ble_buffer),
        //         (uint8_t *)ble_buffer,
        //         false);
        //     if (err != ESP_OK)
        //     {
        //         ESP_LOGE(GATTS_TAG, "Failed to send data %s", esp_err_to_name(err));
        //     }
        // }

        // ESP_LOGI(HAPTICTAG, "Playing long buzz");
        // drv2605_play_effect(drv, DRV2605_EFFECT_LONG_BUZZ);

        ESP_LOGI(IMUTAG, "---");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    drv2605_deinit(drv);
}