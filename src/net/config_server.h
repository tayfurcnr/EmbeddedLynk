#ifndef CONFIG_SERVER_H
#define CONFIG_SERVER_H

#include <Arduino.h>

// Initialize HTTP + WebSocket server and related handlers
void config_server_init(void);

// Send a text message to all connected WebSocket clients (C++ only)
#ifdef __cplusplus
void notifyClients(const String& msg);
#endif

#endif  // CONFIG_SERVER_H