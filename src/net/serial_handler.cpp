#include "serial_handler.h"
#include "codec/frame_codec.h"
#include "core/config_manager.h"
#include "core/frame_router.h"
#include "core/uart_config.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if USER_UART_TYPE == UART_TYPE_SOFTWARE
#include <SoftwareSerial.h>
static SoftwareSerial softUserSerial(USER_UART_RX_PIN, USER_UART_TX_PIN);
#endif

#if MODULE_UART_TYPE == UART_TYPE_SOFTWARE
#include <SoftwareSerial.h>
static SoftwareSerial softModuleSerial(MODULE_UART_RX_PIN, MODULE_UART_TX_PIN);
#endif

#define UART_RX_BUFFER_SIZE 512
#define UART_TX_BUFFER_SIZE 512

static uart_port_t module_uart_port = MODULE_UART_PORT;
static uart_port_t user_uart_port = USER_UART_PORT;

// === MODULE RX Task (hardware UART için) ===
#if MODULE_UART_TYPE == UART_TYPE_HARDWARE
static void serial_rx_task_module(void* arg) {
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    while (true) {
        int len = uart_read_bytes(module_uart_port, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(100));
        if (len > 0) {
            lynk_frame_t frame;
            if (decode_frame(rx_buffer, len, &frame)) {
                Serial.printf("[MODULE RX] Valid frame received (dst_id=%u)\n", frame.dst_id);
                frame_router_process(&frame);
            } else {
                Serial.println("[MODULE RX] Invalid frame");
            }
        }
    }
}
#endif

// === USER RX Task (hardware UART için) ===
#if USER_UART_TYPE == UART_TYPE_HARDWARE
static void serial_rx_task_user(void* arg) {
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    while (true) {
        int len = uart_read_bytes(user_uart_port, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(100));
        if (len > 0) {
            lynk_frame_t frame;
            if (decode_frame(rx_buffer, len, &frame)) {
                Serial.printf("[USER RX] Valid frame received (dst_id=%u)\n", frame.dst_id);
                frame_router_process(&frame);
            } else {
                Serial.println("[USER RX] Invalid frame");
            }
        }
    }
}
#endif

void serial_handler_send_to_module(const lynk_frame_t* frame) {
    uint8_t buffer[512];
    size_t len = 0;

    if (encode_frame(frame, buffer, &len)) {
#if MODULE_UART_TYPE == UART_TYPE_HARDWARE
        int ret = uart_write_bytes(module_uart_port, (const char*)buffer, len);
        if (ret < 0) {
            Serial.printf("[MODULE TX] uart_write_bytes failed with error: %d\n", ret);
        } else {
            Serial.println("[MODULE TX] Frame sent (HW)");
        }
#elif MODULE_UART_TYPE == UART_TYPE_SOFTWARE
        softModuleSerial.write(buffer, len);
        Serial.println("[MODULE TX] Frame sent (SOFT)");
#endif
    } else {
        Serial.println("[MODULE TX] Frame encode FAILED");
    }
}


void serial_handler_send_to_user(const lynk_frame_t* frame) {
    uint8_t buffer[512];
    size_t len = 0;

    if (encode_frame(frame, buffer, &len)) {
#if USER_UART_TYPE == UART_TYPE_HARDWARE
        uart_write_bytes(user_uart_port, (const char*)buffer, len);
        Serial.println("[USER TX] Frame sent (HW)");
#elif USER_UART_TYPE == UART_TYPE_SOFTWARE
        softUserSerial.write(buffer, len);
        Serial.println("[USER TX] Frame sent (SOFT)");
#endif
    } else {
        Serial.println("[USER TX] Frame encode FAILED");
    }
}

void serial_handler_init(void) {
    const lynk_config_t* cfg = config_get();

#if MODULE_UART_TYPE == UART_TYPE_HARDWARE
    uart_driver_delete(MODULE_UART_PORT);

    uart_config_t uart_cfg = {
        .baud_rate = (int)cfg->uart_baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t res = uart_driver_install(MODULE_UART_PORT, UART_RX_BUFFER_SIZE, UART_TX_BUFFER_SIZE, 0, NULL, 0);
    Serial.printf("uart_driver_install result: %d\n", res);
    if (res != ESP_OK) {
        Serial.println("Failed to install MODULE UART driver");
        return;
    }

    res = uart_param_config(MODULE_UART_PORT, &uart_cfg);
    Serial.printf("uart_param_config result: %d\n", res);
    if (res != ESP_OK) {
        Serial.println("Failed to configure MODULE UART parameters");
        return;
    }

    res = uart_set_pin(MODULE_UART_PORT, MODULE_UART_TX_PIN, MODULE_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    Serial.printf("uart_set_pin result: %d\n", res);
    if (res != ESP_OK) {
        Serial.println("Failed to set MODULE UART pins");
        return;
    }

    xTaskCreate(serial_rx_task_module, "serial_rx_module", 4096, NULL, 10, NULL);
    Serial.printf("MODULE UART (HW) initialized: port=%d RX=%d TX=%d\n", MODULE_UART_PORT, MODULE_UART_RX_PIN, MODULE_UART_TX_PIN);
#elif MODULE_UART_TYPE == UART_TYPE_SOFTWARE
    softModuleSerial.begin(cfg->uart_baudrate);
    Serial.printf("MODULE UART (SW) initialized: RX=%d TX=%d\n", MODULE_UART_RX_PIN, MODULE_UART_TX_PIN);
#endif

#if USER_UART_TYPE == UART_TYPE_HARDWARE
    // USER UART donanımsal ise burada başlatılabilir
    uart_driver_delete(USER_UART_PORT); // Önceki kurulumu temizle

    uart_config_t user_uart_cfg = {
        .baud_rate = (int)cfg->uart_baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t user_res = uart_driver_install(USER_UART_PORT, UART_RX_BUFFER_SIZE, UART_TX_BUFFER_SIZE, 0, NULL, 0);
    Serial.printf("user_uart_driver_install result: %d\n", user_res);
    if (user_res != ESP_OK) {
        Serial.println("Failed to install USER UART driver");
        return;
    }

    user_res = uart_param_config(USER_UART_PORT, &user_uart_cfg);
    Serial.printf("user_uart_param_config result: %d\n", user_res);
    if (user_res != ESP_OK) {
        Serial.println("Failed to configure USER UART parameters");
        return;
    }

    user_res = uart_set_pin(USER_UART_PORT, USER_UART_TX_PIN, USER_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    Serial.printf("user_uart_set_pin result: %d\n", user_res);
    if (user_res != ESP_OK) {
        Serial.println("Failed to set USER UART pins");
        return;
    }

    xTaskCreate(serial_rx_task_user, "serial_rx_user", 4096, NULL, 10, NULL);
    Serial.printf("USER UART (HW) initialized: port=%d RX=%d TX=%d\n", USER_UART_PORT, USER_UART_RX_PIN, USER_UART_TX_PIN);
#elif USER_UART_TYPE == UART_TYPE_SOFTWARE
    softUserSerial.begin(cfg->uart_baudrate);
    Serial.printf("USER UART (SW) initialized: RX=%d TX=%d\n", USER_UART_RX_PIN, USER_UART_TX_PIN);
#endif
}
