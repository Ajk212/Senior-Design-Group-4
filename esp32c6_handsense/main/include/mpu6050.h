#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// I2C Configuration
#define I2C_MASTER_NUM I2C_NUM_0

// MPU-6050 Addresses
#define MPU6050_I2C_ADDR_1 0x68
#define MPU6050_I2C_ADDR_2 0x69

// MPU-6050 Register Map
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H 0x43
#define MPU6050_REG_WHO_AM_I 0x75
#define MPU6050_REG_TEMP_OUT_H 0x41

// Function prototypes

/**
 * @brief Initialize I2C master for MPU6050 communication
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t mpu6050_i2c_master_init(void);

/**
 * @brief Initialize a single MPU6050 sensor
 * @param sensor_addr I2C address of the sensor
 * @param sensor_name Descriptive name for logging
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t mpu6050_init_sensor(uint8_t sensor_addr, const char *sensor_name);

/**
 * @brief Initialize both MPU6050 sensors
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t mpu6050_init_both(void);

/**
 * @brief Read accelerometer data from MPU6050
 * @param sensor_addr I2C address of the sensor
 * @param ax Pointer to store X-axis acceleration (m/s²)
 * @param ay Pointer to store Y-axis acceleration (m/s²)
 * @param az Pointer to store Z-axis acceleration (m/s²)
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t mpu6050_read_accelerometer(uint8_t sensor_addr, float *ax, float *ay, float *az);

/**
 * @brief Read gyroscope data from MPU6050
 * @param sensor_addr I2C address of the sensor
 * @param gx Pointer to store X-axis gyroscope (deg/s)
 * @param gy Pointer to store Y-axis gyroscope (deg/s)
 * @param gz Pointer to store Z-axis gyroscope (deg/s)
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t mpu6050_read_gyroscope(uint8_t sensor_addr, float *gx, float *gy, float *gz);

/**
 * @brief Read temperature data from MPU6050
 * @param sensor_addr I2C address of the sensor
 * @param temp_f Pointer to store temperature in Fahrenheit
 * @return esp_err_t ESP_OK on success, error code on failure
 */
esp_err_t mpu6050_read_temperature(uint8_t sensor_addr, float *temp_f);

/**
 * @brief Read all sensor data from MPU6050 (accelerometer, gyroscope, temperature)
 * @param sensor_addr I2C address of the sensor
 * @param ax Pointer to store X-axis acceleration (m/s²)
 * @param ay Pointer to store Y-axis acceleration (m/s²)
 * @param az Pointer to store Z-axis acceleration (m/s²)
 * @param gx Pointer to store X-axis gyroscope (deg/s)
 * @param gy Pointer to store Y-axis gyroscope (deg/s)
 * @param gz Pointer to store Z-axis gyroscope (deg/s)
 * @param temp_f Pointer to store temperature in Fahrenheit
 * @return esp_err_t ESP_OK if all reads successful, error code on failure
 */
esp_err_t mpu6050_read_all_data(uint8_t sensor_addr,
                                float *ax, float *ay, float *az,
                                float *gx, float *gy, float *gz,
                                float *temp_f);

#endif // MPU6050_H