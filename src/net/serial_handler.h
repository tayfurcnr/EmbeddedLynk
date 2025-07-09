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

// Fonksiyon işaretçisi tipi (dependency injection için)
typedef void (*serial_send_func_t)(const lynk_frame_t* frame);

// Bu işaretçiler gönderme fonksiyonlarını çağırmak için kullanılır.
// Ana uygulamada gerçek donanım fonksiyonlarını, testlerde ise mock fonksiyonları gösterirler.
extern serial_send_func_t serial_handler_send_to_module;
extern serial_send_func_t serial_handler_send_to_user;

#ifdef __cplusplus
}
#endif

#endif
