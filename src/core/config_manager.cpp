#include "config_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <WiFi.h>

#define TAG "CONFIG_MANAGER"

#define NVS_NAMESPACE "lynk_cfg"
#define NVS_KEY "active_config"

#define NVS_WIFI_NAMESPACE "wifi_cfg"
#define NVS_WIFI_KEY "credentials"

static lynk_config_t current_config;
static my_wifi_config_t current_wifi_config;

const lynk_config_t* config_get(void) {
    return &current_config;
}

void config_manager_init_defaults(void) {
    current_config.device_id        = 0x01;
    current_config.mode             = LYNK_MODE_DYNAMIC;
    current_config.static_dst_id    = 0xFF;
    current_config.uart_baudrate    = 115200;
    current_config.start_byte       = 0xA5;
    current_config.start_byte_2     = 0x5A;
}

bool config_manager_save(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(nvs, NVS_KEY, &current_config, sizeof(current_config));
    if (err == ESP_OK) nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Lynk Config saved to NVS");
    return err == ESP_OK;
}

bool config_manager_load(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) return false;

    size_t size = sizeof(current_config);
    err = nvs_get_blob(nvs, NVS_KEY, &current_config, &size);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Lynk Config loaded from NVS");
        return true;
    } else {
        ESP_LOGW(TAG, "No Lynk config in NVS, using defaults");
        return false;
    }
}

void config_manager_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (!config_manager_load()) {
        config_manager_init_defaults();
        config_manager_save();
    }
}

bool config_manager_factory_reset(void) {
    ESP_LOGW(TAG, "Performing factory reset! Erasing NVS partition...");
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS partition. Error: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "NVS partition erased successfully.");
    return true;
}

void config_manager_set(const lynk_config_t* new_cfg) {
    current_config = *new_cfg;
    config_manager_save();
}

// --- JSON Parsing Helper Functions ---

// Helper to parse a numeric value (uint8_t) with validation
static bool parse_and_validate_uint8(cJSON* parent, const char* key, uint8_t* out_value) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!item) return true; // Not present is not an error, just skip

    long value;
    if (cJSON_IsNumber(item)) {
        value = item->valueint;
    } else if (cJSON_IsString(item) && item->valuestring != NULL) {
        char* end;
        value = strtol(item->valuestring, &end, 0);
        if (*end != '\0') { // Check if the whole string was parsed
            ESP_LOGE(TAG, "Invalid numeric string for key '%s': %s", key, item->valuestring);
            return false;
        }
    } else {
        ESP_LOGE(TAG, "Invalid type for key '%s', expected number or string.", key);
        return false;
    }

    if (value < 0 || value > UINT8_MAX) {
        ESP_LOGE(TAG, "Value for key '%s' (%ld) is out of range for uint8_t.", key, value);
        return false;
    }

    *out_value = (uint8_t)value;
    return true;
}

// Helper to parse a numeric value (uint32_t) with validation
static bool parse_and_validate_uint32(cJSON* parent, const char* key, uint32_t* out_value) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!item) return true; // Not present is not an error, just skip

    long long value; // Use long long to check range
    if (cJSON_IsNumber(item)) {
        value = (long long)item->valuedouble;
    } else if (cJSON_IsString(item) && item->valuestring != NULL) {
        char* end;
        value = strtoll(item->valuestring, &end, 0);
        if (*end != '\0') {
            ESP_LOGE(TAG, "Invalid numeric string for key '%s': %s", key, item->valuestring);
            return false;
        }
    } else {
        ESP_LOGE(TAG, "Invalid type for key '%s', expected number or string.", key);
        return false;
    }

    if (value < 0 || value > UINT32_MAX) {
        ESP_LOGE(TAG, "Value for key '%s' (%lld) is out of range for uint32_t.", key, value);
        return false;
    }

    *out_value = (uint32_t)value;
    return true;
}

// Helper to parse the mode enum
static bool parse_and_validate_mode(cJSON* parent, const char* key, lynk_mode_t* out_value) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!item) return true; // Not present is not an error, just skip

    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        ESP_LOGE(TAG, "Invalid type for key '%s', expected a string.", key);
        return false;
    }

    if (strcasecmp(item->valuestring, "STATIC") == 0) {
        *out_value = LYNK_MODE_STATIC;
    } else if (strcasecmp(item->valuestring, "DYNAMIC") == 0) {
        *out_value = LYNK_MODE_DYNAMIC;
    } else {
        ESP_LOGE(TAG, "Invalid value for 'mode': '%s'. Must be 'STATIC' or 'DYNAMIC'.", item->valuestring);
        return false;
    }
    return true;
}

bool config_manager_apply_json(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Invalid JSON format: %s", cJSON_GetErrorPtr());
        return false;
    }

    // Create a temporary copy to apply changes.
    // This ensures atomicity: we only update the main config if everything is valid.
    lynk_config_t temp_cfg = current_config;
    bool success = true;

    // Use helper functions for parsing and validation
    if (!parse_and_validate_uint8(root, "device_id", &temp_cfg.device_id)) success = false;
    if (!parse_and_validate_mode(root, "mode", &temp_cfg.mode)) success = false;
    if (!parse_and_validate_uint8(root, "static_dst_id", &temp_cfg.static_dst_id)) success = false;
    if (!parse_and_validate_uint32(root, "uart_baudrate", &temp_cfg.uart_baudrate)) success = false;
    if (!parse_and_validate_uint8(root, "start_byte", &temp_cfg.start_byte)) success = false;
    if (!parse_and_validate_uint8(root, "start_byte_2", &temp_cfg.start_byte_2)) success = false;

    cJSON_Delete(root);

    if (!success) {
        ESP_LOGE(TAG, "One or more fields in JSON are invalid. Configuration not applied.");
        return false;
    }

    // If all validations passed, apply the new config and save.
    config_manager_set(&temp_cfg);

    ESP_LOGI(TAG, "Config applied from JSON successfully.");
    return true;
}

// --- WiFi Config ---

bool wifi_config_save(const my_wifi_config_t* wifi_cfg) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(nvs, NVS_WIFI_KEY, wifi_cfg, sizeof(my_wifi_config_t));
    if (err == ESP_OK) nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "WiFi Config saved to NVS");
    return err == ESP_OK;
}

bool wifi_config_load(my_wifi_config_t* wifi_cfg) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) return false;

    size_t size = sizeof(my_wifi_config_t);
    err = nvs_get_blob(nvs, NVS_WIFI_KEY, wifi_cfg, &size);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi Config loaded from NVS");
        return true;
    } else {
        ESP_LOGW(TAG, "No WiFi config in NVS");
        return false;
    }
}

// Helper to parse and validate a string with length constraints
static bool parse_and_validate_string(cJSON* parent, const char* key, char* dest, size_t max_len, size_t min_len, bool required) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!item) {
        if (required) {
            ESP_LOGE(TAG, "Required key '%s' is missing.", key);
            return false;
        }
        return true; // Not present and not required is OK
    }

    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        ESP_LOGE(TAG, "Invalid type for key '%s', expected a string.", key);
        return false;
    }

    size_t len = strlen(item->valuestring);
    if (len < min_len) {
        ESP_LOGE(TAG, "Value for key '%s' is too short. Min length: %d, got: %d.", key, min_len, len);
        return false;
    }
    if (len >= max_len) {
        ESP_LOGE(TAG, "Value for key '%s' is too long. Max length: %d, got: %d.", key, max_len - 1, len);
        return false;
    }

    strncpy(dest, item->valuestring, max_len - 1);
    dest[max_len - 1] = '\0'; // Ensure null termination
    return true;
}

bool wifi_config_apply_json(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Invalid WiFi JSON format: %s", cJSON_GetErrorPtr());
        return false;
    }

    // Use a temporary struct to validate before saving
    my_wifi_config_t wifi_cfg = {0};
    bool success = true;

    // Validate SSID: required, 1-32 chars. sizeof(wifi_cfg.ssid) is 33.
    if (!parse_and_validate_string(root, "ssid", wifi_cfg.ssid, sizeof(wifi_cfg.ssid), 1, true)) {
        success = false;
    }

    // Validate Password: not required. If present, 0-63 chars. sizeof is 64.
    if (!parse_and_validate_string(root, "password", wifi_cfg.password, sizeof(wifi_cfg.password), 0, false)) {
        success = false;
    }

    cJSON_Delete(root);

    if (!success) {
        ESP_LOGE(TAG, "Invalid WiFi configuration data. Not saved.");
        return false;
    }

    // DEBUG: Yazılacak olan yeni WiFi ayarlarını logla
    Serial.printf("[WIFI_CONFIG] Applying new config - SSID: '%s', Password: '%s'\n", wifi_cfg.ssid, wifi_cfg.password);

    return wifi_config_save(&wifi_cfg);
}
