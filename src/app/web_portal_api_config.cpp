#include "web_portal_api_config.h"

#include "config_manager.h"
#include "log_manager.h"
#include "lcd_driver.h"
#include "web_portal_state.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

static uint32_t parse_hex_color(const char* hex_str) {
    if (!hex_str || hex_str[0] != '#' || strlen(hex_str) != 7) {
        return 0xFFFFFF;
    }

    uint32_t color = 0;
    for (int i = 1; i < 7; i++) {
        char c = hex_str[i];
        uint8_t digit;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else {
            return 0xFFFFFF;
        }

        color = (color << 4) | digit;
    }

    return color & 0xFFFFFF;
}

static void color_to_hex_string(uint32_t color, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "#%06X", color & 0xFFFFFF);
}

static void handleGetConfig(AsyncWebServerRequest *request) {
    if (!g_web_portal_state.current_config) {
        request->send(500, "application/json", "{\"error\":\"Config not initialized\"}");
        return;
    }

    DeviceConfig* cfg = g_web_portal_state.current_config;

    JsonDocument doc;
    doc["wifi_ssid"] = cfg->wifi_ssid;
    doc["wifi_password"] = "";
    doc["device_name"] = cfg->device_name;

    char sanitized[CONFIG_DEVICE_NAME_MAX_LEN];
    config_manager_sanitize_device_name(cfg->device_name, sanitized, CONFIG_DEVICE_NAME_MAX_LEN);
    doc["device_name_sanitized"] = sanitized;

    doc["fixed_ip"] = cfg->fixed_ip;
    doc["subnet_mask"] = cfg->subnet_mask;
    doc["gateway"] = cfg->gateway;
    doc["dns1"] = cfg->dns1;
    doc["dns2"] = cfg->dns2;

    doc["mqtt_broker"] = cfg->mqtt_broker;
    doc["mqtt_port"] = cfg->mqtt_port;
    doc["mqtt_username"] = cfg->mqtt_username;
    doc["mqtt_password"] = "";
    doc["mqtt_topic_solar"] = cfg->mqtt_topic_solar;
    doc["mqtt_topic_grid"] = cfg->mqtt_topic_grid;
    doc["mqtt_solar_value_path"] = cfg->mqtt_solar_value_path;
    doc["mqtt_grid_value_path"] = cfg->mqtt_grid_value_path;

    doc["lcd_brightness"] = cfg->lcd_brightness;

    doc["grid_threshold_0"] = cfg->grid_threshold[0];
    doc["grid_threshold_1"] = cfg->grid_threshold[1];
    doc["grid_threshold_2"] = cfg->grid_threshold[2];
    doc["home_threshold_0"] = cfg->home_threshold[0];
    doc["home_threshold_1"] = cfg->home_threshold[1];
    doc["home_threshold_2"] = cfg->home_threshold[2];
    doc["solar_threshold_0"] = cfg->solar_threshold[0];
    doc["solar_threshold_1"] = cfg->solar_threshold[1];
    doc["solar_threshold_2"] = cfg->solar_threshold[2];

    char color_buf[8];
    color_to_hex_string(cfg->color_good, color_buf, sizeof(color_buf));
    doc["color_good"] = color_buf;
    color_to_hex_string(cfg->color_ok, color_buf, sizeof(color_buf));
    doc["color_ok"] = color_buf;
    color_to_hex_string(cfg->color_attention, color_buf, sizeof(color_buf));
    doc["color_attention"] = color_buf;
    color_to_hex_string(cfg->color_warning, color_buf, sizeof(color_buf));
    doc["color_warning"] = color_buf;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

static void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    (void)index;
    (void)total;

    if (!g_web_portal_state.current_config) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Config not initialized\"}");
        return;
    }

    DeviceConfig* cfg = g_web_portal_state.current_config;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        Logger.logMessagef("Portal", "JSON parse error: %s", error.c_str());
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }

    if (doc.containsKey("wifi_ssid")) {
        strlcpy(cfg->wifi_ssid, doc["wifi_ssid"] | "", CONFIG_SSID_MAX_LEN);
    }

    if (doc.containsKey("wifi_password")) {
        const char* wifi_pass = doc["wifi_password"];
        if (wifi_pass && strlen(wifi_pass) > 0) {
            strlcpy(cfg->wifi_password, wifi_pass, CONFIG_PASSWORD_MAX_LEN);
        }
    }

    if (doc.containsKey("device_name")) {
        const char* device_name = doc["device_name"];
        if (device_name && strlen(device_name) > 0) {
            config_manager_sanitize_device_name(device_name, cfg->device_name, CONFIG_DEVICE_NAME_MAX_LEN);
        }
    }

    if (doc.containsKey("fixed_ip")) strlcpy(cfg->fixed_ip, doc["fixed_ip"] | "", CONFIG_IP_STR_MAX_LEN);
    if (doc.containsKey("subnet_mask")) strlcpy(cfg->subnet_mask, doc["subnet_mask"] | "", CONFIG_IP_STR_MAX_LEN);
    if (doc.containsKey("gateway")) strlcpy(cfg->gateway, doc["gateway"] | "", CONFIG_IP_STR_MAX_LEN);
    if (doc.containsKey("dns1")) strlcpy(cfg->dns1, doc["dns1"] | "", CONFIG_IP_STR_MAX_LEN);
    if (doc.containsKey("dns2")) strlcpy(cfg->dns2, doc["dns2"] | "", CONFIG_IP_STR_MAX_LEN);

    if (doc.containsKey("mqtt_broker")) strlcpy(cfg->mqtt_broker, doc["mqtt_broker"] | "", CONFIG_MQTT_BROKER_MAX_LEN);
    if (doc.containsKey("mqtt_port")) cfg->mqtt_port = doc["mqtt_port"] | 1883;
    if (doc.containsKey("mqtt_username")) strlcpy(cfg->mqtt_username, doc["mqtt_username"] | "", CONFIG_MQTT_USERNAME_MAX_LEN);

    if (doc.containsKey("mqtt_password")) {
        const char* mqtt_pass = doc["mqtt_password"];
        if (mqtt_pass && strlen(mqtt_pass) > 0) {
            strlcpy(cfg->mqtt_password, mqtt_pass, CONFIG_MQTT_PASSWORD_MAX_LEN);
        }
    }

    if (doc.containsKey("mqtt_topic_solar")) strlcpy(cfg->mqtt_topic_solar, doc["mqtt_topic_solar"] | "", CONFIG_MQTT_TOPIC_MAX_LEN);
    if (doc.containsKey("mqtt_topic_grid")) strlcpy(cfg->mqtt_topic_grid, doc["mqtt_topic_grid"] | "", CONFIG_MQTT_TOPIC_MAX_LEN);
    if (doc.containsKey("mqtt_solar_value_path")) strlcpy(cfg->mqtt_solar_value_path, doc["mqtt_solar_value_path"] | "", 32);
    if (doc.containsKey("mqtt_grid_value_path")) strlcpy(cfg->mqtt_grid_value_path, doc["mqtt_grid_value_path"] | "", 32);

    if (doc.containsKey("lcd_brightness")) {
        int brightness = doc["lcd_brightness"] | 100;
        if (brightness < 0) brightness = 0;
        if (brightness > 100) brightness = 100;
        cfg->lcd_brightness = (uint8_t)brightness;
        g_web_portal_state.current_brightness = (uint8_t)brightness;
        lcd_set_backlight(g_web_portal_state.current_brightness);
        Logger.logMessagef("Portal", "Brightness saved: %d%%", g_web_portal_state.current_brightness);
    }

    if (doc.containsKey("grid_threshold_0")) cfg->grid_threshold[0] = doc["grid_threshold_0"];
    if (doc.containsKey("grid_threshold_1")) cfg->grid_threshold[1] = doc["grid_threshold_1"];
    if (doc.containsKey("grid_threshold_2")) cfg->grid_threshold[2] = doc["grid_threshold_2"];
    if (doc.containsKey("home_threshold_0")) cfg->home_threshold[0] = doc["home_threshold_0"];
    if (doc.containsKey("home_threshold_1")) cfg->home_threshold[1] = doc["home_threshold_1"];
    if (doc.containsKey("home_threshold_2")) cfg->home_threshold[2] = doc["home_threshold_2"];
    if (doc.containsKey("solar_threshold_0")) cfg->solar_threshold[0] = doc["solar_threshold_0"];
    if (doc.containsKey("solar_threshold_1")) cfg->solar_threshold[1] = doc["solar_threshold_1"];
    if (doc.containsKey("solar_threshold_2")) cfg->solar_threshold[2] = doc["solar_threshold_2"];

    if (doc.containsKey("color_good")) {
        const char* s = doc["color_good"]; if (s) cfg->color_good = parse_hex_color(s);
    }
    if (doc.containsKey("color_ok")) {
        const char* s = doc["color_ok"]; if (s) cfg->color_ok = parse_hex_color(s);
    }
    if (doc.containsKey("color_attention")) {
        const char* s = doc["color_attention"]; if (s) cfg->color_attention = parse_hex_color(s);
    }
    if (doc.containsKey("color_warning")) {
        const char* s = doc["color_warning"]; if (s) cfg->color_warning = parse_hex_color(s);
    }

    cfg->magic = CONFIG_MAGIC;

    if (!config_manager_is_valid(cfg)) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid configuration\"}");
        return;
    }

    if (!config_manager_validate_thresholds(cfg)) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid threshold values or ordering (T0 <= T1 <= T2 required)\"}");
        return;
    }

    if (config_manager_save(cfg)) {
        Logger.logMessage("Portal", "Config saved");
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");

        if (!request->hasParam("no_reboot")) {
            Logger.logMessage("Portal", "Rebooting device");
            delay(100);
            ESP.restart();
        }
    } else {
        Logger.logMessage("Portal", "Config save failed");
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save\"}");
    }
}

static void handleDeleteConfig(AsyncWebServerRequest *request) {
    if (config_manager_reset()) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration reset\"}");
        delay(100);
        ESP.restart();
    } else {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to reset\"}");
    }
}

void web_portal_config_register_routes(AsyncWebServer* server) {
    server->on("/api/config", HTTP_GET, handleGetConfig);

    server->on(
        "/api/config",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        handlePostConfig
    );

    server->on("/api/config", HTTP_DELETE, handleDeleteConfig);
}
