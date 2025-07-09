#ifndef FRAME_ROUTER_H
#define FRAME_ROUTER_H

#include "codec/frame_codec.h"

// Çerçevenin hangi arayüzden geldiğini belirtmek için enum
typedef enum {
    FRAME_SOURCE_USER,
    FRAME_SOURCE_MODULE,
    FRAME_SOURCE_WIFI
} frame_source_t;

/**
 * @brief Gelen bir LYNK çerçevesini kaynağına ve cihazın moduna göre işler ve yönlendirir.
 * 
 * @param frame Alınan LYNK çerçevesine işaretçi. Bu çerçeve yönlendirme sırasında değiştirilebilir (örn. dst_id).
 * @param source Çerçevenin alındığı kaynak arayüz.
 */
void frame_router_process(lynk_frame_t* frame, frame_source_t source);

#endif // FRAME_ROUTER_H