#ifndef RESET_HANDLER_H
#define RESET_HANDLER_H

#include "hal/platform_hal.h" // Include the new HAL header

#ifdef __cplusplus
extern "C" {
#endif

// State structure for the reset handler logic.
// This makes the logic independent of global state.
typedef struct {
    uint32_t press_start_time;
    int last_countdown_sec;
    const platform_hal_t* hal; // Pointer to the hardware abstraction layer
} reset_handler_state_t;

/**
 * @brief Fabrika ayarlarına sıfırlama kontrol görevini başlatır.
 * Bu görev, arka planda sürekli çalışarak bir butona uzun basılmasını dinler.
 * @param hal Platform-specific functions (real or mock).
 */
void reset_handler_init(const platform_hal_t* hal);

// The core logic of the reset handler, now testable.
void reset_handler_tick(reset_handler_state_t* state);

#ifdef __cplusplus
}
#endif

#endif // RESET_HANDLER_H