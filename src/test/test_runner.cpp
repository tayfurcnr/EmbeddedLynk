#ifdef LYNK_BUILD_TEST

#include <Arduino.h>
#include <string.h>
#include "driver/uart.h"
#include "core/uart_config.h"
#include "core/config_manager.h"
#include "codec/frame_codec.h"
#include "core/frame_router.h"
#include "net/serial_handler.h"

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
// ⚙️ Konfig Varsayılan Değer Testi
// ===============================
void test_config_defaults() {
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
    config_manager_init_defaults();
    lynk_config_t* cfg = (lynk_config_t*)config_get();
    cfg->mode = LYNK_MODE_STATIC;
    cfg->static_dst_id = 0x55;

    lynk_frame_t frame = {};
    frame.start_byte = 0xA5;
    frame.start_byte_2 = 0x5A;
    frame.version = 1;
    frame.frame_type = 0x01;
    frame.src_id = 0x11;
    frame.dst_id = 0x22;  // Override edilecek
    frame.payload_len = 2;
    frame.payload[0] = 0x10;
    frame.payload[1] = 0x20;

    Serial.println("[TEST] Routing frame in STATIC mode...");
    frame_router_process(&frame);
    Serial.printf("[TEST] Expected override dst_id: 0x%02X\n", cfg->static_dst_id);
}

// ===============================
// 🔁 DYNAMIC Mod Yönlendirme
// ===============================
void test_router_logic_dynamic() {
    config_manager_init_defaults();
    lynk_config_t* cfg = (lynk_config_t*)config_get();
    cfg->mode = LYNK_MODE_DYNAMIC;
    cfg->device_id = 0x42;

    Serial.println("[TEST] Routing frames in DYNAMIC mode...");

    lynk_frame_t frames[3];
    memset(&frames, 0, sizeof(frames));

    frames[0].dst_id = 0x42;
    frames[0].src_id = 0x01;
    frames[0].payload_len = 1;
    frames[0].payload[0] = 0xAA;

    frames[1].dst_id = 0xFF;
    frames[1].src_id = 0x02;
    frames[1].payload_len = 1;
    frames[1].payload[0] = 0xBB;

    frames[2].dst_id = 0x33;
    frames[2].src_id = 0x03;
    frames[2].payload_len = 1;
    frames[2].payload[0] = 0xCC;

    for (int i = 0; i < 3; i++) {
        frame_router_process(&frames[i]);
    }

    Serial.println("[TEST] Manually verify: only dst_id 0x42 and 0xFF should be processed");
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
// 🚀 Main Test Entry Point
// ===============================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("=== [LYNK TEST RUNNER STARTED] ===");

    config_manager_init();
    serial_handler_init();

    test_frame_codec_basic();
    test_config_defaults();
    test_apply_json();
    test_router_logic_static();
    test_router_logic_dynamic();
    test_invalid_frames();
}

void loop() {
    delay(2000);
}

#endif // LYNK_BUILD_TEST
