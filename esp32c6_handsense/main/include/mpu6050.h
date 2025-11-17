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
#define MPU6050_REG_CONFIG 0x1A
#define MPU6050_REG_GYRO_CONFIG 0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H 0x43
#define MPU6050_REG_WHO_AM_I 0x75
#define MPU6050_REG_TEMP_OUT_H 0x41

// DLPF_CFG modes
#define DLPF_CFG_0 0x00 // Accel: 260Hz, Gyro: 256Hz, Delay: 0ms
#define DLPF_CFG_1 0x01 // Accel: 184Hz, Gyro: 188Hz, Delay: 2.0ms
#define DLPF_CFG_2 0x02 // Accel: 94Hz,  Gyro: 98Hz,  Delay: 3.0ms
#define DLPF_CFG_3 0x03 // Accel: 44Hz,  Gyro: 42Hz,  Delay: 4.9ms
#define DLPF_CFG_4 0x04 // Accel: 21Hz,  Gyro: 20Hz,  Delay: 8.5ms
#define DLPF_CFG_5 0x05 // Accel: 10Hz,  Gyro: 10Hz,  Delay: 13.8ms
#define DLPF_CFG_6 0x06 // Accel: 5Hz,   Gyro: 5Hz,   Delay: 19.0ms

// Full scale ranges
#define ACCEL_FS_SEL_2G 0x00  // ±2g
#define ACCEL_FS_SEL_4G 0x08  // ±4g
#define ACCEL_FS_SEL_8G 0x10  // ±8g
#define ACCEL_FS_SEL_16G 0x18 // ±16g

#define GYRO_FS_SEL_250DPS 0x00  // ±250°/s
#define GYRO_FS_SEL_500DPS 0x08  // ±500°/s
#define GYRO_FS_SEL_1000DPS 0x10 // ±1000°/s
#define GYRO_FS_SEL_2000DPS 0x18 // ±2000°/s

// Data structures
typedef struct
{
    uint8_t accel_fs_sel;
    uint8_t gyro_fs_sel;
    uint8_t dlpf_cfg;
} sensor_config_t;

typedef struct
{
    float ax, ay, az;
    float gx, gy, gz;
    float temperature;
    uint32_t timestamp;
} imu_data_t;

typedef struct
{
    float accel_bias[3];
    float gyro_bias[3];
    float accel_scale[3];
    uint32_t sample_count;
} calibration_data_t;

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

esp_err_t mpu6050_init_both_advanced(uint8_t dlpf_cfg, uint8_t accel_fs_sel, uint8_t gyro_fs_sel);
esp_err_t mpu6050_init_sensor_advanced(uint8_t sensor_addr, const char *sensor_name,
                                       uint8_t dlpf_cfg, uint8_t accel_fs_sel, uint8_t gyro_fs_sel);
esp_err_t mpu6050_calibrate_both_sensor(calibration_data_t *calib_1, calibration_data_t *calib_2);
void apply_calibration(imu_data_t *data, const calibration_data_t *calib);

#endif // MPU6050_H