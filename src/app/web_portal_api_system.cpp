#include "web_portal_api_system.h"

#include "log_manager.h"
#include "web_portal_state.h"
#include "board_config.h"
#include "web_assets.h"  // PROJECT_NAME / PROJECT_DISPLAY_NAME
#include "../version.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>

// Temperature sensor support (ESP32-C3, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C6, ESP32-H2)
#if SOC_TEMP_SENSOR_SUPPORTED
#include "driver/temperature_sensor.h"
#endif

static void handleGetMode(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"mode\":\"");
    response->print(g_web_portal_state.ap_mode_active ? "core" : "full");
    response->print("\",\"ap_active\":");
    response->print(g_web_portal_state.ap_mode_active ? "true" : "false");
    response->print("}");
    request->send(response);
}

static void handleGetVersion(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"version\":\"");
    response->print(FIRMWARE_VERSION);
    response->print("\",\"build_date\":\"");
    response->print(BUILD_DATE);
    response->print("\",\"build_time\":\"");
    response->print(BUILD_TIME);
    response->print("\",\"chip_model\":\"");
    response->print(ESP.getChipModel());
    response->print("\",\"chip_revision\":");
    response->print(ESP.getChipRevision());
    response->print(",\"chip_cores\":");
    response->print(ESP.getChipCores());
    response->print(",\"cpu_freq\":");
    response->print(ESP.getCpuFreqMHz());
    response->print(",\"flash_chip_size\":");
    response->print(ESP.getFlashChipSize());
    response->print(",\"psram_size\":");
    response->print(ESP.getPsramSize());
    response->print(",\"free_heap\":");
    response->print(ESP.getFreeHeap());
    response->print(",\"sketch_size\":");
    response->print(ESP.getSketchSize());
    response->print(",\"free_sketch_space\":");
    response->print(ESP.getFreeSketchSpace());
    response->print(",\"mac_address\":\"");
    response->print(WiFi.macAddress());
    response->print("\",\"wifi_hostname\":\"");
    response->print(WiFi.getHostname());
    response->print("\",\"mdns_name\":\"");
    response->print(WiFi.getHostname());
    response->print(".local\",\"hostname\":\"");
    response->print(WiFi.getHostname());
    response->print("\",\"project_name\":\"");
    response->print(PROJECT_NAME);
    response->print("\",\"project_display_name\":\"");
    response->print(PROJECT_DISPLAY_NAME);
    response->print("\"}");
    request->send(response);
}

static void handleGetHealth(AsyncWebServerRequest *request) {
    JsonDocument doc;

    // System
    uint64_t uptime_us = esp_timer_get_time();
    doc["uptime_seconds"] = uptime_us / 1000000;

    // Reset reason
    esp_reset_reason_t reset_reason = esp_reset_reason();
    const char* reset_str = "Unknown";
    switch (reset_reason) {
        case ESP_RST_POWERON:   reset_str = "Power On"; break;
        case ESP_RST_SW:        reset_str = "Software"; break;
        case ESP_RST_PANIC:     reset_str = "Panic"; break;
        case ESP_RST_INT_WDT:   reset_str = "Interrupt WDT"; break;
        case ESP_RST_TASK_WDT:  reset_str = "Task WDT"; break;
        case ESP_RST_WDT:       reset_str = "WDT"; break;
        case ESP_RST_DEEPSLEEP: reset_str = "Deep Sleep"; break;
        case ESP_RST_BROWNOUT:  reset_str = "Brownout"; break;
        case ESP_RST_SDIO:      reset_str = "SDIO"; break;
        default: break;
    }
    doc["reset_reason"] = reset_str;

    // CPU
    doc["cpu_freq"] = ESP.getCpuFreqMHz();

    // CPU usage via IDLE task delta calculation
    TaskStatus_t task_stats[16];
    uint32_t total_runtime;
    int task_count = uxTaskGetSystemState(task_stats, 16, &total_runtime);

    uint32_t idle_runtime = 0;
    for (int i = 0; i < task_count; i++) {
        if (strstr(task_stats[i].pcTaskName, "IDLE") != nullptr) {
            idle_runtime += task_stats[i].ulRunTimeCounter;
        }
    }

    unsigned long now = millis();
    int cpu_usage = 0;

    if (g_web_portal_state.last_cpu_check > 0 && (now - g_web_portal_state.last_cpu_check) > 100) {
        uint32_t idle_delta = idle_runtime - g_web_portal_state.last_idle_runtime;
        uint32_t total_delta = total_runtime - g_web_portal_state.last_total_runtime;

        if (total_delta > 0) {
            float idle_percent = ((float)idle_delta / total_delta) * 100.0f;
            cpu_usage = (int)(100.0f - idle_percent);
            if (cpu_usage < 0) cpu_usage = 0;
            if (cpu_usage > 100) cpu_usage = 100;
        }
    }

    g_web_portal_state.last_idle_runtime = idle_runtime;
    g_web_portal_state.last_total_runtime = total_runtime;
    g_web_portal_state.last_cpu_check = now;

    doc["cpu_usage"] = cpu_usage;

#if SOC_TEMP_SENSOR_SUPPORTED
    float temp_celsius = 0;
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) == ESP_OK) {
        if (temperature_sensor_enable(temp_sensor) == ESP_OK) {
            if (temperature_sensor_get_celsius(temp_sensor, &temp_celsius) == ESP_OK) {
                doc["temperature"] = (int)temp_celsius;
            } else {
                doc["temperature"] = nullptr;
            }
            temperature_sensor_disable(temp_sensor);
        } else {
            doc["temperature"] = nullptr;
        }
        temperature_sensor_uninstall(temp_sensor);
    } else {
        doc["temperature"] = nullptr;
    }
#else
    doc["temperature"] = nullptr;
#endif

    // Memory
    doc["heap_free"] = ESP.getFreeHeap();
    doc["heap_min"] = ESP.getMinFreeHeap();
    doc["heap_size"] = ESP.getHeapSize();

    size_t largest_block = ESP.getMaxAllocHeap();
    size_t free_heap = ESP.getFreeHeap();
    float fragmentation = 0;
    if (free_heap > 0) {
        fragmentation = (1.0f - ((float)largest_block / free_heap)) * 100.0f;
    }
    doc["heap_fragmentation"] = (int)fragmentation;

    // Flash usage
    doc["flash_used"] = ESP.getSketchSize();
    doc["flash_total"] = ESP.getSketchSize() + ESP.getFreeSketchSpace();

    // WiFi stats
    if (WiFi.status() == WL_CONNECTED) {
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["wifi_channel"] = WiFi.channel();
        doc["ip_address"] = WiFi.localIP().toString();
        doc["hostname"] = WiFi.getHostname();
    } else {
        doc["wifi_rssi"] = nullptr;
        doc["wifi_channel"] = nullptr;
        doc["ip_address"] = nullptr;
        doc["hostname"] = nullptr;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

static void handleReboot(AsyncWebServerRequest *request) {
    Logger.logMessage("API", "POST /api/reboot");

    request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting device...\"}");

    delay(100);
    Logger.logMessage("Portal", "Rebooting");
    ESP.restart();
}

void web_portal_system_register_routes(AsyncWebServer* server) {
    server->on("/api/mode", HTTP_GET, handleGetMode);
    server->on("/api/info", HTTP_GET, handleGetVersion);
    server->on("/api/health", HTTP_GET, handleGetHealth);
    server->on("/api/reboot", HTTP_POST, handleReboot);
}
