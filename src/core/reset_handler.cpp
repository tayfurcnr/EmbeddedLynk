#include "reset_handler.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/uart_config.h" // For RESET_BUTTON_PIN

#define TAG "RESET_HANDLER"

/**
 * @brief The core logic for checking the reset button state.
 * This function is now pure and testable, relying only on its state and the HAL.
 * @param state The current state of the handler (press time, etc.).
 */
void reset_handler_tick(reset_handler_state_t* state) {
    const platform_hal_t* hal = state->hal;

    if (hal->read_pin(RESET_BUTTON_PIN) == LOW) {
        // --- Button is pressed ---
        if (state->press_start_time == 0) {
            // Button was just pressed, record the start time.
            state->press_start_time = hal->get_millis();
            state->last_countdown_sec = 0; // Reset countdown state
            hal->hal_log_i(TAG, "Reset button pressed. Hold for 5 seconds to reset.");
        }

        uint32_t elapsed = hal->get_millis() - state->press_start_time;

        // Countdown logic to give user feedback
        int countdown_sec = 5 - (elapsed / 1000);
        if (countdown_sec < 5 && countdown_sec != state->last_countdown_sec) {
            hal->hal_log_w(TAG, "Resetting in %d...", countdown_sec > 0 ? countdown_sec : 0);
            state->last_countdown_sec = countdown_sec;
        }

        // Check if 5 seconds have passed
        if (elapsed >= 5000) {
            hal->hal_log_w(TAG, "Button held for 5 seconds. Performing factory reset NOW.");
            if (hal->factory_reset()) {
                hal->hal_log_w(TAG, "Factory reset successful. Restarting device.");
                vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for log message to be sent
                hal->restart();
            } else {
                hal->hal_log_e(TAG, "Factory reset FAILED. Continuing without reset.");
                state->press_start_time = 0; // Reset timer to prevent re-triggering
            }
        }
    } else {
        // --- Button is not pressed ---
        if (state->press_start_time != 0) {
            // Button was released before 5 seconds.
            hal->hal_log_i(TAG, "Reset cancelled by user.");
        }
        state->press_start_time = 0; // Reset timer
    }
}

/**
 * @brief The FreeRTOS task that periodically calls the handler logic.
 */
static void reset_task_fn(void* pvParameters) {
    reset_handler_state_t state = {0};
    state.hal = (const platform_hal_t*)pvParameters;
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

    for (;;) {
        reset_handler_tick(&state);
        vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100ms
    }
}

void reset_handler_init(const platform_hal_t* hal) {
    xTaskCreate(reset_task_fn, "ResetTask", 2048, (void*)hal, 2, NULL);
}