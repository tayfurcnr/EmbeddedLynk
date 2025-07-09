#ifdef LYNK_BUILD_TEST

#include <Arduino.h>
#include <string.h>
#include "driver/uart.h"
#include "core/uart_config.h"
#include "core/config_manager.h"
#include "codec/frame_codec.h"
#include "core/frame_router.h"
#include "core/reset_handler.h"
#include "net/serial_handler.h"

// Helper function to compare configs
bool compare_configs(const lynk_config_t* cfg1, const lynk_config_t* cfg2) {
    return (cfg1->device_id == cfg2->device_id &&
            cfg1->mode == cfg2->mode &&
            cfg1->static_dst_id == cfg2->static_dst_id &&
            cfg1->uart_baudrate == cfg2->uart_baudrate &&
            cfg1->start_byte == cfg2->start_byte &&
            cfg1->start_byte_2 == cfg2->start_byte_2);
}

// ===============================
// 🔁 Mock/Spy for Serial Handler
// ===============================
// Bu mock (taklit) yapı, test sırasında serial_handler fonksiyonlarının
// çağrılıp çağrılmadığını ve hangi veriyle çağrıldığını takip eder.
typedef enum {
    MOCK_PORT_NONE,
    MOCK_PORT_USER,
    MOCK_PORT_MODULE
} mock_port_t;

struct {
    bool was_called;
    mock_port_t port;
    lynk_frame_t last_frame;
} mock_serial_spy;

void reset_serial_spy() {
    mock_serial_spy.was_called = false;
    mock_serial_spy.port = MOCK_PORT_NONE;
    memset(&mock_serial_spy.last_frame, 0, sizeof(lynk_frame_t));
}

// Bunlar gönderme fonksiyonlarının sahte (mock) implementasyonlarıdır.
static void mock_send_to_user(const lynk_frame_t* frame) {
    mock_serial_spy.was_called = true;
    mock_serial_spy.port = MOCK_PORT_USER;
    mock_serial_spy.last_frame = *frame;
}

static void mock_send_to_module(const lynk_frame_t* frame) {
    mock_serial_spy.was_called = true;
    mock_serial_spy.port = MOCK_PORT_MODULE;
    mock_serial_spy.last_frame = *frame;
}

// ===============================
// 🧪 Frame Encode/Decode Testi
// ===============================
void test_frame_codec_basic() {
    lynk_frame_t frame = {
        .start_byte = 0xA5, .start_byte_2 = 0x5A, .version = 1,
        .frame_type = 0x01, .src_id = 0x10, .dst_id = 0x20,
        .payload_len = 3, .payload = {0xAA, 0xBB, 0xCC}
    };

    uint8_t buffer[512];
    size_t len = 0;

    if (!encode_frame(&frame, buffer, &len)) {
        Serial.println("[TEST] ❌ Encode FAILED");
        return;
    }

    lynk_frame_t decoded;
    if (!decode_frame(buffer, len, &decoded)) {
        Serial.println("[TEST] ❌ Decode FAILED");
    } else {
        Serial.println("[TEST] ✅ Frame encode/decode PASSED");
    }
}

// ===============================
// ⚙️ Fabrika Ayarlarına Sıfırlama Testi
// ===============================
void test_factory_reset() {
    Serial.println("[TEST] Testing factory reset...");

    // 1. Varsayılan konfigürasyonu al ve sakla
    lynk_config_t default_cfg;
    config_manager_init_defaults();
    memcpy(&default_cfg, config_get(), sizeof(lynk_config_t));

    // 2. Konfigürasyonu değiştir ve kaydet
    lynk_config_t modified_cfg = default_cfg;
    modified_cfg.device_id = 0x99;
    modified_cfg.mode = LYNK_MODE_STATIC;
    config_manager_set(&modified_cfg);

    // 3. Değişikliğin uygulandığını doğrula (NVS'den tekrar okuyarak)
    config_manager_load();
    const lynk_config_t* current_cfg = config_get();
    if (compare_configs(current_cfg, &default_cfg)) {
        Serial.println("[TEST] ❌ Factory Reset FAILED (config did not change before reset)");
        return;
    }

    // 4. Fabrika ayarlarına sıfırla
    if (!config_manager_factory_reset()) {
        Serial.println("[TEST] ❌ Factory Reset FAILED (nvs_flash_erase failed)");
        return;
    }

    // 5. Yeniden başlatmayı simüle et (varsayılanları yüklemesi gerekir)
    config_manager_init(); 

    // 6. Konfigürasyonun varsayılanlara döndüğünü doğrula
    current_cfg = config_get();
    if (compare_configs(current_cfg, &default_cfg)) {
        Serial.println("[TEST] ✅ Factory Reset PASSED");
    } else {
        Serial.println("[TEST] ❌ Factory Reset FAILED (config did not revert to defaults)");
        Serial.printf("      Expected device_id: 0x%02X, Got: 0x%02X\n", default_cfg.device_id, current_cfg->device_id);
    }
}

// ===============================
// ⚙️ WiFi Konfigürasyon Testi (JSON)
// ===============================
void test_wifi_config_json() {
    Serial.println("[TEST] Testing WiFi config via JSON...");

    // 1. Geçerli bir JSON ile test et
    const char* valid_json = R"({
        "ssid": "MyTestSSID",
        "password": "MyPassword123"
    })";

    if (!wifi_config_apply_json(valid_json)) {
        Serial.println("[TEST] ❌ WiFi Config FAILED (apply valid json failed)");
        return;
    }

    my_wifi_config_t loaded_cfg;
    if (!wifi_config_load(&loaded_cfg)) {
        Serial.println("[TEST] ❌ WiFi Config FAILED (load after apply failed)");
        return;
    }

    if (strcmp(loaded_cfg.ssid, "MyTestSSID") == 0 && strcmp(loaded_cfg.password, "MyPassword123") == 0) {
        Serial.println("[TEST] ✅ WiFi Config (valid JSON) PASSED");
    } else {
        Serial.println("[TEST] ❌ WiFi Config FAILED (values mismatch)");
    }

    // 2. Geçersiz bir JSON ile test et (SSID eksik)
    const char* invalid_json = R"({"password": "somepass"})";
    if (wifi_config_apply_json(invalid_json)) {
        Serial.println("[TEST] ❌ WiFi Config FAILED (incorrectly accepted invalid JSON)");
    } else {
        Serial.println("[TEST] ✅ WiFi Config (invalid JSON) PASSED");
    }
}

// ===============================
// 🧪 Frame Codec Sınır Durum Testleri
// ===============================
void test_frame_codec_edge_cases() {
    Serial.println("[TEST] Testing frame codec edge cases...");
    
    uint8_t buffer[512];
    size_t len = 0;
    lynk_frame_t decoded;

    // Test: Boş payload (payload_len = 0)
    lynk_frame_t frame_no_payload = {
        .start_byte = 0xA5, .start_byte_2 = 0x5A, .version = 1,
        .frame_type = 0x01, .src_id = 0x10, .dst_id = 0x20,
        .payload_len = 0
    };
    memset(frame_no_payload.payload, 0, sizeof(frame_no_payload.payload));

    if (!encode_frame(&frame_no_payload, buffer, &len)) {
        Serial.println("[TEST] ❌ Edge Case FAILED (encode empty payload)");
        return;
    }
    if (!decode_frame(buffer, len, &decoded) || decoded.payload_len != 0) {
        Serial.println("[TEST] ❌ Edge Case FAILED (decode empty payload)");
    } else {
        Serial.println("[TEST] ✅ Edge Case PASSED (empty payload)");
    }
}

// ===============================
// ⚙️ Konfig Varsayılan Değer Testi
// ===============================
void test_config_defaults() {
    // Varsayılan değerleri yükle
    config_manager_init_defaults();
    config_manager_save();
    const lynk_config_t* cfg = config_get();
    Serial.printf("[TEST] Config Defaults: device_id=0x%02X, mode=%d, baudrate=%lu\n",
                  cfg->device_id, cfg->mode, cfg->uart_baudrate);
}

// ===============================
// ⚙️ JSON ile Konfig Uygulama
// ===============================
void test_apply_json() {
    const char* json = R"({
        "device_id": "0x42",
        "mode": "STATIC",
        "static_dst_id": "0x33",
        "uart_baudrate": "57600",
        "start_byte": "0xAB",
        "start_byte_2": "0xCD"
    })";

    if (!config_manager_apply_json(json)) {
        Serial.println("[TEST] ❌ apply_config_from_json FAILED (parse error)");
        return;
    }

    const lynk_config_t* cfg = config_get();
    if (cfg->device_id == 0x42 && cfg->mode == LYNK_MODE_STATIC &&
        cfg->static_dst_id == 0x33 && cfg->uart_baudrate == 57600 &&
        cfg->start_byte == 0xAB && cfg->start_byte_2 == 0xCD) {
        Serial.println("[TEST] ✅ apply_config_from_json PASSED");
    } else {
        Serial.println("[TEST] ❌ apply_config_from_json FAILED (values mismatch)");
    }
}

// ===============================
// 🔁 STATIC Mod Yönlendirme
// ===============================
void test_router_logic_static() {
    Serial.println("[TEST] Testing router logic in STATIC mode...");
    
    // Kurulum (Setup)
    config_manager_init_defaults();
    lynk_config_t new_cfg = *config_get();
    new_cfg.mode = LYNK_MODE_STATIC;
    new_cfg.static_dst_id = 0x55;
    config_manager_set(&new_cfg);

    lynk_frame_t frame = {};
    frame.dst_id = 0x22;  // Bu ID'nin üzerine yazılmalı

    reset_serial_spy();

    // Çalıştırma (Execute)
    frame_router_process(&frame, FRAME_SOURCE_USER);

    // Doğrulama (Verify)
    if (mock_serial_spy.was_called && 
        mock_serial_spy.port == MOCK_PORT_MODULE &&
        mock_serial_spy.last_frame.dst_id == 0x55) {
        Serial.println("[TEST] ✅ STATIC mode routing PASSED");
    } else {
        Serial.println("[TEST] ❌ STATIC mode routing FAILED");
        Serial.printf("      Spy state: called=%d, port=%d, dst_id=0x%02X\n", 
                      mock_serial_spy.was_called, mock_serial_spy.port, mock_serial_spy.last_frame.dst_id);
    }
}

// ===============================
// 🔁 DYNAMIC Mod Yönlendirme
// ===============================
void test_router_logic_dynamic() {
    Serial.println("[TEST] Testing router logic in DYNAMIC mode...");

    // Kurulum (Setup)
    config_manager_init_defaults();
    lynk_config_t new_cfg = *config_get();
    new_cfg.mode = LYNK_MODE_DYNAMIC;
    new_cfg.device_id = 0x42;
    config_manager_set(&new_cfg);

    lynk_frame_t frame_to_me = { .dst_id = 0x42 };
    lynk_frame_t frame_broadcast = { .dst_id = 0xFF };
    lynk_frame_t frame_to_other = { .dst_id = 0x33 };

    // --- Test 1: Cihaza gelen frame yönlendirilmeli ---
    reset_serial_spy();
    frame_router_process(&frame_to_me, FRAME_SOURCE_MODULE);
    if (!mock_serial_spy.was_called || mock_serial_spy.port != MOCK_PORT_USER) {
        Serial.println("[TEST] ❌ DYNAMIC mode FAILED (frame for me was not forwarded)");
        return;
    }
    Serial.println("[TEST] ✅ DYNAMIC mode PASSED (frame for me)");

    // --- Test 2: Broadcast frame yönlendirilmeli ---
    reset_serial_spy();
    frame_router_process(&frame_broadcast, FRAME_SOURCE_MODULE);
    if (!mock_serial_spy.was_called || mock_serial_spy.port != MOCK_PORT_USER) {
        Serial.println("[TEST] ❌ DYNAMIC mode FAILED (broadcast frame was not forwarded)");
        return;
    }
    Serial.println("[TEST] ✅ DYNAMIC mode PASSED (broadcast frame)");

    // --- Test 3: Başka cihaza giden frame yok sayılmalı ---
    reset_serial_spy();
    frame_router_process(&frame_to_other, FRAME_SOURCE_MODULE);
    if (mock_serial_spy.was_called) {
        Serial.println("[TEST] ❌ DYNAMIC mode FAILED (frame for other was incorrectly forwarded)");
        return;
    }
    Serial.println("[TEST] ✅ DYNAMIC mode PASSED (frame for other ignored)");
}

// ===============================
// ❌ Geçersiz Frame Testleri
// ===============================
void test_invalid_frames() {
    Serial.println("[TEST] Testing invalid frames...");

    uint8_t buffer[32];
    size_t len = 0;

    // 1. Hatalı start byte
    buffer[0] = 0x00;  // Geçersiz
    buffer[1] = 0x5A;
    buffer[2] = 0x01;  // versiyon
    buffer[3] = 0x01;  // frame_type
    buffer[4] = 0x10;  // src
    buffer[5] = 0x20;  // dst
    buffer[6] = 0x01;  // len
    buffer[7] = 0xAA;  // payload
    buffer[8] = 0x00;  // CRC (yanlış olacak)
    buffer[9] = 0x00;

    len = 10;
    lynk_frame_t decoded;
    if (!decode_frame(buffer, len, &decoded)) {
        Serial.println("[TEST] ✅ Rejected invalid frame with bad start byte");
    } else {
        Serial.println("[TEST] ❌ Incorrectly accepted frame with bad start byte");
    }

    // 2. Geçerli start byte ama hatalı CRC
    buffer[0] = 0xA5;
    buffer[1] = 0x5A;
    buffer[2] = 0x01;
    buffer[3] = 0x01;
    buffer[4] = 0x10;
    buffer[5] = 0x20;
    buffer[6] = 0x01;
    buffer[7] = 0xAA;
    buffer[8] = 0x12;  // uydurma CRC
    buffer[9] = 0x34;

    if (!decode_frame(buffer, len, &decoded)) {
        Serial.println("[TEST] ✅ Rejected frame with bad CRC");
    } else {
        Serial.println("[TEST] ❌ Incorrectly accepted frame with bad CRC");
    }

    // 3. Eksik veri (uzunluk < minimum frame size)
    if (!decode_frame(buffer, 4, &decoded)) {
        Serial.println("[TEST] ✅ Rejected too short frame");
    } else {
        Serial.println("[TEST] ❌ Incorrectly accepted too short frame");
    }

    // 4. Payload uzunluğu uyumsuz (length 2 ama 1 byte var)
    buffer[6] = 0x02;  // 2 byte payload yazıyor ama sadece 1 var
    len = 9;           // intentionally eksik bırakıldı
    if (!decode_frame(buffer, len, &decoded)) {
        Serial.println("[TEST] ✅ Rejected incomplete payload");
    } else {
        Serial.println("[TEST] ❌ Incorrectly accepted frame with missing payload bytes");
    }

    Serial.println("[TEST] Invalid frame test completed.");
}

// ===============================
// 🔄 Reset Handler Logic Test
// ===============================

// --- Mock HAL for Reset Handler Test ---
struct {
    uint32_t current_time_ms;
    int pin_state;
    bool restart_called;
    bool factory_reset_called;
    bool factory_reset_should_succeed;
} mock_platform;

void reset_mock_platform() {
    mock_platform.current_time_ms = 0;
    mock_platform.pin_state = HIGH; // Button not pressed
    mock_platform.restart_called = false;
    mock_platform.factory_reset_called = false;
    mock_platform.factory_reset_should_succeed = true;
}

uint32_t mock_get_millis() { return mock_platform.current_time_ms; }
int mock_read_pin(uint8_t pin) { return mock_platform.pin_state; }
void mock_restart() { mock_platform.restart_called = true; }
bool mock_factory_reset() { 
    mock_platform.factory_reset_called = true;
    return mock_platform.factory_reset_should_succeed;
}
void mock_hal_log(const char* tag, const char* format, ...) { /* Do nothing in test */ }

const platform_hal_t mock_hal = {
    .get_millis = mock_get_millis,
    .read_pin = mock_read_pin,
    .restart = mock_restart,
    .hal_log_i = mock_hal_log,
    .hal_log_w = mock_hal_log,
    .hal_log_e = mock_hal_log,
    .factory_reset = mock_factory_reset
};

void test_reset_handler_logic() {
    Serial.println("[TEST] Testing reset handler logic...");
    reset_handler_state_t state = {0};
    state.hal = &mock_hal;

    // --- Test Case 1: Button pressed for 5 seconds, should reset ---
    reset_mock_platform();
    mock_platform.pin_state = LOW; // Simulate button press

    // Simulate time passing for 6 seconds, calling tick every 100ms
    for (int i = 0; i < 60; i++) {
        reset_handler_tick(&state);
        mock_platform.current_time_ms += 100;
        if (mock_platform.restart_called) break;
    }

    if (mock_platform.factory_reset_called && mock_platform.restart_called) {
        Serial.println("[TEST] ✅ Reset handler PASSED (successful reset)");
    } else {
        Serial.println("[TEST] ❌ Reset handler FAILED (did not reset on long press)");
    }

    // --- Test Case 2: Button released early, should not reset ---
    reset_mock_platform();
    state.press_start_time = 0; // Reset state for new test

    mock_platform.pin_state = LOW; // Press button
    mock_platform.current_time_ms = 0;
    reset_handler_tick(&state); // Register the press
    mock_platform.current_time_ms = 3000; // 3 seconds later
    reset_handler_tick(&state); // Check state
    mock_platform.pin_state = HIGH; // Release button
    reset_handler_tick(&state); // Register the release

    if (!mock_platform.factory_reset_called && !mock_platform.restart_called) {
        Serial.println("[TEST] ✅ Reset handler PASSED (cancelled reset)");
    } else {
        Serial.println("[TEST] ❌ Reset handler FAILED (reset on short press)");
    }
}

// ===============================
// 🔗 Entegrasyon Testi: USER -> MODULE
// ===============================
void test_integration_user_to_module() {
    Serial.println("[TEST] Testing integration: USER UART to MODULE UART...");

    // 1. Kurulum (Setup): Cihazı STATIC modda yapılandır
    config_manager_init_defaults();
    lynk_config_t new_cfg = *config_get();
    new_cfg.mode = LYNK_MODE_STATIC;
    new_cfg.static_dst_id = 0xAA; // Bu ID'nin uygulanmasını bekliyoruz
    config_manager_set(&new_cfg);

    // 2. Giriş Verisi: USER UART'tan geliyormuş gibi bir frame oluştur
    lynk_frame_t input_frame = {
        .src_id = 0x10,
        .dst_id = 0x20, // Bu ID'nin override edilmesini bekliyoruz
        .payload_len = 2,
        .payload = {0xBE, 0xEF}
    };

    // Bu frame'i ham byte dizisine çevir (UART'tan bu şekilde gelecekti)
    uint8_t encoded_buffer[512];
    size_t encoded_len = 0;
    if (!encode_frame(&input_frame, encoded_buffer, &encoded_len)) {
        Serial.println("[TEST] ❌ Integration FAILED (setup: could not encode frame)");
        return;
    }

    // 3. Yönlendirme ve Yakalama (Execution & Capture)
    reset_serial_spy(); // Gönderme casusunu sıfırla

    // --- Bu blok, serial_rx_task_user'ın davranışını simüle eder ---
    // a. Gelen byte'ları decode et
    lynk_frame_t decoded_frame;
    if (decode_frame(encoded_buffer, encoded_len, &decoded_frame)) {
        // b. Decode başarılıysa, frame'i yönlendiriciye gönder
        frame_router_process(&decoded_frame, FRAME_SOURCE_USER);
    } else {
        Serial.println("[TEST] ❌ Integration FAILED (execution: could not decode frame)");
        return;
    }
    // --- Simülasyon sonu ---

    // 4. Doğrulama (Verification)
    if (mock_serial_spy.was_called &&
        mock_serial_spy.port == MOCK_PORT_MODULE &&
        mock_serial_spy.last_frame.dst_id == 0xAA &&
        mock_serial_spy.last_frame.payload[0] == 0xBE) {
        Serial.println("[TEST] ✅ Integration USER -> MODULE PASSED");
    } else {
        Serial.printf("[TEST] ❌ Integration FAILED (Spy state: called=%d, port=%d, dst_id=0x%02X)\n",
                      mock_serial_spy.was_called, mock_serial_spy.port, mock_serial_spy.last_frame.dst_id);
    }
}

// ===============================
// 🚀 Main Test Entry Point
// ===============================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("=== [LYNK TEST RUNNER STARTED] ===");

    config_manager_init();
    serial_handler_init();

    // --- Serial handler'ları test için mock fonksiyonlara yönlendir ---
    // Bu, testler için bağımlılık enjeksiyonunun temelidir.
    serial_handler_send_to_module = mock_send_to_module;
    serial_handler_send_to_user = mock_send_to_user;

    test_frame_codec_basic();
    test_config_defaults();
    test_apply_json();
    test_router_logic_static();
    test_router_logic_dynamic();
    test_invalid_frames();
    test_factory_reset();
    test_wifi_config_json();
    test_frame_codec_edge_cases();
    test_reset_handler_logic();
    test_integration_user_to_module();
}

void loop() {
    delay(2000);
}

#endif // LYNK_BUILD_TEST
