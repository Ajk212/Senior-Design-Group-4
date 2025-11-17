#pragma once
#include "driver/i2c.h"
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief DRV2605 haptic motor driver component
 */

// I2C default configuration
#define DRV2605_I2C_DEFAULT_CONFIG() {   \
    .mode = I2C_MODE_MASTER,             \
    .sda_io_num = GPIO_NUM_22,           \
    .scl_io_num = GPIO_NUM_21,           \
    .sda_pullup_en = GPIO_PULLUP_ENABLE, \
    .scl_pullup_en = GPIO_PULLUP_ENABLE, \
    .master.clk_speed = 100000,          \
}

    // Device configuration
    typedef struct
    {
        i2c_port_t i2c_port;     // I2C port number
        uint8_t i2c_address;     // I2C device address
        gpio_num_t enable_pin;   // EN pin GPIO
        bool enable_pin_control; // Whether to control EN pin
    } drv2605_config_t;

#define DRV2605_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0,         \
    .i2c_address = 0x5A,           \
    .enable_pin = GPIO_NUM_5,      \
    .enable_pin_control = true,    \
}

    // Effect library
    typedef enum
    {
        DRV2605_EFFECT_STRONG_CLICK = 1,
        DRV2605_EFFECT_DOUBLE_CLICK = 4,
        DRV2605_EFFECT_TRIPLE_CLICK = 5,
        DRV2605_EFFECT_LONG_BUZZ = 17,
        DRV2605_EFFECT_SOLID_BUZZ = 7,
        DRV2605_EFFECT_RAMP_UP = 8,
        DRV2605_EFFECT_RAMP_DOWN = 9,
    } drv2605_effect_t;

    // Operation modes
    typedef enum
    {
        DRV2605_MODE_INTTRIG = 0x00,  // Internal trigger
        DRV2605_MODE_REALTIME = 0x05, // Real-time playback
    } drv2605_mode_t;

    // Driver handle
    typedef void *drv2605_handle_t;

    /**
     * @brief Initialize DRV2605 driver
     *
     * @param config Configuration structure
     * @return drv2605_handle_t Driver handle or NULL on error
     */
    drv2605_handle_t drv2605_init(const drv2605_config_t *config);

    /**
     * @brief Deinitialize DRV2605 driver
     *
     * @param handle Driver handle
     */
    void drv2605_deinit(drv2605_handle_t handle);

    /**
     * @brief Play a waveform effect
     *
     * @param handle Driver handle
     * @param effect_id Effect ID to play
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t drv2605_play_effect(drv2605_handle_t handle, uint8_t effect_id);

    /**
     * @brief Play multiple effects in sequence
     *
     * @param handle Driver handle
     * @param effects Array of effect IDs
     * @param count Number of effects
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t drv2605_play_sequence(drv2605_handle_t handle, const uint8_t *effects, size_t count);

    /**
     * @brief Set real-time playback strength
     *
     * @param handle Driver handle
     * @param strength Strength value (0-255)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t drv2605_set_realtime_strength(drv2605_handle_t handle, uint8_t strength);

    /**
     * @brief Stop playback
     *
     * @param handle Driver handle
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t drv2605_stop(drv2605_handle_t handle);

    /**
     * @brief Set motor strength configuration
     *
     * @param handle Driver handle
     * @param rated_voltage Rated voltage setting
     * @param clamp_voltage Clamp voltage setting
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t drv2605_set_strength_config(drv2605_handle_t handle, uint8_t rated_voltage, uint8_t clamp_voltage);

    /**
     * @brief Get device status
     *
     * @param handle Driver handle
     * @param status Output status register value
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t drv2605_get_status(drv2605_handle_t handle, uint8_t *status);

#ifdef __cplusplus
}
#endif