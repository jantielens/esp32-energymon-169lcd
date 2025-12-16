#include "web_portal_pages.h"

#include "web_assets.h"
#include "web_portal_state.h"

#include <ESPAsyncWebServer.h>

static void handleRoot(AsyncWebServerRequest *request) {
    if (g_web_portal_state.ap_mode_active) {
        request->redirect("/network.html");
    } else {
        request->redirect("/home.html");
    }
}

static void handleHome(AsyncWebServerRequest *request) {
    if (g_web_portal_state.ap_mode_active) {
        request->redirect("/network.html");
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", home_html_gz, home_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

static void handleNetwork(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", network_html_gz, network_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

static void handleFirmware(AsyncWebServerRequest *request) {
    if (g_web_portal_state.ap_mode_active) {
        request->send(403, "text/plain", "Not available in AP mode");
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", firmware_html_gz, firmware_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

static void handleCSS(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/css", portal_css_gz, portal_css_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

static void handleJS(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", portal_js_gz, portal_js_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void web_portal_pages_register_routes(AsyncWebServer* server) {
    server->on("/", HTTP_GET, handleRoot);
    server->on("/home.html", HTTP_GET, handleHome);
    server->on("/network.html", HTTP_GET, handleNetwork);
    server->on("/firmware.html", HTTP_GET, handleFirmware);
    server->on("/portal.css", HTTP_GET, handleCSS);
    server->on("/portal.js", HTTP_GET, handleJS);
}
