#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Çalışma modları
typedef enum {
    LYNK_MODE_STATIC = 0,   // Statik hedef ID mod
    LYNK_MODE_DYNAMIC = 1   // Dinamik hedef ID mod
} LynkMode_t;

// Konfigürasyon yapısı
typedef struct {
    uint8_t device_id;         // Cihaz kimliği
    LynkMode_t mode;           // Çalışma modu (Static / Dynamic)
    uint8_t static_dst_id;     // Statik modda hedef cihaz ID'si
    uint32_t uart_baudrate;    // UART haberleşme hızı
    uint8_t start_byte;        // Başlangıç baytı 1
    uint8_t start_byte_2;      // Başlangıç baytı 2
} lynk_config_t;

/**
 * @brief Aktif konfigürasyon yapısını döner (salt okunur).
 */
const lynk_config_t* config_get(void);

/**
 * @brief EEPROM/NVS’den konfigürasyonu yükler.
 * @return Başarılı ise true, başarısız ise false.
 */
bool config_manager_load(void);

/**
 * @brief Konfigürasyonu EEPROM/NVS’ye kaydeder.
 * @return Başarılı ise true, başarısız ise false.
 */
bool config_manager_save(void);

/**
 * @brief Varsayılan konfigürasyon değerlerini set eder.
 */
void config_manager_init_defaults(void);

/**
 * @brief EEPROM/NVS başlatır, varsa kayıtlı konfigürasyonu yükler, yoksa varsayılanları atar.
 */
void config_manager_init(void);

/**
 * @brief Yeni konfigürasyonu aktif yapılandırma olarak set eder ve EEPROM/NVS’ye kaydeder.
 * @param new_cfg Yeni konfigürasyon yapısı.
 */
void config_manager_set(const lynk_config_t* new_cfg);

/**
 * @brief JSON formatındaki string’i parse ederek konfigürasyonu uygular ve kaydeder.
 * @param json_str JSON formatında konfigürasyon stringi.
 * @return Başarılı ise true, başarısız ise false.
 */
bool config_manager_apply_json(const char* json_str);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
