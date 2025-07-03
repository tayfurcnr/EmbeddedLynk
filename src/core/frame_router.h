#ifndef FRAME_ROUTER_H
#define FRAME_ROUTER_H

#include "codec/frame_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

// Gelen frame'i değerlendir ve gerekirse hedefe ilet
void frame_router_process(const lynk_frame_t* frame);

#ifdef __cplusplus
}
#endif

#endif
