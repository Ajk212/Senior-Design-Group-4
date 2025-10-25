#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c.h"
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

#include "imu_ble.h"

// I2C Configuration
#define I2C_MASTER_SCL_IO 1
#define I2C_MASTER_SDA_IO 2
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

// MPU-6050 Addresses
#define MPU6050_I2C_ADDR_1 0x68
#define MPU6050_I2C_ADDR_2 0x69

// MPU-6050 Register Map
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H 0x43
#define MPU6050_REG_WHO_AM_I 0x75

// BLE Configuration
#define GATTS_TAG "GATTS_BLE"
#define IMUTAG "MPU6050"

#define GATTS_SERVICE_UUID 0x00FF
#define GATTS_CHAR_UUID 0xFF01
#define GATTS_DESCR_UUID 0x3333
#define GATTS_NUM_HANDLE 4

static char device_name[ESP_BLE_ADV_NAME_LEN_MAX] = CONFIG_HANDSENSE_DEVICE_NAME;

#define PROFILE_NUM 1
#define PROFILE_A_APP_ID 0

// Global BLE variables
static uint16_t current_conn_id = 0xFFFF;
static uint16_t current_gatts_if = ESP_GATT_IF_NONE;
static bool notifications_enabled = false;
static uint16_t local_mtu = 23;

// IMU data structure
static imu_frame_t imu_data = {0};
static uint32_t current_frame_count = 0;

// BLE profile structure
struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static uint8_t adv_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // first uuid, 16bit, [12],[13] is the value
    0xfb,
    0x34,
    0x9b,
    0x5f,
    0x80,
    0x00,
    0x00,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0xEE,
    0x00,
    0x00,
    0x00,
    // second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb,
    0x34,
    0x9b,
    0x5f,
    0x80,
    0x00,
    0x00,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0xFF,
    0x00,
    0x00,
    0x00,
};

static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = NULL, // Will be set in event handler
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = PROFILE_A_APP_ID,
        .conn_id = 0,
        .service_handle = 0,
        .char_handle = 0,
        .descr_handle = 0},
};

// ===== IMU Functions =====
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf;
    memset(&conf, 0, sizeof(i2c_config_t));
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
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

static esp_err_t mpu6050_register_write(uint8_t sensor_addr, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, sensor_addr,
                                      write_buf, sizeof(write_buf),
                                      pdMS_TO_TICKS(1000));
}

static esp_err_t mpu6050_register_read(uint8_t sensor_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, sensor_addr,
                                        &reg_addr, 1, data, len,
                                        pdMS_TO_TICKS(1000));
}

static esp_err_t mpu6050_init_sensor(uint8_t sensor_addr, const char *sensor_name)
{
    ESP_LOGI(IMUTAG, "Initializing %s at address 0x%02X", sensor_name, sensor_addr);

    esp_err_t ret = mpu6050_register_write(sensor_addr, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK)
    {
        ESP_LOGE(IMUTAG, "Failed to initialize %s! Error: 0x%x", sensor_name, ret);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t who_am_i;
    ret = mpu6050_register_read(sensor_addr, MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(IMUTAG, "Failed to read from %s! Error: 0x%x", sensor_name, ret);
        return ret;
    }

    ESP_LOGI(IMUTAG, "%s WHO_AM_I = 0x%02X", sensor_name, who_am_i);

    if (who_am_i != 0x68)
    {
        ESP_LOGE(IMUTAG, "%s not responding correctly!", sensor_name);
        return ESP_FAIL;
    }

    ESP_LOGI(IMUTAG, "%s initialized successfully", sensor_name);
    return ESP_OK;
}

static esp_err_t mpu6050_init_both(void)
{
    esp_err_t ret1 = mpu6050_init_sensor(MPU6050_I2C_ADDR_1, "IMU_1");
    esp_err_t ret2 = mpu6050_init_sensor(MPU6050_I2C_ADDR_2, "IMU_2");
    return (ret1 == ESP_OK && ret2 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

static esp_err_t mpu6050_read_accelerometer_sensor(uint8_t sensor_addr, float *ax, float *ay, float *az)
{
    uint8_t data[6];
    esp_err_t ret = mpu6050_register_read(sensor_addr, MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data));

    if (ret == ESP_OK)
    {
        int16_t accel_x = (data[0] << 8) | data[1];
        int16_t accel_y = (data[2] << 8) | data[3];
        int16_t accel_z = (data[4] << 8) | data[5];

        float ax_g = accel_x / 16384.0;
        float ay_g = accel_y / 16384.0;
        float az_g = accel_z / 16384.0;

        *ax = ax_g * 9.80665;
        *ay = ay_g * 9.80665;
        *az = az_g * 9.80665;
    }
    return ret;
}

static esp_err_t mpu6050_read_gyroscope_sensor(uint8_t sensor_addr, float *gx, float *gy, float *gz)
{
    uint8_t data[6];
    esp_err_t ret = mpu6050_register_read(sensor_addr, MPU6050_REG_GYRO_XOUT_H, data, sizeof(data));

    if (ret == ESP_OK)
    {
        int16_t gyro_x = (data[0] << 8) | data[1];
        int16_t gyro_y = (data[2] << 8) | data[3];
        int16_t gyro_z = (data[4] << 8) | data[5];

        *gx = gyro_x / 131.0;
        *gy = gyro_y / 131.0;
        *gz = gyro_z / 131.0;
    }
    return ret;
}

static esp_err_t mpu6050_read_temperature_sensor(uint8_t sensor_addr, float *temp_f)
{
    uint8_t data[2];
    esp_err_t ret = mpu6050_register_read(sensor_addr, 0x41, data, sizeof(data));

    if (ret == ESP_OK)
    {
        int16_t temp_raw = (data[0] << 8) | data[1];
        float temp_c = (temp_raw / 340.0) + 36.53;
        *temp_f = (temp_c * 9.0 / 5.0) + 32.0;
    }
    return ret;
}

// ===== BLE Functions =====
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TAG, "Advertising start failed, status %d", param->adv_start_cmpl.status);
            break;
        }
        ESP_LOGI(GATTS_TAG, "Advertising start successfully");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TAG, "Advertising stop failed, status %d", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GATTS_TAG, "Advertising stop successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(GATTS_TAG, "Connection params update, status %d, conn_int %d, latency %d, timeout %d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "GATT server register, status %d, app_id %d, gatts_if %d",
                 param->reg.status, param->reg.app_id, gatts_if);

        // Set device name first
        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(device_name);
        if (set_dev_name_ret)
        {
            ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        else
        {
            ESP_LOGI(GATTS_TAG, "Device name set to: %s", device_name);
        }

        // Configure advertising data
        esp_ble_adv_data_t adv_data = {
            .set_scan_rsp = false,
            .include_name = true,
            .include_txpower = false,
            .min_interval = 0x0006, // slave connection min interval, Time = min_interval * 1.25 msec
            .max_interval = 0x0010, // slave connection max interval, Time = max_interval * 1.25 msec
            .appearance = 0x00,
            .manufacturer_len = 0,
            .p_manufacturer_data = NULL,
            .service_data_len = 0,
            .p_service_data = NULL,
            .service_uuid_len = sizeof(adv_service_uuid128),
            .p_service_uuid = adv_service_uuid128,
            .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
        };

        esp_ble_gap_config_adv_data(&adv_data);

        // Create service
        gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE);
        break;

    case ESP_GATTS_READ_EVT:
        ESP_LOGI(GATTS_TAG, "GATT read event");
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(GATTS_TAG, "GATT write event, handle %d", param->write.handle);
        if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].descr_handle && param->write.len == 2)
        {
            uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
            if (descr_value == 0x0001)
            {
                ESP_LOGI(GATTS_TAG, "Notifications enabled");
                notifications_enabled = true;
            }
            else if (descr_value == 0x0000)
            {
                ESP_LOGI(GATTS_TAG, "Notifications disabled");
                notifications_enabled = false;
            }
        }
        break;

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(GATTS_TAG, "MTU exchange, MTU %d", param->mtu.mtu);
        local_mtu = param->mtu.mtu;
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "Service created, handle %d", param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;

        // Add characteristic
        gl_profile_tab[PROFILE_A_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID;

        esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
                               &gl_profile_tab[PROFILE_A_APP_ID].char_uuid,
                               ESP_GATT_PERM_READ,
                               property, NULL, NULL);
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(GATTS_TAG, "Characteristic added, handle %d", param->add_char.attr_handle);
        gl_profile_tab[PROFILE_A_APP_ID].char_handle = param->add_char.attr_handle;

        // Add descriptor (CCCD)
        gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
                                     &gl_profile_tab[PROFILE_A_APP_ID].descr_uuid,
                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                     NULL, NULL);
        break;

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(GATTS_TAG, "Descriptor added, handle %d", param->add_char_descr.attr_handle);
        gl_profile_tab[PROFILE_A_APP_ID].descr_handle = param->add_char_descr.attr_handle;

        // Start service and advertising
        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);

        esp_ble_adv_params_t adv_params = {
            .adv_int_min = 0x20,
            .adv_int_max = 0x40,
            .adv_type = ADV_TYPE_IND,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL,
            .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        };
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client connected, conn_id %d", param->connect.conn_id);
        current_conn_id = param->connect.conn_id;
        current_gatts_if = gatts_if;
        notifications_enabled = false;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client disconnected");
        current_conn_id = 0xFFFF;
        notifications_enabled = false;
        // Restart advertising
        esp_ble_adv_params_t adv_params_restart = {
            .adv_int_min = 0x20,
            .adv_int_max = 0x40,
            .adv_type = ADV_TYPE_IND,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL,
            .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        };
        esp_ble_gap_start_advertising(&adv_params_restart);
        break;

    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
            gl_profile_tab[param->reg.app_id].gatts_cb = gatts_profile_event_handler;
        }
    }

    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[PROFILE_A_APP_ID].gatts_if)
    {
        if (gl_profile_tab[PROFILE_A_APP_ID].gatts_cb)
        {
            gl_profile_tab[PROFILE_A_APP_ID].gatts_cb(event, gatts_if, param);
        }
    }
}

// ===== Main Application Task =====
static void imu_ble_task(void *pvParameters)
{
    ESP_LOGI(IMUTAG, "Starting IMU + BLE task");

    // Data variables
    float accel1_x, accel1_y, accel1_z, gyro1_x, gyro1_y, gyro1_z, temp1_f;
    float accel2_x, accel2_y, accel2_z, gyro2_x, gyro2_y, gyro2_z, temp2_f;

    while (1)
    {
        // Read IMU 1
        bool imu1_ok = (mpu6050_read_accelerometer_sensor(MPU6050_I2C_ADDR_1, &accel1_x, &accel1_y, &accel1_z) == ESP_OK &&
                        mpu6050_read_gyroscope_sensor(MPU6050_I2C_ADDR_1, &gyro1_x, &gyro1_y, &gyro1_z) == ESP_OK &&
                        mpu6050_read_temperature_sensor(MPU6050_I2C_ADDR_1, &temp1_f) == ESP_OK);

        // Read IMU 2
        bool imu2_ok = (mpu6050_read_accelerometer_sensor(MPU6050_I2C_ADDR_2, &accel2_x, &accel2_y, &accel2_z) == ESP_OK &&
                        mpu6050_read_gyroscope_sensor(MPU6050_I2C_ADDR_2, &gyro2_x, &gyro2_y, &gyro2_z) == ESP_OK &&
                        mpu6050_read_temperature_sensor(MPU6050_I2C_ADDR_2, &temp2_f) == ESP_OK);

        if (imu1_ok && imu2_ok)
        {
            // Update IMU data structure
            current_frame_count++;

            // IMU 1 Data
            imu_data.accel1_x = accel1_x;
            imu_data.accel1_y = accel1_y;
            imu_data.accel1_z = accel1_z;
            imu_data.gyro1_x = gyro1_x;
            imu_data.gyro1_y = gyro1_y;
            imu_data.gyro1_z = gyro1_z;
            imu_data.temp1 = temp1_f;

            // IMU 2 Data
            imu_data.accel2_x = accel2_x;
            imu_data.accel2_y = accel2_y;
            imu_data.accel2_z = accel2_z;
            imu_data.gyro2_x = gyro2_x;
            imu_data.gyro2_y = gyro2_y;
            imu_data.gyro2_z = gyro2_z;
            imu_data.temp2 = temp2_f;

            // Metadata
            imu_data.timestamp = esp_log_timestamp();
            imu_data.frame_count = current_frame_count;
            imu_data.status = 1; // Good status

            // Send via BLE if notifications are enabled and connected
            if (notifications_enabled && current_conn_id != 0xFFFF)
            {
                esp_err_t err = esp_ble_gatts_send_indicate(
                    current_gatts_if,
                    current_conn_id,
                    gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                    IMU_FRAME_SIZE,
                    (uint8_t *)&imu_data,
                    false // Notification (not indication)
                );

                if (err == ESP_OK)
                {
                    ESP_LOGI(GATTS_TAG, "Sent IMU data frame %" PRIu32, current_frame_count);
                }
                else
                {
                    ESP_LOGE(GATTS_TAG, "Failed to send IMU data: %s", esp_err_to_name(err));
                }
            }
        }
        else
        {
            ESP_LOGE(IMUTAG, "Failed to read one or both IMUs");
            imu_data.status = 0; // Error status
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // 10Hz update rate
    }
}

void app_main(void)
{
    ESP_LOGI("MAIN", "Starting Dual MPU6050 + BLE Application");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(IMUTAG, "I2C initialized successfully");

    // Initialize MPU6050 sensors
    if (mpu6050_init_both() != ESP_OK)
    {
        ESP_LOGE(IMUTAG, "Failed to initialize MPU6050 sensors");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(IMUTAG, "Both IMU sensors initialized successfully");

    // Initialize Bluetooth
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register BLE callbacks
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "GATTS register callback failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "GAP register callback failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register GATT application
    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return;
    }

    // Set larger MTU for IMU data
    esp_ble_gatt_set_local_mtu(512);

    // Start IMU + BLE task
    xTaskCreate(imu_ble_task, "imu_ble_task", 4096, NULL, 5, NULL);

    ESP_LOGI("MAIN", "Application started successfully");
}