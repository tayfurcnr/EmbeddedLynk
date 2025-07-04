#include "config_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

#define TAG "CONFIG_MANAGER"
#define NVS_NAMESPACE "lynk_cfg"
#define NVS_KEY "active_config"

static lynk_config_t current_config;

const lynk_config_t* config_get(void) {
    return &current_config;
}

void config_manager_init_defaults(void) {
    current_config.device_id = 0x01;
    current_config.mode = LYNK_MODE_DYNAMIC;
    current_config.static_dst_id = 0xFF;
    current_config.uart_baudrate = 115200;
    current_config.start_byte = 0xA5;
    current_config.start_byte_2 = 0x5A;
}

bool config_manager_save(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(nvs, NVS_KEY, &current_config, sizeof(current_config));
    if (err == ESP_OK) nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Config saved to NVS");
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
        ESP_LOGI(TAG, "Config loaded from NVS");
        return true;
    } else {
        ESP_LOGW(TAG, "No config in NVS, using defaults");
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

void config_manager_set(const lynk_config_t* new_cfg) {
    current_config = *new_cfg;   // Yapıyı güncelle
    config_manager_save();       // NVS'e kaydet
}

bool config_manager_apply_json(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Invalid JSON format");
        return false;
    }

    cJSON* item;

    item = cJSON_GetObjectItemCaseSensitive(root, "device_id");
    if (item) {
        if (cJSON_IsNumber(item)) current_config.device_id = (uint8_t)item->valueint;
        else if (cJSON_IsString(item)) current_config.device_id = (uint8_t)strtol(item->valuestring, NULL, 0);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (item && cJSON_IsString(item)) {
        if (strcasecmp(item->valuestring, "STATIC") == 0) current_config.mode = LYNK_MODE_STATIC;
        else current_config.mode = LYNK_MODE_DYNAMIC;
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "static_dst_id");
    if (item) {
        if (cJSON_IsNumber(item)) current_config.static_dst_id = (uint8_t)item->valueint;
        else if (cJSON_IsString(item)) current_config.static_dst_id = (uint8_t)strtol(item->valuestring, NULL, 0);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "uart_baudrate");
    if (item) {
        if (cJSON_IsNumber(item)) current_config.uart_baudrate = (uint32_t)item->valueint;
        else if (cJSON_IsString(item)) current_config.uart_baudrate = (uint32_t)strtoul(item->valuestring, NULL, 0);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "start_byte");
    if (item) {
        if (cJSON_IsNumber(item)) current_config.start_byte = (uint8_t)item->valueint;
        else if (cJSON_IsString(item)) current_config.start_byte = (uint8_t)strtol(item->valuestring, NULL, 0);
    }

    item = cJSON_GetObjectItemCaseSensitive(root, "start_byte_2");
    if (item) {
        if (cJSON_IsNumber(item)) current_config.start_byte_2 = (uint8_t)item->valueint;
        else if (cJSON_IsString(item)) current_config.start_byte_2 = (uint8_t)strtol(item->valuestring, NULL, 0);
    }

    cJSON_Delete(root);

    if (!config_manager_save()) {
        ESP_LOGE(TAG, "Failed to save config after JSON apply");
        return false;
    }

    ESP_LOGI(TAG, "Config applied from JSON");
    return true;
}
