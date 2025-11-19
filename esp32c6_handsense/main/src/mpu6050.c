#include "mpu6050.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "oled.h"

static const char *TAG = "MPU6050";

static sensor_config_t sensor1_config = {ACCEL_FS_SEL_2G, GYRO_FS_SEL_250DPS, DLPF_CFG_5};
static sensor_config_t sensor2_config = {ACCEL_FS_SEL_2G, GYRO_FS_SEL_250DPS, DLPF_CFG_5};

// Write to MPU6050 register
static esp_err_t mpu6050_register_write(uint8_t sensor_addr, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, sensor_addr,
                                      write_buf, sizeof(write_buf),
                                      pdMS_TO_TICKS(1000));
}

// Read from MPU6050 register
static esp_err_t mpu6050_register_read(uint8_t sensor_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, sensor_addr,
                                        &reg_addr, 1, data, len,
                                        pdMS_TO_TICKS(1000));
}

// Initialize MPU6050 sensor
esp_err_t mpu6050_init_sensor(uint8_t sensor_addr, const char *sensor_name)
{
    ESP_LOGI(TAG, "Initializing %s at address 0x%02X", sensor_name, sensor_addr);

    esp_err_t ret = mpu6050_register_write(sensor_addr, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize %s! Error: 0x%x", sensor_name, ret);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t who_am_i;
    ret = mpu6050_register_read(sensor_addr, MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read from %s! Error: 0x%x", sensor_name, ret);
        return ret;
    }

    ESP_LOGI(TAG, "%s WHO_AM_I = 0x%02X", sensor_name, who_am_i);

    if (who_am_i != 0x68 && who_am_i != 0x70)
    {
        ESP_LOGE(TAG, "%s not responding correctly!", sensor_name);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s initialized successfully", sensor_name);
    return ESP_OK;
}

// Initialize both MPU6050 sensors
esp_err_t mpu6050_init_both(void)
{
    esp_err_t ret1 = mpu6050_init_sensor(MPU6050_I2C_ADDR_1, "Main IMU");
    esp_err_t ret2 = mpu6050_init_sensor(MPU6050_I2C_ADDR_2, "Index IMU");
    return (ret1 == ESP_OK && ret2 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

// Configure DLPF and full scale ranges
static esp_err_t mpu6050_configure_sensor(uint8_t sensor_addr, uint8_t dlpf_cfg,
                                          uint8_t accel_fs_sel, uint8_t gyro_fs_sel)
{
    esp_err_t ret;

    // Configure DLPF (Digital Low Pass Filter)
    ret = mpu6050_register_write(sensor_addr, MPU6050_REG_CONFIG, dlpf_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure DLPF! Error: 0x%x", ret);
        return ret;
    }

    // Configure accelerometer full scale range
    ret = mpu6050_register_write(sensor_addr, MPU6050_REG_ACCEL_CONFIG, accel_fs_sel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure accelerometer! Error: 0x%x", ret);
        return ret;
    }

    // Configure gyroscope full scale range
    ret = mpu6050_register_write(sensor_addr, MPU6050_REG_GYRO_CONFIG, gyro_fs_sel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure gyroscope! Error: 0x%x", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Sensor configured - DLPF: 0x%02X, Accel FS: 0x%02X, Gyro FS: 0x%02X",
             dlpf_cfg, accel_fs_sel, gyro_fs_sel);

    return ESP_OK;
}

esp_err_t mpu6050_init_sensor_advanced(uint8_t sensor_addr, const char *sensor_name,
                                       uint8_t dlpf_cfg, uint8_t accel_fs_sel, uint8_t gyro_fs_sel)
{
    ESP_LOGI(TAG, "Initializing %s at address 0x%02X with advanced config", sensor_name, sensor_addr);

    // Wake up the sensor
    esp_err_t ret = mpu6050_register_write(sensor_addr, MPU6050_REG_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize %s! Error: 0x%x", sensor_name, ret);
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify device identity
    uint8_t who_am_i;
    ret = mpu6050_register_read(sensor_addr, MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read from %s! Error: 0x%x", sensor_name, ret);
        return ret;
    }
    ESP_LOGI(TAG, "%s WHO_AM_I = 0x%02X", sensor_name, who_am_i);

    if (who_am_i != 0x68 && who_am_i != 0x70)
    {
        ESP_LOGE(TAG, "%s not responding correctly!", sensor_name);
        return ESP_FAIL;
    }

    // Configure sensor with custom settings
    ret = mpu6050_configure_sensor(sensor_addr, dlpf_cfg, accel_fs_sel, gyro_fs_sel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure %s!", sensor_name);
        return ret;
    }

    sensor_config_t *config = (sensor_addr == MPU6050_I2C_ADDR_1) ? &sensor1_config : &sensor2_config;
    config->accel_fs_sel = accel_fs_sel;
    config->gyro_fs_sel = gyro_fs_sel;
    config->dlpf_cfg = dlpf_cfg;

    ESP_LOGI(TAG, "%s initialized successfully with custom DLPF", sensor_name);
    return ESP_OK;
}

esp_err_t mpu6050_init_both_advanced(uint8_t dlpf_cfg, uint8_t accel_fs_sel, uint8_t gyro_fs_sel)
{
    esp_err_t ret1 = mpu6050_init_sensor_advanced(MPU6050_I2C_ADDR_1, "IMU_1",
                                                  dlpf_cfg, accel_fs_sel, gyro_fs_sel);
    esp_err_t ret2 = mpu6050_init_sensor_advanced(MPU6050_I2C_ADDR_2, "IMU_2",
                                                  dlpf_cfg, accel_fs_sel, gyro_fs_sel);
    return (ret1 == ESP_OK && ret2 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

static float get_accel_sensitivity(uint8_t accel_fs_sel)
{
    switch (accel_fs_sel)
    {
    case ACCEL_FS_SEL_2G:
        return 16384.0; // LSB/g
    case ACCEL_FS_SEL_4G:
        return 8192.0;
    case ACCEL_FS_SEL_8G:
        return 4096.0;
    case ACCEL_FS_SEL_16G:
        return 2048.0;
    default:
        return 16384.0; // Default to ±2g
    }
}

// Read accelerometer data
esp_err_t mpu6050_read_accelerometer(uint8_t sensor_addr, float *ax, float *ay, float *az)
{
    uint8_t data[6];
    esp_err_t ret = mpu6050_register_read(sensor_addr, MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data));

    if (ret == ESP_OK)
    {
        int16_t accel_x = (data[0] << 8) | data[1];
        int16_t accel_y = (data[2] << 8) | data[3];
        int16_t accel_z = (data[4] << 8) | data[5];

        // Get the configuration for this sensor
        sensor_config_t *config = (sensor_addr == MPU6050_I2C_ADDR_1) ? &sensor1_config : &sensor2_config;
        float sensitivity = get_accel_sensitivity(config->accel_fs_sel);

        float ax_g = accel_x / sensitivity;
        float ay_g = accel_y / sensitivity;
        float az_g = accel_z / sensitivity;

        *ax = ax_g * 9.80665;
        *ay = ay_g * 9.80665;
        *az = az_g * 9.80665;
    }
    return ret;
}

static float get_gyro_sensitivity(uint8_t gyro_fs_sel)
{
    switch (gyro_fs_sel)
    {
    case GYRO_FS_SEL_250DPS:
        return 131.0; // LSB/°/s
    case GYRO_FS_SEL_500DPS:
        return 65.5;
    case GYRO_FS_SEL_1000DPS:
        return 32.8;
    case GYRO_FS_SEL_2000DPS:
        return 16.4;
    default:
        return 131.0; // Default to ±250°/s
    }
}

// Read gyroscope data with configurable scaling
esp_err_t mpu6050_read_gyroscope(uint8_t sensor_addr, float *gx, float *gy, float *gz)
{
    uint8_t data[6];
    esp_err_t ret = mpu6050_register_read(sensor_addr, MPU6050_REG_GYRO_XOUT_H, data, sizeof(data));

    if (ret == ESP_OK)
    {
        int16_t gyro_x = (data[0] << 8) | data[1];
        int16_t gyro_y = (data[2] << 8) | data[3];
        int16_t gyro_z = (data[4] << 8) | data[5];

        // Get the configuration for this sensor
        sensor_config_t *config = (sensor_addr == MPU6050_I2C_ADDR_1) ? &sensor1_config : &sensor2_config;
        float sensitivity = get_gyro_sensitivity(config->gyro_fs_sel);

        *gx = gyro_x / sensitivity;
        *gy = gyro_y / sensitivity;
        *gz = gyro_z / sensitivity;
    }
    return ret;
}

// Read temperature data
esp_err_t mpu6050_read_temperature(uint8_t sensor_addr, float *temp_f)
{
    uint8_t data[2];
    esp_err_t ret = mpu6050_register_read(sensor_addr, MPU6050_REG_TEMP_OUT_H, data, sizeof(data));

    if (ret == ESP_OK)
    {
        int16_t temp_raw = (data[0] << 8) | data[1];
        float temp_c = (temp_raw / 340.0) + 36.53;
        *temp_f = (temp_c * 9.0 / 5.0) + 32.0;
    }
    return ret;
}

// Read all sensor data
esp_err_t mpu6050_read_all_data(uint8_t sensor_addr,
                                float *ax, float *ay, float *az,
                                float *gx, float *gy, float *gz,
                                float *temp_f)
{
    esp_err_t ret_accel = mpu6050_read_accelerometer(sensor_addr, ax, ay, az);
    esp_err_t ret_gyro = mpu6050_read_gyroscope(sensor_addr, gx, gy, gz);
    esp_err_t ret_temp = mpu6050_read_temperature(sensor_addr, temp_f);

    return (ret_accel == ESP_OK && ret_gyro == ESP_OK && ret_temp == ESP_OK) ? ESP_OK : ESP_FAIL;
}

// Calibrate sensors by collecting data while stationary
esp_err_t mpu6050_calibrate_both_sensor(calibration_data_t *calib_1, calibration_data_t *calib_2)
{
    ESP_LOGI(TAG, "Starting calibration - keep sensor stationary");

    char buf[21];
    snprintf(buf, sizeof(buf), "Keep glove at rest");
    oled_print_text(5, 0, buf);

    float a1x_sum = 0, a1y_sum = 0, a1z_sum = 0;
    float g1x_sum = 0, g1y_sum = 0, g1z_sum = 0;
    float a2x_sum = 0, a2y_sum = 0, a2z_sum = 0;
    float g2x_sum = 0, g2y_sum = 0, g2z_sum = 0;
    const uint32_t num_samples = 500;

    for (int i = 0; i < num_samples; i++)
    {
        float a1x, a1y, a1z, g1x, g1y, g1z, temp1;
        float a2x, a2y, a2z, g2x, g2y, g2z, temp2;
        if (mpu6050_read_all_data(MPU6050_I2C_ADDR_1, &a1x, &a1y, &a1z, &g1x, &g1y, &g1z, &temp1) == ESP_OK)
        {
            a1x_sum += a1x;
            a1y_sum += a1y;
            a1z_sum += a1z;
            g1x_sum += g1x;
            g1y_sum += g1y;
            g1z_sum += g1z;
        }
        if (mpu6050_read_all_data(MPU6050_I2C_ADDR_2, &a2x, &a2y, &a2z, &g2x, &g2y, &g2z, &temp2) == ESP_OK)
        {
            a2x_sum += a2x;
            a2y_sum += a2y;
            a2z_sum += a2z;
            g2x_sum += g2x;
            g2y_sum += g2y;
            g2z_sum += g2z;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Calculate biases (assuming sensor should read 0 for gyro, 9.8m/s² for Z accelerometer)
    calib_1->gyro_bias[0] = g1x_sum / num_samples;
    calib_1->gyro_bias[1] = g1y_sum / num_samples;
    calib_1->gyro_bias[2] = g1z_sum / num_samples;

    calib_1->accel_bias[0] = a1x_sum / num_samples;
    calib_1->accel_bias[1] = a1y_sum / num_samples;
    calib_1->accel_bias[2] = (a1z_sum / num_samples) - 9.80665; // Remove gravity

    calib_2->gyro_bias[0] = g2x_sum / num_samples;
    calib_2->gyro_bias[1] = g2y_sum / num_samples;
    calib_2->gyro_bias[2] = g2z_sum / num_samples;

    calib_2->accel_bias[0] = a2x_sum / num_samples;
    calib_2->accel_bias[1] = a2y_sum / num_samples;
    calib_2->accel_bias[2] = (a2z_sum / num_samples) - 9.80665; // Remove gravity

    calib_1->sample_count = num_samples;
    calib_2->sample_count = num_samples;

    snprintf(buf, sizeof(buf), "Calibration complete");
    oled_print_text(5, 0, buf);

    ESP_LOGI(TAG, "Calibration complete");
    ESP_LOGI(TAG, "Accel bias for Sensor 1: %.3f, %.3f, %.3f",
             calib_1->accel_bias[0], calib_1->accel_bias[1], calib_1->accel_bias[2]);
    ESP_LOGI(TAG, "Gyro bias for Sensor 1: %.3f, %.3f, %.3f",
             calib_1->gyro_bias[0], calib_1->gyro_bias[1], calib_1->gyro_bias[2]);
    ESP_LOGI(TAG, "Accel bias for Sensor 2: %.3f, %.3f, %.3f",
             calib_2->accel_bias[0], calib_2->accel_bias[1], calib_2->accel_bias[2]);
    ESP_LOGI(TAG, "Gyro bias for Sensor 2: %.3f, %.3f, %.3f",
             calib_2->gyro_bias[0], calib_2->gyro_bias[1], calib_2->gyro_bias[2]);

    return ESP_OK;
}

// Apply calibration to raw data
void apply_calibration(imu_data_t *data, const calibration_data_t *calib)
{
    data->ax -= calib->accel_bias[0];
    data->ay -= calib->accel_bias[1];
    data->az -= calib->accel_bias[2];

    data->gx -= calib->gyro_bias[0];
    data->gy -= calib->gyro_bias[1];
    data->gz -= calib->gyro_bias[2];
}