#include "frame_codec.h"
#include "core/config_manager.h"
#include <string.h>
#include <Arduino.h> // Serial.printf için

#define LYNK_HEADER_SIZE 7
#define LYNK_CRC_SIZE 2
#define LYNK_MIN_FRAME_SIZE (LYNK_HEADER_SIZE + LYNK_CRC_SIZE)

static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

bool encode_frame(const lynk_frame_t* frame, uint8_t* buffer, size_t* len) {
    size_t index = 0;
    const lynk_config_t* cfg = config_get();

    buffer[index++] = cfg->start_byte;
    buffer[index++] = cfg->start_byte_2;
    buffer[index++] = frame->version;
    buffer[index++] = frame->frame_type;
    buffer[index++] = frame->src_id;
    buffer[index++] = frame->dst_id;
    buffer[index++] = frame->payload_len;

    if (frame->payload_len > 0 && frame->payload_len <= LYNK_MAX_PAYLOAD_SIZE) {
        memcpy(&buffer[index], frame->payload, frame->payload_len);
        index += frame->payload_len;
    }

    uint16_t crc = crc16(buffer, index);
    buffer[index++] = crc & 0xFF;
    buffer[index++] = (crc >> 8) & 0xFF;

    *len = index;
    return true;
}

bool decode_frame(const uint8_t* buffer, size_t len, lynk_frame_t* frame) {
    const lynk_config_t* cfg = config_get();

    // 1. Uzunluk Kontrolü
    if (len < LYNK_MIN_FRAME_SIZE) {
        Serial.printf("[DECODE_ERR] Frame too short. Min length: %d, Got: %d\n", LYNK_MIN_FRAME_SIZE, len);
        return false;
    }

    // 2. Başlangıç Byte Kontrolü
    if (buffer[0] != cfg->start_byte || buffer[1] != cfg->start_byte_2) {
        Serial.printf("[DECODE_ERR] Invalid start bytes. Expected: 0x%02X 0x%02X, Got: 0x%02X 0x%02X\n",
                      cfg->start_byte, cfg->start_byte_2, buffer[0], buffer[1]);
        return false;
    }

    // Başlık bilgilerini geçici olarak al
    uint8_t payload_len_from_header = buffer[6];

    // 3. Beklenen Toplam Uzunluk Kontrolü
    size_t expected_total_len = LYNK_HEADER_SIZE + payload_len_from_header + LYNK_CRC_SIZE;
    if (len != expected_total_len) {
        Serial.printf("[DECODE_ERR] Length mismatch. Header says payload is %d bytes (total %d), but received buffer is %d bytes.\n",
                      payload_len_from_header, expected_total_len, len);
        return false;
    }

    // 4. CRC Kontrolü
    size_t data_len_for_crc = len - LYNK_CRC_SIZE;
    uint16_t calculated_crc = crc16(buffer, data_len_for_crc);
    uint16_t received_crc = (uint16_t)(buffer[len - 1] << 8) | buffer[len - 2];

    if (calculated_crc != received_crc) {
        Serial.printf("[DECODE_ERR] CRC mismatch. Calculated: 0x%04X, Received: 0x%04X\n",
                      calculated_crc, received_crc);
        Serial.print("               Data for CRC: ");
        for(size_t i=0; i<data_len_for_crc; ++i) Serial.printf("%02X ", buffer[i]);
        Serial.println();
        return false;
    }

    // Tüm kontroller başarılı, frame yapısını doldur
    frame->start_byte   = buffer[0];
    frame->start_byte_2 = buffer[1];
    frame->version      = buffer[2];
    frame->frame_type   = buffer[3];
    frame->src_id       = buffer[4];
    frame->dst_id       = buffer[5];
    frame->payload_len  = payload_len_from_header;

    if (frame->payload_len > 0) {
        memcpy(frame->payload, &buffer[LYNK_HEADER_SIZE], frame->payload_len);
    }

    frame->crc = received_crc;

    return true;
}