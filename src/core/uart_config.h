#ifndef UART_CONFIG_H
#define UART_CONFIG_H

#include "driver/uart.h"

typedef enum {
    UART_TYPE_HARDWARE = 0,
    UART_TYPE_SOFTWARE = 1
} UartType_t;

// FACTORY SETTINGS [GPIO 0]
#define RESET_BUTTON_PIN 0

// MODULE UART
#define MODULE_UART_TYPE     UART_TYPE_HARDWARE
#define MODULE_UART_PORT     UART_NUM_1           
#define MODULE_UART_TX_PIN   16
#define MODULE_UART_RX_PIN   17

// USER UART
#define USER_UART_TYPE       UART_TYPE_SOFTWARE
#define USER_UART_PORT       UART_NUM_2
#define USER_UART_TX_PIN     5
#define USER_UART_RX_PIN     4

#endif
