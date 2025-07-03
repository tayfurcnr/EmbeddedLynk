#ifndef CONFIG_SERVER_H
#define CONFIG_SERVER_H

#include <Arduino.h>

// config_server_init() tüm platformlarda kullanılabilir
void config_server_init(void);

// notifyClients() yalnızca C++ tarafında kullanılabilir (String C'de yok)
#ifdef __cplusplus
void notifyClients(const String& msg);
#endif

#endif  // CONFIG_SERVER_H
