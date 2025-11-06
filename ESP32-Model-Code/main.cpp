#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "imu_ble.h"

#include "models/Lite_LSTM_test.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

constexpr int kTensorArenaSize = 10 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
int WINDOW_SIZE = 20;

tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

const char* gesture_names[] = {
    "rest" ,
    "tilt_left",
    "tilt_right",
    "tilt_forward",
    "tilt_backward",
};

extern "C" void app_main() {

    const tflite::Model* model = tflite::GetModel(Lite_LSTM_Test_tflite);

    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("Model schema version mismatch!\n");
        while(1);
    }

    static tflite::AllOpsResolver resolver;

    static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);
    
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("Tensor allocation failed!\n");
        while(1);
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    printf("Model loaded successfully!\n");

    //Main model inference loop
    while (1) {

        //Read senor data frame - 20 values of 3-axis gyro
        for (int t = 0; t < WINDOW_SIZE; t++) {
            float gx, gy, gz;
            if (mpu6050_read_gyroscope_sensor(MPU6050_I2C_ADDR_1, &gx, &gy, &gz) != ESP_OK) {
                printf("Failed to read IMU1 gyro at step %d\n", t);
            }

            input->data.f[t * 3 + 0] = gx;
            input->data.f[t * 3 + 1] = gy;
            input->data.f[t * 3 + 2] = gz;

        }

        //Run model - If failed print and continue
        if (interpreter->Invoke() != kTfLiteOk) {
            printf("Model inference failed!\n");
            continue;
        }

        //Locate most probable gesture
        int max_index = 0;
        float max_value = output->data.f[0];
        for (int i = 1; i < output->dims->data[1]; i++) {
            if (output->data.f[i] > max_value) {
                max_value = output->data.f[i];
                max_index = i;
            }
        }

        //Take index and convert to gesture name
        const char* predicted_gesture = gesture_names[max_index];

        //Send result to buffer
        char ble_buffer[32]; 
        memset(ble_buffer, 0, sizeof(ble_buffer));
        strncpy(ble_buffer, predicted_gesture, sizeof(ble_buffer) - 1);

        //Send to computer via BLE
        if (notifications_enabled && current_conn_id != 0xFFFF) {
            esp_err_t err = esp_ble_gatts_send_indicate(
                current_gatts_if,
                current_conn_id,
                gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                strlen(ble_buffer),
                (uint8_t *)ble_buffer,
                false 
            );
            if (err != ESP_OK){
                    ESP_LOGE(GATTS_TAG, "Failed to send data %s", esp_err_to_name(err));
                }
        }
    }
}