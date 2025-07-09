#include "frame_router.h"
#include "config_manager.h"
#include "net/serial_handler.h" // serial_handler_send_to_... fonksiyonlarını sağlar
#include <Arduino.h>

// Genel yayın (broadcast) ID'sini tanımla
#define BROADCAST_ID 0xFF

/**
 * @brief Gelen bir LYNK çerçevesini işler ve yönlendirir.
 * 
 * Bu fonksiyon, cihazın temel yönlendirme mantığını isteklerinize göre uygular:
 * - USER'dan gelen çerçeveler her zaman MODULE'e yönlendirilir. STATIC modda hedef ID'si üzerine yazılır.
 * - MODULE'den gelen çerçeveler, yalnızca bu cihaza veya genel yayına adreslenmişse USER'a yönlendirilir.
 * - Yönlendirilen tüm çerçevelerin kaynak ID'si, bu cihazın kendi ID'si olarak ayarlanır.
 */
void frame_router_process(lynk_frame_t* frame, frame_source_t source) {
    const lynk_config_t* cfg = config_get();

    // Yönlendirmeden önce kaynak ID'yi her zaman bu cihazın ID'si olarak ayarla.
    // Bu, cihazın diğer uç noktalar açısından bir yönlendirici gibi davranmasını sağlar.
    frame->src_id = cfg->device_id;

    switch (source) {
        case FRAME_SOURCE_USER: {
            // Çerçeve, USER portundan geldi ve radyo ağına (MODULE) gönderilecek.
            Serial.println("[ROUTER] Frame from USER, forwarding to MODULE.");

            if (cfg->mode == LYNK_MODE_STATIC) {
                // STATIC modda, tüm giden çerçeveler tek bir hedefe zorlanır.
                Serial.printf("[ROUTER] STATIC mode: Overriding dst_id from 0x%02X to 0x%02X\n", frame->dst_id, cfg->static_dst_id);
                frame->dst_id = cfg->static_dst_id;
            }
            // DYNAMIC modda, USER'dan gelen orijinal dst_id korunur.

            serial_handler_send_to_module(frame);
            break;
        }

        case FRAME_SOURCE_MODULE: {
            // Çerçeve, radyo ağından (MODULE) geldi, bizim için olup olmadığını kontrol et.
            Serial.printf("[ROUTER] Frame from MODULE. Checking dst_id: 0x%02X (My ID: 0x%02X, Broadcast: 0x%02X)\n", 
                          frame->dst_id, cfg->device_id, BROADCAST_ID);

            // Çerçevenin bu cihaza veya genel yayına adreslenip adreslenmediğini kontrol et.
            if (frame->dst_id == cfg->device_id || frame->dst_id == BROADCAST_ID) {
                // Bu çerçeve bizim için. USER portuna yönlendir.
                Serial.println("[ROUTER] Frame is for me or broadcast, forwarding to USER.");
                serial_handler_send_to_user(frame);
            } else {
                // Bu çerçeve ağdaki başka bir cihaz için. Yok say.
                Serial.println("[ROUTER] Frame is for another device, ignoring.");
            }
            break;
        }

        case FRAME_SOURCE_WIFI: {
            // Gelecekteki WiFi yönlendirme mantığı için yer tutucu
            Serial.println("[ROUTER] Frame from WIFI, routing not yet implemented.");
            break;
        }
    }
}