#ifndef FRAME_CODEC_H
#define FRAME_CODEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define LYNK_MAX_PAYLOAD_SIZE 248

typedef struct {
    uint8_t start_byte;
    uint8_t start_byte_2;
    uint8_t version;
    uint8_t frame_type;
    uint8_t src_id;
    uint8_t dst_id;
    uint8_t payload_len;
    uint8_t payload[LYNK_MAX_PAYLOAD_SIZE];
    uint16_t crc;
} lynk_frame_t;

/**
 * @brief Bir LYNK çerçevesini byte dizisine kodlar.
 * @param frame Kodlanacak çerçeve yapısı.
 * @param buffer Çıktı tamponu.
 * @param len Çıktı tamponuna yazılan byte sayısı.
 * @return Başarılıysa true, değilse false.
 */
bool encode_frame(const lynk_frame_t* frame, uint8_t* buffer, size_t* len);

/**
 * @brief Bir byte dizisini LYNK çerçevesine çözer ve detaylı hata ayıklama logları üretir.
 * @param buffer Gelen byte dizisi.
 * @param len Gelen byte dizisinin uzunluğu.
 * @param frame Çıktı olarak doldurulacak çerçeve yapısı.
 * @return Başarılıysa true, değilse false.
 */
bool decode_frame(const uint8_t* buffer, size_t len, lynk_frame_t* frame);

#endif // FRAME_CODEC_H