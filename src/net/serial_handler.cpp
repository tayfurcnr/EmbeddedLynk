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

// --- Ortak Frame Ayrıştırma Yardımcıları (Hem Donanımsal hem Yazılımsal UART için) ---

/**
 * @brief Gelen ham byte dizisinden beklenen toplam frame uzunluğunu hesaplar.
 * 
 * Bu fonksiyon, frame başlığındaki `payload_len` alanını okuyarak çalışır.
 * Başlık tam olarak alınmadıysa 0 döner.
 * 
 * @param buffer Gelen byte'ları içeren tampon.
 * @param len Tamponda şu anki byte sayısı.
 * @return Beklenen toplam frame uzunluğu veya 0.
 */
static size_t get_expected_frame_length(const uint8_t* buffer, size_t len) {
    // payload_len alanını okumak için gereken minimum uzunluk
    const size_t LYNK_HEADER_SIZE = 7;
    const size_t LYNK_CRC_SIZE = 2;

    if (len < LYNK_HEADER_SIZE) {
        return 0; // Uzunluğu belirlemek için yeterli veri yok.
    }

    uint8_t payload_len = buffer[6]; // payload_len alanı 7. byte'dır (index 6)
    return LYNK_HEADER_SIZE + payload_len + LYNK_CRC_SIZE;
}

// Seri porttan gelen veriyi işlemek için durum makinesi (state machine)
typedef enum {
    WAITING_FOR_START_1,
    WAITING_FOR_START_2,
    READING_FRAME
} RxState;

// Durum makinesini kullanarak byte'ları işleyen genel fonksiyon
static void process_byte(uint8_t byte, uint8_t* rx_buffer, size_t* buffer_idx, size_t buffer_size, RxState* state, frame_source_t source, const lynk_config_t* cfg);

// === MODULE RX Task (hardware UART için) ===
#if MODULE_UART_TYPE == UART_TYPE_HARDWARE
static void serial_rx_task_module(void* arg) {
    uint8_t data_buffer[UART_RX_BUFFER_SIZE];
    
    // Durum makinesi için değişkenler
    uint8_t frame_buffer[UART_RX_BUFFER_SIZE];
    size_t buffer_idx = 0;
    RxState state = WAITING_FOR_START_1;
    const lynk_config_t* cfg = config_get();

    while (true) {
        // UART'tan veri oku (daha kısa timeout ile daha sık kontrol)
        int len = uart_read_bytes(module_uart_port, data_buffer, sizeof(data_buffer), pdMS_TO_TICKS(20));
        if (len > 0) {
            // Gelen her byte'ı durum makinesi ile işle
            for (int i = 0; i < len; i++) {
                process_byte(data_buffer[i], frame_buffer, &buffer_idx, sizeof(frame_buffer), &state, FRAME_SOURCE_MODULE, cfg);
            }
        }
    }
}
#endif

// === USER RX Task (hardware UART için) ===
#if USER_UART_TYPE == UART_TYPE_HARDWARE
static void serial_rx_task_user(void* arg) {
    uint8_t data_buffer[UART_RX_BUFFER_SIZE];

    // Durum makinesi için değişkenler
    uint8_t frame_buffer[UART_RX_BUFFER_SIZE];
    size_t buffer_idx = 0;
    RxState state = WAITING_FOR_START_1;
    const lynk_config_t* cfg = config_get();

    while (true) {
        // UART'tan veri oku (daha kısa timeout ile daha sık kontrol)
        int len = uart_read_bytes(user_uart_port, data_buffer, sizeof(data_buffer), pdMS_TO_TICKS(20));
        if (len > 0) {
            // Gelen her byte'ı durum makinesi ile işle
            for (int i = 0; i < len; i++) {
                process_byte(data_buffer[i], frame_buffer, &buffer_idx, sizeof(frame_buffer), &state, FRAME_SOURCE_USER, cfg);
            }
        }
    }
}
#endif

// Durum makinesini kullanarak byte'ları işleyen genel fonksiyon
static void process_byte(uint8_t byte, uint8_t* rx_buffer, size_t* buffer_idx, size_t buffer_size, RxState* state, frame_source_t source, const lynk_config_t* cfg) {
    switch (*state) {
        case WAITING_FOR_START_1:
            if (byte == cfg->start_byte) {
                rx_buffer[0] = byte;
                *buffer_idx = 1;
                *state = WAITING_FOR_START_2;
            }
            break;

        case WAITING_FOR_START_2:
            if (byte == cfg->start_byte_2) {
                rx_buffer[(*buffer_idx)++] = byte;
                *state = READING_FRAME;
            } else {
                // Yanlış sıra, başa dön. Eğer yeni byte ilk start byte ise, durumu ona göre ayarla.
                *state = WAITING_FOR_START_1;
                if (byte == cfg->start_byte) {
                    rx_buffer[0] = byte;
                    *buffer_idx = 1;
                    *state = WAITING_FOR_START_2;
                }
            }
            break;

        case READING_FRAME:
            if (*buffer_idx < buffer_size) {
                rx_buffer[(*buffer_idx)++] = byte;
            } else {
                // Buffer taştı, ayrıştırıcıyı sıfırla
                Serial.printf("[%s RX] Buffer overflow, resetting parser.\n", source == FRAME_SOURCE_USER ? "USER" : "MODULE");
                *state = WAITING_FOR_START_1;
                *buffer_idx = 0;
                break;
            }

            // Frame'in tamamının gelip gelmediğini kontrol et
            size_t total_frame_len = get_expected_frame_length(rx_buffer, *buffer_idx);
            if (total_frame_len > 0 && total_frame_len <= buffer_size && *buffer_idx >= total_frame_len) {
                lynk_frame_t frame;
                if (decode_frame(rx_buffer, total_frame_len, &frame)) {
                    const char* source_str = (source == FRAME_SOURCE_USER) ? "USER" : "MODULE";
                    Serial.printf("[%s RX] Valid frame received (dst_id=0x%02X)\n", source_str, frame.dst_id);
                    frame_router_process(&frame, source);
                } // Hata durumunda loglama artık decode_frame içinde yapılıyor.

                // Sonraki frame için durumu sıfırla
                *state = WAITING_FOR_START_1;
                *buffer_idx = 0;
            }
            break;
    }
}

// === MODULE RX Task (software UART için) ===
#if MODULE_UART_TYPE == UART_TYPE_SOFTWARE
static void serial_rx_task_module_soft(void* arg) {
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    size_t buffer_idx = 0;
    RxState state = WAITING_FOR_START_1;
    const lynk_config_t* cfg = config_get();

    while (true) {
        if (softModuleSerial.available()) {
            uint8_t byte = softModuleSerial.read();
            process_byte(byte, rx_buffer, &buffer_idx, sizeof(rx_buffer), &state, FRAME_SOURCE_MODULE, cfg);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10)); // No data, yield to other tasks
        }
    }
}
#endif

// === USER RX Task (software UART için) ===
#if USER_UART_TYPE == UART_TYPE_SOFTWARE

static void serial_rx_task_user_soft(void* arg) {
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    size_t buffer_idx = 0;
    RxState state = WAITING_FOR_START_1;
    const lynk_config_t* cfg = config_get();

    while (true) {
        if (softUserSerial.available()) {
            uint8_t byte = softUserSerial.read();
            process_byte(byte, rx_buffer, &buffer_idx, sizeof(rx_buffer), &state, FRAME_SOURCE_USER, cfg);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10)); // No data, yield to other tasks
        }
    }
}
#endif

// --- Internal Hardware Implementations ---
// These are the actual functions that write to the UART ports.
// They are renamed to avoid conflict with the function pointers and made static.
static void real_serial_send_to_module(const lynk_frame_t* frame) {
    uint8_t buffer[512];
    size_t len = 0;

    if (encode_frame(frame, buffer, &len)) {
#if MODULE_UART_TYPE == UART_TYPE_HARDWARE
        // DEBUG: Gönderilecek ham byte'ları Hex formatında yazdır
        Serial.print("[MODULE TX RAW] Sending data: ");
        for (size_t i = 0; i < len; i++) {
            Serial.printf("%02X ", buffer[i]);
        }
        Serial.println();

        int bytes_written = uart_write_bytes(module_uart_port, (const char*)buffer, len);
        if (bytes_written == (int)len) {
            Serial.println("[MODULE TX] Frame sent (HW)");
        } else {
            // Bu log, verinin donanım tamponuna yazılamadığını gösterir.
            Serial.printf("[MODULE TX] uart_write_bytes failed. Expected %d, wrote %d\n", len, bytes_written);
        }
#elif MODULE_UART_TYPE == UART_TYPE_SOFTWARE
        softModuleSerial.write(buffer, len);
        Serial.println("[MODULE TX] Frame sent (SOFT)");
#endif
    } else {
        Serial.println("[MODULE TX] Frame encode FAILED");
    }
}

static void real_serial_send_to_user(const lynk_frame_t* frame) {
    uint8_t buffer[512];
    size_t len = 0;

    if (encode_frame(frame, buffer, &len)) {
#if USER_UART_TYPE == UART_TYPE_HARDWARE
        // DEBUG: Gönderilecek ham byte'ları Hex formatında yazdır
        Serial.print("[USER TX RAW] Sending data: ");
        for (size_t i = 0; i < len; i++) {
            Serial.printf("%02X ", buffer[i]);
        }
        Serial.println();

        int bytes_written = uart_write_bytes(user_uart_port, (const char*)buffer, len);
        if (bytes_written == (int)len) {
            Serial.println("[USER TX] Frame sent (HW)");
        } else {
            // Bu log, verinin donanım tamponuna yazılamadığını gösterir.
            Serial.printf("[USER TX] uart_write_bytes failed. Expected %d, wrote %d\n", len, bytes_written);
        }
#elif USER_UART_TYPE == UART_TYPE_SOFTWARE
        softUserSerial.write(buffer, len);
        Serial.println("[USER TX] Frame sent (SOFT)");
#endif
    } else {
        Serial.println("[USER TX] Frame encode FAILED");
    }
}

// --- Public Function Pointers ---
// These pointers are defined here and initialized to point to the real functions.
// The 'extern' declarations in the header file make them accessible to other modules.
serial_send_func_t serial_handler_send_to_module = real_serial_send_to_module;
serial_send_func_t serial_handler_send_to_user = real_serial_send_to_user;

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
    xTaskCreate(serial_rx_task_module_soft, "serial_rx_module_soft", 4096, NULL, 10, NULL);
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
    xTaskCreate(serial_rx_task_user_soft, "serial_rx_user_soft", 4096, NULL, 10, NULL);
#endif
}
