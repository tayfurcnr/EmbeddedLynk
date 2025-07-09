#include "platform_hal.h"
#include <Arduino.h>
#include "esp_log.h"
#include "core/config_manager.h"
#include <stdarg.h>

// 📍 Gerçek zaman sayacı (millis)
static uint32_t real_get_millis() {
    return millis();
}

// 📍 Dijital pin okuma (reset butonu vb.)
static int real_read_pin(uint8_t pin) {
    return digitalRead(pin);
}

// 📍 Cihazı yeniden başlatma
static void real_restart() {
    ESP.restart();
}

// 📍 ESP_LOG ile loglama — Info
static void real_log_i(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_INFO, tag, format, args);
    va_end(args);
}

// 📍 ESP_LOG ile loglama — Warning
static void real_log_w(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_WARN, tag, format, args);
    va_end(args);
}

// 📍 ESP_LOG ile loglama — Error
static void real_log_e(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_ERROR, tag, format, args);
    va_end(args);
}

// 📍 Donanım soyutlama yapısı (Real HAL instance)
static const platform_hal_t real_hal = {
    .get_millis = real_get_millis,
    .read_pin = real_read_pin,
    .restart = real_restart,
    .hal_log_i = real_log_i,
    .hal_log_w = real_log_w,
    .hal_log_e = real_log_e,
    .factory_reset = config_manager_factory_reset
};

// 📍 Uygulamada kullanılmak üzere gerçek HAL nesnesini döndür
const platform_hal_t* platform_hal_get_real(void) {
    return &real_hal;
}
