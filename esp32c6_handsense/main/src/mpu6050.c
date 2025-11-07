#include "mpu6050.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "MPU6050";

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

    if (who_am_i != 0x68)
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
    esp_err_t ret1 = mpu6050_init_sensor(MPU6050_I2C_ADDR_1, "IMU_1");
    esp_err_t ret2 = mpu6050_init_sensor(MPU6050_I2C_ADDR_2, "IMU_2");
    return (ret1 == ESP_OK && ret2 == ESP_OK) ? ESP_OK : ESP_FAIL;
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

        float ax_g = accel_x / 16384.0;
        float ay_g = accel_y / 16384.0;
        float az_g = accel_z / 16384.0;

        *ax = ax_g * 9.80665;
        *ay = ay_g * 9.80665;
        *az = az_g * 9.80665;
    }
    return ret;
}

// Read gyroscope data
esp_err_t mpu6050_read_gyroscope(uint8_t sensor_addr, float *gx, float *gy, float *gz)
{
    uint8_t data[6];
    esp_err_t ret = mpu6050_register_read(sensor_addr, MPU6050_REG_GYRO_XOUT_H, data, sizeof(data));

    if (ret == ESP_OK)
    {
        int16_t gyro_x = (data[0] << 8) | data[1];
        int16_t gyro_y = (data[2] << 8) | data[3];
        int16_t gyro_z = (data[4] << 8) | data[5];

        *gx = gyro_x / 65.5;
        *gy = gyro_y / 65.5;
        *gz = gyro_z / 65.5;
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