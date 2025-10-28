#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

// I2C Configuration
#define I2C_MASTER_SCL_IO 2
#define I2C_MASTER_SDA_IO 1
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

// OLED Configuration
#define OLED_ADDRESS 0x3C

static const char *IMUTAG = "MPU6050";
static const char *OLEDTAG = "OLED";

// Initialize I2C master
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

// Simple OLED functions
static void oled_write_command(uint8_t command)
{
    uint8_t buf[2] = {0x00, command};
    i2c_master_write_to_device(I2C_MASTER_NUM, OLED_ADDRESS, buf, 2, pdMS_TO_TICKS(1000));
}

static void oled_write_data(uint8_t data)
{
    uint8_t buf[2] = {0x40, data};
    i2c_master_write_to_device(I2C_MASTER_NUM, OLED_ADDRESS, buf, 2, pdMS_TO_TICKS(1000));
}

static void oled_clear_screen(void)
{
    for (int page = 0; page < 8; page++)
    {
        oled_write_command(0xB0 + page);
        oled_write_command(0x00);
        oled_write_command(0x10);
        for (int col = 0; col < 128; col++)
        {
            oled_write_data(0x00);
        }
    }
}

static void oled_set_cursor(uint8_t page, uint8_t column)
{
    oled_write_command(0xB0 + page);
    oled_write_command(0x00 + (column & 0x0F));
    oled_write_command(0x10 + ((column >> 4) & 0x0F));
}

// Simple font data for basic characters (0-9, A-Z, a-z, space, and some symbols)
static const uint8_t simple_font[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x10, 0x08, 0x08, 0x10, 0x08}, // ~
};

static void oled_write_char(char c)
{
    if (c < 32 || c > 126)
        return; // Only printable ASCII characters

    const uint8_t *char_data = simple_font[c - 32];
    for (int i = 0; i < 5; i++)
    {
        oled_write_data(char_data[i]);
    }
    oled_write_data(0x00); // Space between characters
}

static void oled_print_text(uint8_t page, uint8_t column, const char *text)
{
    oled_set_cursor(page, column);
    for (int i = 0; text[i] != '\0'; i++)
    {
        oled_write_char(text[i]);
    }
}

static void oled_init_simple(void)
{
    ESP_LOGI(OLEDTAG, "Initializing OLED display...");

    vTaskDelay(pdMS_TO_TICKS(100));

    // SSD1306 initialization sequence
    oled_write_command(0xAE); // Display OFF
    oled_write_command(0x20); // Memory addressing mode
    oled_write_command(0x00); // Horizontal addressing mode
    oled_write_command(0x21); // Column address
    oled_write_command(0x00); // Start column 0
    oled_write_command(0x7F); // End column 127
    oled_write_command(0x22); // Page address
    oled_write_command(0x00); // Start page 0
    oled_write_command(0x07); // End page 7
    oled_write_command(0x40); // Start line 0
    oled_write_command(0xA0); // Segment remap
    oled_write_command(0xA8); // Multiplex ratio
    oled_write_command(0x3F); // 64MUX
    oled_write_command(0xC0); // COM scan normal
    oled_write_command(0xA4); // Entire display ON
    oled_write_command(0xA6); // Normal display
    oled_write_command(0xD3); // Display offset
    oled_write_command(0x00); // No offset
    oled_write_command(0xD5); // Display clock divide
    oled_write_command(0x80); // Default
    oled_write_command(0xD9); // Pre-charge period
    oled_write_command(0xF1);
    oled_write_command(0xDA); // COM pins hardware configuration
    oled_write_command(0x12);
    oled_write_command(0xDB); // VCOMH deselect level
    oled_write_command(0x40);
    oled_write_command(0x8D); // Charge pump setting
    oled_write_command(0x14); // Enable charge pump
    oled_write_command(0xAF); // Display ON

    oled_clear_screen();
    ESP_LOGI(OLEDTAG, "OLED initialized successfully");
}

static void oled_update_display(float accel1_x, float accel1_y, float accel1_z,
                                float accel2_x, float accel2_y, float accel2_z,
                                float gyro1_x, float gyro1_y, float gyro1_z,
                                float gyro2_x, float gyro2_y, float gyro2_z,
                                float temp1, float temp2)
{
    char line[22];
    char temp_buf[25];

    // Line 0: Header
    snprintf(line, sizeof(line), "Dual IMU Monitor");
    oled_print_text(0, 0, line);

    // Line 1: IMU1 Acceleration
    snprintf(temp_buf, sizeof(temp_buf), "A1:%.1f,%.1f,%.1f", accel1_x, accel1_y, accel1_z);
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(1, 0, line);

    // Line 2: IMU1 Gyroscope
    snprintf(temp_buf, sizeof(temp_buf), "G1:%.1f,%.1f,%.1f", gyro1_x, gyro1_y, gyro1_z);
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(2, 0, line);

    // Line 3: IMU2 Acceleration
    snprintf(temp_buf, sizeof(temp_buf), "A2:%.1f,%.1f,%.1f", accel2_x, accel2_y, accel2_z);
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(3, 0, line);

    // Line 4: IMU2 Gyroscope
    snprintf(temp_buf, sizeof(temp_buf), "G2:%.1f,%.1f,%.1f", gyro2_x, gyro2_y, gyro2_z);
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(4, 0, line);

    // Line 5: Temperatures
    snprintf(temp_buf, sizeof(temp_buf), "T1:%.1fF T2:%.1fF", temp1, temp2);
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(5, 0, line);

    // Line 6: Status
    static int counter = 0;
    snprintf(temp_buf, sizeof(temp_buf), "Update: %d", counter++);
    snprintf(line, sizeof(line), "\%-20.20s", temp_buf);
    oled_print_text(6, 0, line);

    snprintf(line, sizeof(line), "\%-20.20s", "");
    oled_print_text(7, 0, line);
}

// MPU-6050 Functions
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

        *gx = gyro_x / 65.5;
        *gy = gyro_y / 65.5;
        *gz = gyro_z / 65.5;
    }
    return ret;
}

static esp_err_t mpu6050_read_temperature_sensor(uint8_t sensor_addr, float *temp_f)
{
    uint8_t data[2];
    esp_err_t ret = mpu6050_register_read(sensor_addr, 0x41, data, sizeof(data)); // 0x41 is the temperature out reg

    if (ret == ESP_OK)
    {
        int16_t temp_raw = (data[0] << 8) | data[1];
        float temp_c = (temp_raw / 340.0) + 36.53;
        *temp_f = (temp_c * 9.0 / 5.0) + 32.0;
    }
    return ret;
}

void app_main(void)
{
    ESP_LOGI(IMUTAG, "Starting Dual MPU6050 + OLED Display");

    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(IMUTAG, "I2C initialized successfully");

    // Test I2C communication by scanning for devices
    ESP_LOGI(IMUTAG, "Scanning I2C bus...");
    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK)
        {
            ESP_LOGI(IMUTAG, "Found device at address: 0x%02X", addr);
        }
    }
    ESP_LOGI(IMUTAG, "I2C scan complete");

    // Initialize OLED
    // oled_init_simple();

    // Initialize both MPU-6050 sensors
    // if (mpu6050_init_both() != ESP_OK)
    if (mpu6050_init_sensor(MPU6050_I2C_ADDR_1, "IMU_1") != ESP_OK)
    {
        ESP_LOGE(IMUTAG, "Failed to initialize MPU6050 sensors");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(IMUTAG, "All devices initialized, starting readings...");

    // Data variables
    float accel1_x, accel1_y, accel1_z, gyro1_x, gyro1_y, gyro1_z, temp1_f;
    // float accel2_x, accel2_y, accel2_z, gyro2_x, gyro2_y, gyro2_z, temp2_f;

    while (1)
    {
        // Read IMU 1
        bool imu1_ok = (mpu6050_read_accelerometer_sensor(MPU6050_I2C_ADDR_1, &accel1_x, &accel1_y, &accel1_z) == ESP_OK &&
                        mpu6050_read_gyroscope_sensor(MPU6050_I2C_ADDR_1, &gyro1_x, &gyro1_y, &gyro1_z) == ESP_OK &&
                        mpu6050_read_temperature_sensor(MPU6050_I2C_ADDR_1, &temp1_f) == ESP_OK);

        // Read IMU 2
        // bool imu2_ok = (mpu6050_read_accelerometer_sensor(MPU6050_I2C_ADDR_2, &accel2_x, &accel2_y, &accel2_z) == ESP_OK &&
        //                 mpu6050_read_gyroscope_sensor(MPU6050_I2C_ADDR_2, &gyro2_x, &gyro2_y, &gyro2_z) == ESP_OK &&
        //                 mpu6050_read_temperature_sensor(MPU6050_I2C_ADDR_2, &temp2_f) == ESP_OK);

        // if (imu1_ok && imu2_ok)
        if (imu1_ok)
        {
            // Update OLED display
            // oled_update_display(accel1_x, accel1_y, accel1_z,
            //                     accel2_x, accel2_y, accel2_z,
            //                     gyro1_x, gyro1_y, gyro1_z,
            //                     gyro2_x, gyro2_y, gyro2_z,
            //                     temp1_f, temp2_f);

            // Log to serial
            ESP_LOGI(IMUTAG, "IMU1 Accel: X=%.2f, Y=%.2f, Z=%.2f m/s^2", accel1_x, accel1_y, accel1_z);
            ESP_LOGI(IMUTAG, "IMU1 Gyro:  X=%.2f, Y=%.2f, Z=%.2f deg/s", gyro1_x, gyro1_y, gyro1_z);
            // ESP_LOGI(IMUTAG, "IMU2 Accel: X=%.2f, Y=%.2f, Z=%.2f m/s^2", accel2_x, accel2_y, accel2_z);
            // ESP_LOGI(IMUTAG, "IMU2 Gyro:  X=%.2f, Y=%.2f, Z=%.2f deg/s", gyro2_x, gyro2_y, gyro2_z);
            // ESP_LOGI(IMUTAG, "Temps: IMU1=%.1f°F, IMU2=%.1f°F", temp1_f, temp2_f);
            ESP_LOGI(IMUTAG, "Temps: IMU1=%.1f°F", temp1_f);
        }
        else
        {
            ESP_LOGE(IMUTAG, "Failed to read one or both IMUs");
        }

        ESP_LOGI(IMUTAG, "---");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}