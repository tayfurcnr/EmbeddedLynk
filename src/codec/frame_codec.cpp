#include "frame_codec.h"
#include <string.h>

uint16_t calculate_crc(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

bool encode_frame(const lynk_frame_t* frame, uint8_t* buffer, size_t* encoded_len) {
    if (frame->payload_len > MAX_PAYLOAD_SIZE) return false;

    size_t idx = 0;
    buffer[idx++] = frame->start_byte;
    buffer[idx++] = frame->start_byte_2;
    buffer[idx++] = frame->version;
    buffer[idx++] = frame->frame_type;
    buffer[idx++] = frame->src_id;
    buffer[idx++] = frame->dst_id;
    buffer[idx++] = (frame->payload_len >> 8) & 0xFF;
    buffer[idx++] = frame->payload_len & 0xFF;
    memcpy(&buffer[idx], frame->payload, frame->payload_len);
    idx += frame->payload_len;

    uint16_t crc = calculate_crc(buffer, idx);
    buffer[idx++] = (crc >> 8) & 0xFF;
    buffer[idx++] = crc & 0xFF;

    *encoded_len = idx;
    return true;
}

bool decode_frame(const uint8_t* buffer, size_t buffer_len, lynk_frame_t* frame) {
    if (buffer_len < 10) return false;

    frame->start_byte = buffer[0];
    frame->start_byte_2 = buffer[1];
    frame->version = buffer[2];
    frame->frame_type = buffer[3];
    frame->src_id = buffer[4];
    frame->dst_id = buffer[5];
    frame->payload_len = (buffer[6] << 8) | buffer[7];

    if (frame->payload_len > MAX_PAYLOAD_SIZE || buffer_len < 8 + frame->payload_len + 2)
        return false;

    memcpy(frame->payload, &buffer[8], frame->payload_len);
    frame->crc = (buffer[8 + frame->payload_len] << 8) | buffer[9 + frame->payload_len];

    uint16_t computed_crc = calculate_crc(buffer, 8 + frame->payload_len);
    return computed_crc == frame->crc;
}
