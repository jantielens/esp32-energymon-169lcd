#include "web_portal_api_brightness.h"

#include "lcd_driver.h"
#include "log_manager.h"
#include "web_portal_state.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

static void handleGetBrightness(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["brightness"] = g_web_portal_state.current_brightness;

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);
}

static void handlePostBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    (void)index;
    (void)total;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    if (error) {
        Logger.logMessagef("Portal", "Brightness JSON parse error: %s", error.c_str());
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (!doc.containsKey("brightness")) {
        request->send(400, "application/json", "{\"error\":\"Missing brightness field\"}");
        return;
    }

    int brightness = doc["brightness"];
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;

    g_web_portal_state.current_brightness = (uint8_t)brightness;
    lcd_set_backlight(g_web_portal_state.current_brightness);

    Logger.logMessagef("Portal", "Brightness set to %d%%", g_web_portal_state.current_brightness);

    JsonDocument response_doc;
    response_doc["brightness"] = g_web_portal_state.current_brightness;

    String response;
    serializeJson(response_doc, response);

    request->send(200, "application/json", response);
}

void web_portal_brightness_register_routes(AsyncWebServer* server) {
    server->on("/api/brightness", HTTP_GET, handleGetBrightness);
    server->on(
        "/api/brightness",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        handlePostBrightness
    );
}
