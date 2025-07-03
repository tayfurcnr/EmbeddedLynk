#ifndef FRAME_CODEC_H
#define FRAME_CODEC_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PAYLOAD_SIZE 256

typedef struct {
    uint8_t start_byte;
    uint8_t start_byte_2;
    uint8_t version;
    uint8_t frame_type;
    uint8_t src_id;
    uint8_t dst_id;
    uint16_t payload_len;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint16_t crc;
} lynk_frame_t;

bool encode_frame(const lynk_frame_t* frame, uint8_t* buffer, size_t* encoded_len);
bool decode_frame(const uint8_t* buffer, size_t buffer_len, lynk_frame_t* frame);

uint16_t calculate_crc(const uint8_t* data, size_t len);

#endif
