#ifndef BLE_H
#define BLE_H

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

// BLE Configuration Constants
#define GATTS_TAG "GATTS_BLE"

#define GATTS_SERVICE_UUID 0x00FF
#define GATTS_CHAR_UUID_TX 0xFF01 // Server -> Client (notifications)
#define GATTS_CHAR_UUID_RX 0xFF02 // Client -> Server (writes/acknowledgments)
#define GATTS_DESCR_UUID 0x3333
#define GATTS_NUM_HANDLE 8
#define ACK_TIMEOUT_MS 2000

#define PROFILE_NUM 1
#define PROFILE_A_APP_ID 0

// Maximum device name length
#define DEVICE_NAME_MAX_LEN ESP_BLE_ADV_NAME_LEN_MAX

    // BLE profile instance structure
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

    // Global BLE variables (extern declarations)
    extern uint16_t current_conn_id;
    extern uint16_t current_gatts_if;
    extern bool notifications_enabled;
    extern uint16_t local_mtu;
    extern bool ack_received;
    extern uint16_t char_handle_tx;
    extern uint16_t char_handle_rx;
    extern uint16_t descr_handle_tx;
    extern char device_name[DEVICE_NAME_MAX_LEN];
    extern struct gatts_profile_inst gl_profile_tab[PROFILE_NUM];
    extern uint8_t adv_service_uuid128[32];

    /**
     * @brief Initialize the BLE GATT server
     *
     * This function initializes NVS, Bluetooth controller, Bluedroid stack,
     * and registers all necessary BLE callbacks and applications.
     *
     * @note This function must be called before any other BLE functions
     */
    void ble_init(void);

    /**
     * @brief Send a string to the connected BLE client
     *
     * @param string The null-terminated string to send
     * @return true if the string was sent successfully
     * @return false if sending failed (no connection, notifications disabled, etc.)
     */
    bool send_string_to_client(const char *string);

    /**
     * @brief Send a string and wait for acknowledgment from the client
     *
     * This function sends a string to the client and waits for an "ACK" response
     * within the defined timeout period.
     *
     * @param string The null-terminated string to send
     * @return true if the string was sent and acknowledged
     * @return false if sending failed or acknowledgment timeout occurred
     */
    bool send_string_and_wait_ack(const char *string);

    /**
     * @brief Check if a BLE client is currently connected
     *
     * @return true if a client is connected
     * @return false if no client is connected
     */
    static inline bool ble_is_connected(void)
    {
        return (current_conn_id != 0xFFFF);
    }

    /**
     * @brief Check if notifications are enabled by the client
     *
     * @return true if notifications are enabled
     * @return false if notifications are disabled
     */
    static inline bool ble_notifications_enabled(void)
    {
        return notifications_enabled;
    }

    /**
     * @brief Get the current MTU size
     *
     * @return uint16_t The current MTU size in bytes
     */
    static inline uint16_t ble_get_mtu(void)
    {
        return local_mtu;
    }

    /**
     * @brief Get the current connection ID
     *
     * @return uint16_t The connection ID, or 0xFFFF if not connected
     */
    static inline uint16_t ble_get_connection_id(void)
    {
        return current_conn_id;
    }

    /**
     * @brief Set the device name
     *
     * @param name The new device name (will be truncated if too long)
     * @return true if name was set successfully
     * @return false if setting name failed
     */
    bool ble_set_device_name(const char *name);

    /**
     * @brief Restart BLE advertising
     *
     * Use this function to restart advertising after a disconnect
     * or when you want to make the device discoverable again.
     */
    void ble_restart_advertising(void);

    /**
     * @brief Stop BLE advertising
     *
     * Use this function to stop advertising when you don't want
     * the device to be discoverable.
     */
    void ble_stop_advertising(void);

    // Internal event handlers (should not be called directly by application)
    void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
    void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
    void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

#ifdef __cplusplus
}
#endif

#endif // BLE_H