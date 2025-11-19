#ifndef FRAME_BLE_COMMON_H
#define FRAME_BLE_COMMON_H

#include <stdint.h>

// Sensor Frame structure for BLE transmission for model training
#pragma pack(push, 1)
typedef struct
{
    // IMU 1 Data
    float accel1_x, accel1_y, accel1_z;
    float gyro1_x, gyro1_y, gyro1_z;

    // IMU 2 Data
    float accel2_x, accel2_y, accel2_z;
    float gyro2_x, gyro2_y, gyro2_z;

    // Flex Sensor Data
    float flex1_angle, flex2_angle, flex3_angle, flex4_angle, flex5_angle;
    float touch_voltage;

    // Metadata
    uint32_t timestamp;

} sensor_frame_t;
#pragma pack(pop)

#define SENSOR_FRAME_SIZE sizeof(sensor_frame_t)

#endif