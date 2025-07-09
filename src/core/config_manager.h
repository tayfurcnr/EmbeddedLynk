#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LYNK_MODE_STATIC = 0,
    LYNK_MODE_DYNAMIC = 1
} lynk_mode_t;

typedef struct {
    uint8_t device_id;
    lynk_mode_t mode;
    uint8_t static_dst_id;
    uint32_t uart_baudrate;
    uint8_t start_byte;
    uint8_t start_byte_2;
} lynk_config_t;

/**
 * WiFi yapı tanımı - isim çatışmasını önlemek için farklı isimlendirme kullanıldı
 */
typedef struct my_wifi_config_s {
    char ssid[32];
    char password[64];
} my_wifi_config_t;

/**
 * @brief LYNK config yapısına pointer döner
 */
const lynk_config_t* config_get(void);

/**
 * @brief Varsayılan config değerlerini yükler
 */
void config_manager_init_defaults(void);

/**
 * @brief Config yöneticisini başlatır, EEPROM'dan yükler
 */
void config_manager_init(void);

/**
 * @brief NVS'i silerek fabrika ayarlarına döndürür
 */
bool config_manager_factory_reset(void);

/**
 * @brief Yeni config ayarlarını uygular ve kaydeder
 */
void config_manager_set(const lynk_config_t* new_cfg);

/**
 * @brief Config'i NVS'ye kaydeder
 */
bool config_manager_save(void);

/**
 * @brief Config'i NVS'den yükler
 */
bool config_manager_load(void);

/**
 * @brief JSON ile gelen config ayarlarını uygular
 */
bool config_manager_apply_json(const char* json_str);

/**
 * @brief WiFi ayarlarını kaydeder
 */
bool wifi_config_save(const my_wifi_config_t* wifi_cfg);

/**
 * @brief WiFi ayarlarını yükler
 */
bool wifi_config_load(my_wifi_config_t* wifi_cfg);

/**
 * @brief JSON ile gelen WiFi ayarlarını uygular
 */
bool wifi_config_apply_json(const char* json_str);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
