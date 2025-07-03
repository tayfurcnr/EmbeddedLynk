#ifndef SERIAL_HANDLER_H
#define SERIAL_HANDLER_H

#include "codec/frame_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UART haberleşme sistemini başlatır.
 * MODULE ve USER UART'ları başlatır.
 */
void serial_handler_init(void);

/**
 * @brief MODULE UART üzerinden frame gönderir.
 * Yazılımsal veya donanımsal UART farkını içerir.
 */
void serial_handler_send_to_module(const lynk_frame_t* frame);

/**
 * @brief USER UART üzerinden frame gönderir.
 * Yazılımsal veya donanımsal UART farkını içerir.
 */
void serial_handler_send_to_user(const lynk_frame_t* frame);

#ifdef __cplusplus
}
#endif

#endif
