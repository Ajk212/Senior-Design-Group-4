#include "ble.h"
#include "string.h"
#include "oled.h"

char device_name[DEVICE_NAME_MAX_LEN] = "ESP32C6-HANDSENSE";

// // Global BLE variables
uint16_t current_conn_id = 0xFFFF;
uint16_t current_gatts_if = ESP_GATT_IF_NONE;
bool notifications_enabled = false;
uint16_t local_mtu = 23;
bool ack_received = false;
uint16_t char_handle_tx = 0, char_handle_rx = 0;
uint16_t descr_handle_tx = 0;

uint8_t adv_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // first uuid, 16bit, [12],[13] is the value
    0xfb,
    0x34,
    0x9b,
    0x5f,
    0x80,
    0x00,
    0x00,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0xEE,
    0x00,
    0x00,
    0x00,
    // second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb,
    0x34,
    0x9b,
    0x5f,
    0x80,
    0x00,
    0x00,
    0x80,
    0x00,
    0x10,
    0x00,
    0x00,
    0xFF,
    0x00,
    0x00,
    0x00,
};

struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = NULL, // Will be set in event handler
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = PROFILE_A_APP_ID,
        .conn_id = 0,
        .service_handle = 0,
        .char_handle = 0,
        .descr_handle = 0},
};

// BLE Functions
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TAG, "Advertising start failed, status %d", param->adv_start_cmpl.status);
            break;
        }
        ESP_LOGI(GATTS_TAG, "Advertising start successfully");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TAG, "Advertising stop failed, status %d", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GATTS_TAG, "Advertising stop successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(GATTS_TAG, "Connection params update, status %d, conn_int %d, latency %d, timeout %d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        ESP_LOGD(GATTS_TAG, "Unhandled GAP event: %d", event);
        break;
    }
}

void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_CONF_EVT:
        // This event is triggered when client acknowledges the indication
        if (param->conf.status == ESP_GATT_OK)
        {
            ESP_LOGI(GATTS_TAG, "Client acknowledged the indication");
        }
        else
        {
            ESP_LOGE(GATTS_TAG, "Client failed to acknowledge indication");
        }
        break;
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "GATT server register, status %d, app_id %d, gatts_if %d",
                 param->reg.status, param->reg.app_id, gatts_if);

        // Set device name first
        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(device_name);
        if (set_dev_name_ret)
        {
            ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        else
        {
            ESP_LOGI(GATTS_TAG, "Device name set to: %s", device_name);
        }

        // Configure advertising data
        esp_ble_adv_data_t adv_data = {
            .set_scan_rsp = false,
            .include_name = true,
            .include_txpower = true,
            .min_interval = 0x0006,
            .max_interval = 0x0010,
            .appearance = 0x00,
            .manufacturer_len = 0,
            .p_manufacturer_data = NULL,
            .service_data_len = 0,
            .p_service_data = NULL,
            .service_uuid_len = sizeof(adv_service_uuid128),
            .p_service_uuid = adv_service_uuid128,
            .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)};

        esp_ble_gap_config_adv_data(&adv_data);

        // Create service
        gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE);
        break;

    case ESP_GATTS_READ_EVT:
        ESP_LOGI(GATTS_TAG, "GATT read event");
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(GATTS_TAG, "GATT write event, handle %d, len %d, need_rsp=%d, is_prep=%d",
                 param->write.handle, param->write.len,
                 param->write.need_rsp, param->write.is_prep);

        bool responded = false;

        // Handle CCCD writes (notification enable/disable)
        if (param->write.handle == descr_handle_tx && param->write.len == 2)
        {
            uint16_t cccd = (uint16_t)param->write.value[0] | ((uint16_t)param->write.value[1] << 8);
            notifications_enabled = (cccd == 0x0001); // 0x0001 = notifications, 0x0002 = indications
            ESP_LOGI(GATTS_TAG, "Notifications %s", notifications_enabled ? "enabled" : "disabled");
            if (param->write.need_rsp)
            {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                            param->write.trans_id, ESP_GATT_OK, NULL);
                responded = true;
            }
            break;
        }
        // Handle RX characteristic writes (client acknowledgments)
        if (param->write.handle == char_handle_rx)
        {
            // If long write/prepare write, handle in prepare buffer and return
            if (param->write.is_prep)
            {
                // handle prepare write chunks here, then respond OK if need_rsp
                if (param->write.need_rsp)
                {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                                param->write.trans_id, ESP_GATT_OK, NULL);
                    responded = true;
                }
                break;
            }
            // Client sends data - treat as acknowledgment
            char ack_data[param->write.len + 1];
            size_t n = param->write.len < ESP_GATT_MAX_ATTR_LEN ? param->write.len : ESP_GATT_MAX_ATTR_LEN;
            memcpy(ack_data, param->write.value, n);
            ack_data[n] = '\0';

            ESP_LOGI(GATTS_TAG, "Client sent: %s", ack_data);

            // Set acknowledgment flag if client sends "ACK"
            if (n >= 3 && memcmp(param->write.value, "ACK", 3) == 0)
            {
                ack_received = true;
                ESP_LOGI(GATTS_TAG, "Acknowledgment flag set");
            }

            if (param->write.need_rsp && !responded)
            {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                            param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;
        }
        if (param->write.need_rsp && !responded)
        {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                        param->write.trans_id, ESP_GATT_OK, NULL);
        }
        break;

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(GATTS_TAG, "MTU exchange, MTU %d", param->mtu.mtu);
        local_mtu = param->mtu.mtu;
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "Service created, handle %d", param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;

        // Add TX Characteristic (Server -> Client)
        esp_bt_uuid_t char_uuid_tx = {
            .len = ESP_UUID_LEN_16,
            .uuid = {.uuid16 = GATTS_CHAR_UUID_TX}};

        esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
                                                        &char_uuid_tx,
                                                        ESP_GATT_PERM_READ,
                                                        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                                        NULL, NULL);
        if (add_char_ret)
            ESP_LOGE(GATTS_TAG, "Add TX char failed: %s", esp_err_to_name(add_char_ret));
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(GATTS_TAG, "Characteristic added, handle %d, uuid 0x%04x",
                 param->add_char.attr_handle, param->add_char.char_uuid.uuid.uuid16);

        if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_TX)
        {
            char_handle_tx = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "TX characteristic handle: %d", char_handle_tx);

            uint8_t cccd_val[2] = {0x00, 0x00};
            esp_attr_value_t cccd_attr = {
                .attr_max_len = sizeof(cccd_val),
                .attr_len = sizeof(cccd_val),
                .attr_value = cccd_val,
            };

            // Add CCCD descriptor for TX
            esp_bt_uuid_t descr_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}};

            esp_err_t ret = esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
                                                         &descr_uuid,
                                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                         &cccd_attr, NULL);
            if (ret)
                ESP_LOGE(GATTS_TAG, "Add TX descriptor failed: %s", esp_err_to_name(ret));
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_RX)
        {
            // Store RX characteristic handle
            char_handle_rx = param->add_char.attr_handle;
            ESP_LOGI(GATTS_TAG, "RX characteristic handle: %d", char_handle_rx);

            // Start service and advertise after the creation of both characteristics
            esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);

            esp_ble_adv_params_t adv_params = {
                .adv_int_min = 0x20,
                .adv_int_max = 0x40,
                .adv_type = ADV_TYPE_IND,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            };
            esp_ble_gap_start_advertising(&adv_params);
            ESP_LOGI(GATTS_TAG, "Service started and advertising begun");
        }
        break;

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        descr_handle_tx = param->add_char_descr.attr_handle;
        gl_profile_tab[PROFILE_A_APP_ID].descr_handle = param->add_char_descr.attr_handle;
        ESP_LOGI(GATTS_TAG, "TX descriptor handle: %d", descr_handle_tx);

        // RX Characteristic (Client -> Server)
        esp_bt_uuid_t char_uuid_rx = {
            .len = ESP_UUID_LEN_16,
            .uuid = {.uuid16 = GATTS_CHAR_UUID_RX}};

        esp_err_t ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
                                               &char_uuid_rx,
                                               ESP_GATT_PERM_WRITE,
                                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                                               NULL, NULL);
        if (ret)
            ESP_LOGE(GATTS_TAG, "Add RX char failed: %s", esp_err_to_name(ret));
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client connected, conn_id %d", param->connect.conn_id);
        current_conn_id = param->connect.conn_id;
        current_gatts_if = gatts_if;
        notifications_enabled = false;
        oled_draw_bitmap(0, 0, icon_bluetooth_14x16, 14, 16, false);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Client disconnected");
        current_conn_id = 0xFFFF;
        oled_draw_bitmap(0, 0, icon_bluetooth_disconn_14x16, 14, 16, false);
        notifications_enabled = false;
        // Restart advertising
        esp_ble_adv_params_t adv_params_restart = {
            .adv_int_min = 0x20,
            .adv_int_max = 0x40,
            .adv_type = ADV_TYPE_IND,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL,
            .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        };
        esp_ble_gap_start_advertising(&adv_params_restart);
        break;

    default:
        ESP_LOGD(GATTS_TAG, "Unhandled GATT event: %d", event);
        break;
    }
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
            gl_profile_tab[param->reg.app_id].gatts_cb = gatts_profile_event_handler;
        }
    }

    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[PROFILE_A_APP_ID].gatts_if)
    {
        if (gl_profile_tab[PROFILE_A_APP_ID].gatts_cb)
        {
            gl_profile_tab[PROFILE_A_APP_ID].gatts_cb(event, gatts_if, param);
        }
    }
}

void ble_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Bluetooth
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register BLE callbacks
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "GATTS register callback failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "GAP register callback failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register GATT application
    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return;
    }

    // Set larger MTU
    esp_ble_gatt_set_local_mtu(512);
}

bool send_string_to_client(const char *string)
{
    if (!notifications_enabled || current_conn_id == 0xFFFF)
    {
        ESP_LOGE(GATTS_TAG, "Cannot send - client not ready (notifications: %d, conn_id: 0x%04x)",
                 notifications_enabled, current_conn_id);
        return false;
    }

    if (char_handle_tx == 0)
    {
        ESP_LOGE(GATTS_TAG, "TX characteristic not initialized");
        return false;
    }

    size_t len = strlen(string);
    if (len > local_mtu - 3)
    { // Account for ATT headers
        ESP_LOGW(GATTS_TAG, "String too long, truncating");
        len = local_mtu - 3;
    }

    esp_err_t ret = esp_ble_gatts_send_indicate(current_gatts_if,
                                                current_conn_id,
                                                char_handle_tx,
                                                len, (uint8_t *)string,
                                                false); // false = notification

    if (ret != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to send string: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(GATTS_TAG, "String sent to client: %s", string);
    return true;
}

bool send_string_and_wait_ack(const char *string)
{
    // Reset acknowledgment flag
    ack_received = false;
    uint32_t send_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Send the string
    if (!send_string_to_client(string))
    {
        return false;
    }

    // Wait for acknowledgment with timeout
    while (!ack_received)
    {
        if ((xTaskGetTickCount() * portTICK_PERIOD_MS - send_time) > ACK_TIMEOUT_MS)
        {
            ESP_LOGE(GATTS_TAG, "Acknowledgment timeout after %d ms", ACK_TIMEOUT_MS);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Smaller delay for better responsiveness
    }

    ESP_LOGI(GATTS_TAG, "Acknowledgment received");
    return true;
}

bool ble_set_device_name(const char *name)
{
    if (name == NULL)
    {
        return false;
    }

    // Copy to our device name buffer
    strncpy(device_name, name, DEVICE_NAME_MAX_LEN - 1);
    device_name[DEVICE_NAME_MAX_LEN - 1] = '\0';

    // Update the BLE device name
    esp_err_t ret = esp_ble_gap_set_device_name(device_name);
    if (ret != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to set device name: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(GATTS_TAG, "Device name updated to: %s", device_name);
    return true;
}

void ble_restart_advertising(void)
{
    esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x20,
        .adv_int_max = 0x40,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

    esp_ble_gap_start_advertising(&adv_params);
    ESP_LOGI(GATTS_TAG, "Advertising restarted");
}

void ble_stop_advertising(void)
{
    esp_ble_gap_stop_advertising();
    ESP_LOGI(GATTS_TAG, "Advertising stopped");
}