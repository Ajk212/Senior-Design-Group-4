#ifndef IMU_BLE_COMMON_H
#define IMU_BLE_COMMON_H

#include <stdint.h>

// IMU Frame structure for BLE transmission
#pragma pack(push, 1)
typedef struct
{
    // IMU 1 Data
    float accel1_x, accel1_y, accel1_z;
    float gyro1_x, gyro1_y, gyro1_z;
    float temp1;

    // IMU 2 Data
    float accel2_x, accel2_y, accel2_z;
    float gyro2_x, gyro2_y, gyro2_z;
    float temp2;

    // Metadata
    uint32_t timestamp;
    uint32_t frame_count;
    uint8_t status;
} imu_frame_t;
#pragma pack(pop)

#define IMU_FRAME_SIZE sizeof(imu_frame_t)

#endif