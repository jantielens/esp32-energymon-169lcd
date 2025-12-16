#include "web_portal_api_ota.h"

#include "log_manager.h"
#include "web_portal_state.h"

#include <Update.h>
#include <ESPAsyncWebServer.h>

static void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
        Logger.logBegin("OTA Update");
        Logger.logLinef("File: %s", filename.c_str());
        Logger.logLinef("Size: %d bytes", request->contentLength());

        g_web_portal_state.ota_in_progress = true;
        g_web_portal_state.ota_progress = 0;
        g_web_portal_state.ota_total = request->contentLength();

        if (!filename.endsWith(".bin")) {
            Logger.logEnd("Not a .bin file");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Only .bin files are supported\"}");
            g_web_portal_state.ota_in_progress = false;
            return;
        }

        size_t updateSize = (g_web_portal_state.ota_total > 0) ? g_web_portal_state.ota_total : UPDATE_SIZE_UNKNOWN;
        size_t freeSpace = ESP.getFreeSketchSpace();

        Logger.logLinef("Free space: %d bytes", freeSpace);

        if (g_web_portal_state.ota_total > 0 && g_web_portal_state.ota_total > freeSpace) {
            Logger.logEnd("Firmware too large");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Firmware too large\"}");
            g_web_portal_state.ota_in_progress = false;
            return;
        }

        if (!Update.begin(updateSize, U_FLASH)) {
            Logger.logEnd("Begin failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"OTA begin failed\"}");
            g_web_portal_state.ota_in_progress = false;
            return;
        }
    }

    if (len) {
        if (Update.write(data, len) != len) {
            Logger.logEnd("Write failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Write failed\"}");
            g_web_portal_state.ota_in_progress = false;
            return;
        }

        g_web_portal_state.ota_progress += len;

        static uint8_t last_percent = 0;
        uint8_t percent = (g_web_portal_state.ota_progress * 100) / g_web_portal_state.ota_total;
        if (percent >= last_percent + 10) {
            Logger.logLinef("Progress: %d%%", percent);
            last_percent = percent;
        }
    }

    if (final) {
        if (Update.end(true)) {
            Logger.logLinef("Written: %d bytes", g_web_portal_state.ota_progress);
            Logger.logEnd("Success - rebooting");

            request->send(200, "application/json", "{\"success\":true,\"message\":\"Update successful! Rebooting...\"}");

            delay(500);
            ESP.restart();
        } else {
            Logger.logEnd("Update failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Update failed\"}");
        }

        g_web_portal_state.ota_in_progress = false;
    }
}

void web_portal_ota_register_routes(AsyncWebServer* server) {
    server->on(
        "/api/update",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        handleOTAUpload
    );
}
