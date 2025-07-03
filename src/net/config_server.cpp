#include <Arduino.h>
#include "config_server.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "core/config_manager.h"

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

static const char* AP_SSID = "LYNK-ConfigAP";
static const char* AP_PASS = "12345678";

void notifyClients(const String& msg) {
    ws.textAll(msg);
}

void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->opcode == WS_TEXT) {
        data[len] = 0;
        String msg = (char*)data;

        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, msg);
        if (err) {
            Serial.println("WebSocket JSON parse error");
            return;
        }

        String cmd = doc["cmd"].as<String>();
        if (cmd == "get_config") {
            const lynk_config_t* cfg = config_get();
            StaticJsonDocument<256> res;

            res["device_id"] = cfg->device_id;
            res["mode"] = cfg->mode;
            res["static_dst_id"] = cfg->static_dst_id;
            res["uart_baudrate"] = cfg->uart_baudrate;
            res["start_byte"] = cfg->start_byte;
            res["start_byte_2"] = cfg->start_byte_2;

            String respStr;
            serializeJson(res, respStr);
            ws.textAll(respStr);
        }
        else if (cmd == "set_config") {
            lynk_config_t new_cfg = *config_get();

            if (doc.containsKey("device_id")) new_cfg.device_id = doc["device_id"];
            if (doc.containsKey("mode")) new_cfg.mode = doc["mode"];
            if (doc.containsKey("static_dst_id")) new_cfg.static_dst_id = doc["static_dst_id"];
            if (doc.containsKey("uart_baudrate")) new_cfg.uart_baudrate = doc["uart_baudrate"];
            if (doc.containsKey("start_byte")) new_cfg.start_byte = doc["start_byte"];
            if (doc.containsKey("start_byte_2")) new_cfg.start_byte_2 = doc["start_byte_2"];

            config_manager_set(&new_cfg);
            config_manager_save();

            notifyClients("{\"status\":\"config_updated\"}");
        }
    }
}

// WebSocket bağlantı ve veri olayları
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        IPAddress ip = client->remoteIP();
        Serial.printf("[WS] Client connected from IP: %s\n", ip.toString().c_str());
    } else if (type == WS_EVT_DATA) {
        handleWebSocketMessage(arg, data, len);
    }
}

void config_server_init() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
            Serial.println("[WIFI] New station connected to ESP32 AP");
        }
    }, ARDUINO_EVENT_WIFI_AP_STACONNECTED);

    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // "/" isteği /main sayfasına yönlendir
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/main");
    });

    // Ana menü sayfası
    server.on("/main", HTTP_GET, [](AsyncWebServerRequest* request) {
        IPAddress clientIp = request->client()->remoteIP();
        Serial.printf("[HTTP] Client connected to /main: %s\n", clientIp.toString().c_str());
        request->send(SPIFFS, "/main.html", "text/html");
    });

    // LYNK Konfigürasyon sayfası
    server.on("/lynk-config", HTTP_GET, [](AsyncWebServerRequest* request) {
        IPAddress clientIp = request->client()->remoteIP();
        Serial.printf("[HTTP] Client connected to /lynk-config: %s\n", clientIp.toString().c_str());
        request->send(SPIFFS, "/lynk-config.html", "text/html");
    });

    // WiFi Konfigürasyon sayfası
    server.on("/wifi-config", HTTP_GET, [](AsyncWebServerRequest* request) {
        IPAddress clientIp = request->client()->remoteIP();
        Serial.printf("[HTTP] Client connected to /wifi-config: %s\n", clientIp.toString().c_str());
        request->send(SPIFFS, "/wifi-config.html", "text/html");
    });

    server.begin();
    Serial.println("HTTP + WebSocket server started");
}