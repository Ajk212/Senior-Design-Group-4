#include <stdio.h>
#include "drv2605.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DRV2605";

// Register map
typedef enum
{
    DRV2605_REG_STATUS = 0x00,
    DRV2605_REG_MODE = 0x01,
    DRV2605_REG_RTPIN = 0x02,
    DRV2605_REG_LIBRARY = 0x03,
    DRV2605_REG_WAVESEQ1 = 0x04,
    DRV2605_REG_WAVESEQ2 = 0x05,
    DRV2605_REG_WAVESEQ3 = 0x06,
    DRV2605_REG_WAVESEQ4 = 0x07,
    DRV2605_REG_WAVESEQ5 = 0x08,
    DRV2605_REG_WAVESEQ6 = 0x09,
    DRV2605_REG_WAVESEQ7 = 0x0A,
    DRV2605_REG_WAVESEQ8 = 0x0B,
    DRV2605_REG_GO = 0x0C,
    DRV2605_REG_RATEDV = 0x16,
    DRV2605_REG_CLAMPV = 0x17,
    DRV2605_REG_FEEDBACK = 0x1A,
} drv2605_register_t;

// Driver context structure
typedef struct
{
    i2c_port_t i2c_port;
    uint8_t i2c_address;
    gpio_num_t enable_pin;
    bool enable_pin_control;
    bool initialized;
} drv2605_dev_t;

// Private function declarations
static esp_err_t drv2605_write_register(drv2605_handle_t handle, uint8_t reg, uint8_t value);
static esp_err_t drv2605_read_register(drv2605_handle_t handle, uint8_t reg, uint8_t *value);
static esp_err_t drv2605_configure_device(drv2605_handle_t handle);

drv2605_handle_t drv2605_init(const drv2605_config_t *config)
{
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Config cannot be NULL");
        return NULL;
    }

    // Allocate device context
    drv2605_dev_t *dev = calloc(1, sizeof(drv2605_dev_t));
    if (dev == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate device context");
        return NULL;
    }

    // Copy configuration
    dev->i2c_port = config->i2c_port;
    dev->i2c_address = config->i2c_address;
    dev->enable_pin = config->enable_pin;
    dev->enable_pin_control = config->enable_pin_control;

    // Configure EN pin if needed
    if (dev->enable_pin_control)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << dev->enable_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
        gpio_set_level(dev->enable_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(10)); // Power stabilization
        ESP_LOGI(TAG, "Enabled DRV2605 on GPIO %d", dev->enable_pin);
    }

    // Configure device
    if (drv2605_configure_device(dev) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure DRV2605");
        free(dev);
        return NULL;
    }

    dev->initialized = true;
    ESP_LOGI(TAG, "DRV2605 initialized successfully");
    return (drv2605_handle_t)dev;
}

void drv2605_deinit(drv2605_handle_t handle)
{
    if (handle == NULL)
        return;

    drv2605_dev_t *dev = (drv2605_dev_t *)handle;

    // Stop playback and disable device
    drv2605_stop(handle);

    if (dev->enable_pin_control)
    {
        gpio_set_level(dev->enable_pin, 0);
    }

    free(dev);
    ESP_LOGI(TAG, "DRV2605 deinitialized");
}

static esp_err_t drv2605_configure_device(drv2605_handle_t handle)
{
    drv2605_dev_t *dev = (drv2605_dev_t *)handle;
    esp_err_t err;

    // Read status to verify communication
    uint8_t status;
    err = drv2605_read_register(handle, DRV2605_REG_STATUS, &status);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to communicate with DRV2605");
        return err;
    }
    ESP_LOGI(TAG, "Device status: 0x%02X", status);

    // Basic configuration
    err = drv2605_write_register(handle, DRV2605_REG_MODE, DRV2605_MODE_INTTRIG);
    err |= drv2605_write_register(handle, DRV2605_REG_RTPIN, 0x00);
    err |= drv2605_write_register(handle, DRV2605_REG_LIBRARY, 0x01); // TS2200A library

    // Clear waveform sequence
    for (int i = 0; i < 8; i++)
    {
        err |= drv2605_write_register(handle, DRV2605_REG_WAVESEQ1 + i, 0x00);
    }

    // Default strength configuration for 3V motor
    err |= drv2605_write_register(handle, DRV2605_REG_RATEDV, 0x90); // ~3.0V
    err |= drv2605_write_register(handle, DRV2605_REG_CLAMPV, 0xA0); // ~4.0V max

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure DRV2605 registers");
    }

    return err;
}

esp_err_t drv2605_play_effect(drv2605_handle_t handle, uint8_t effect_id)
{
    if (handle == NULL)
        return ESP_ERR_INVALID_ARG;

    ESP_LOGD(TAG, "Playing effect: %d", effect_id);

    esp_err_t err = drv2605_stop(handle);
    err |= drv2605_write_register(handle, DRV2605_REG_WAVESEQ1, effect_id);
    err |= drv2605_write_register(handle, DRV2605_REG_WAVESEQ2, 0x00); // Clear next slot
    err |= drv2605_write_register(handle, DRV2605_REG_GO, 0x01);

    return err;
}

esp_err_t drv2605_play_sequence(drv2605_handle_t handle, const uint8_t *effects, size_t count)
{
    if (handle == NULL || effects == NULL || count == 0 || count > 8)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Playing sequence of %d effects", count);

    esp_err_t err = drv2605_stop(handle);

    // Set waveform sequence
    for (size_t i = 0; i < count; i++)
    {
        err |= drv2605_write_register(handle, DRV2605_REG_WAVESEQ1 + i, effects[i]);
    }

    // Clear remaining slots
    for (size_t i = count; i < 8; i++)
    {
        err |= drv2605_write_register(handle, DRV2605_REG_WAVESEQ1 + i, 0x00);
    }

    err |= drv2605_write_register(handle, DRV2605_REG_GO, 0x01);

    return err;
}

esp_err_t drv2605_set_realtime_strength(drv2605_handle_t handle, uint8_t strength)
{
    if (handle == NULL)
        return ESP_ERR_INVALID_ARG;

    ESP_LOGD(TAG, "Setting real-time strength: %d", strength);

    esp_err_t err = drv2605_write_register(handle, DRV2605_REG_MODE, DRV2605_MODE_REALTIME);
    err |= drv2605_write_register(handle, DRV2605_REG_RTPIN, strength);

    return err;
}

esp_err_t drv2605_stop(drv2605_handle_t handle)
{
    if (handle == NULL)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = drv2605_write_register(handle, DRV2605_REG_GO, 0x00);
    err |= drv2605_write_register(handle, DRV2605_REG_RTPIN, 0x00);
    err |= drv2605_write_register(handle, DRV2605_REG_MODE, DRV2605_MODE_INTTRIG);

    return err;
}

esp_err_t drv2605_set_strength_config(drv2605_handle_t handle, uint8_t rated_voltage, uint8_t clamp_voltage)
{
    if (handle == NULL)
        return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "Setting strength config: rated=0x%02X, clamp=0x%02X", rated_voltage, clamp_voltage);

    esp_err_t err = drv2605_write_register(handle, DRV2605_REG_RATEDV, rated_voltage);
    err |= drv2605_write_register(handle, DRV2605_REG_CLAMPV, clamp_voltage);

    return err;
}

esp_err_t drv2605_get_status(drv2605_handle_t handle, uint8_t *status)
{
    if (handle == NULL || status == NULL)
        return ESP_ERR_INVALID_ARG;

    return drv2605_read_register(handle, DRV2605_REG_STATUS, status);
}

// Private functions
static esp_err_t drv2605_write_register(drv2605_handle_t handle, uint8_t reg, uint8_t value)
{
    drv2605_dev_t *dev = (drv2605_dev_t *)handle;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Write failed: reg 0x%02X = 0x%02X", reg, value);
    }

    return ret;
}

static esp_err_t drv2605_read_register(drv2605_handle_t handle, uint8_t reg, uint8_t *value)
{
    drv2605_dev_t *dev = (drv2605_dev_t *)handle;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->i2c_address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Read failed: reg 0x%02X", reg);
    }

    return ret;
}
