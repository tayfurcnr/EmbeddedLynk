#ifndef PLATFORM_HAL_H
#define PLATFORM_HAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Donanım soyutlama yapısı
typedef struct {
    uint32_t (*get_millis)();
    int (*read_pin)(uint8_t pin);
    void (*restart)();

    void (*hal_log_i)(const char* tag, const char* format, ...);
    void (*hal_log_w)(const char* tag, const char* format, ...);
    void (*hal_log_e)(const char* tag, const char* format, ...);

    bool (*factory_reset)();
} platform_hal_t;

// HAL sağlayıcı
const platform_hal_t* platform_hal_get_real(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_HAL_H
