#ifdef LYNK_BUILD_MAIN

#include <Arduino.h>
#include "core/config_manager.h"
#include "net/serial_handler.h"
#include "net/config_server.h"
#include "core/reset_handler.h"
#include "hal/platform_hal.h"

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("=== LYNK System Starting ===");

    config_manager_init();                          // EEPROM'dan yapılandırmayı yükle
    reset_handler_init(platform_hal_get_real());    // Fabrika ayarlarına sıfırlama kontrol task'ini başlat
    serial_handler_init();                          // UART’ları kur ve RX task’lerini başlat
    config_server_init();                           // SPIFFS, Access Point, WebSocket yapılandırma arayüzü
}

void loop() {
    // Sistem FreeRTOS task’leriyle ilerliyor, loop() boş
    vTaskDelay(portMAX_DELAY);
}

#endif
