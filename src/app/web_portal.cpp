/*
 * Web Configuration Portal Implementation
 *
 * Composition root for the portal: creates the AsyncWebServer, sets up captive portal,
 * and wires handler modules (config/system/OTA/image/pages).
 */

// Increase AsyncTCP task stack size to prevent overflow
// Default is 8192, increase to 16384 for web assets
#define CONFIG_ASYNC_TCP_STACK_SIZE 16384

#include "web_portal.h"
#include "config_manager.h"
#include "display_manager.h"
#include "log_manager.h"

#include "image_api.h"
#include "web_portal_api_brightness.h"
#include "web_portal_api_config.h"
#include "web_portal_api_ota.h"
#include "web_portal_api_system.h"
#include "web_portal_pages.h"
#include "web_portal_state.h"

#include "web_assets.h"  // PROJECT_NAME / PROJECT_DISPLAY_NAME

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>


// Web server on port 80 (pointer to avoid constructor issues)
AsyncWebServer *server = nullptr;

// DNS server for captive portal (port 53)
DNSServer dnsServer;

// AP configuration
#define DNS_PORT 53

// Captive portal default IP
static const IPAddress CAPTIVE_PORTAL_IP(192, 168, 4, 1);

// ===== PUBLIC API =====

// Initialize web portal
void web_portal_init(DeviceConfig *config) {
    Logger.logBegin("Portal Init");

    g_web_portal_state.current_config = config;

    // Initialize current brightness from config (or default 100%)
    if (config && config->magic == CONFIG_MAGIC) {
        g_web_portal_state.current_brightness = config->lcd_brightness;
    } else {
        g_web_portal_state.current_brightness = 100;
    }
    Logger.logMessagef("Portal", "Initial brightness: %d%%", g_web_portal_state.current_brightness);
    
    // Create web server instance (avoid global constructor issues)
    if (server == nullptr) {
        yield();
        delay(100);
        
        server = new AsyncWebServer(80);
        
        yield();
        delay(100);
    }

    web_portal_pages_register_routes(server);
    web_portal_system_register_routes(server);
    web_portal_config_register_routes(server);
    web_portal_brightness_register_routes(server);
    web_portal_ota_register_routes(server);

    // Image API module (port-friendly adapter)
    ImageApiBackend backend;
    backend.hide_current_image = []() {
        display_hide_strip_image();
        display_hide_image();
    };
    backend.start_strip_session = [](int width, int height, unsigned long timeout_ms, unsigned long start_time) -> bool {
        return display_start_strip_upload(width, height, timeout_ms, start_time);
    };
    backend.decode_strip = [](const uint8_t* jpeg_data, size_t jpeg_size, uint8_t strip_index, bool output_bgr565) -> bool {
        return display_decode_strip_ex(jpeg_data, jpeg_size, strip_index, output_bgr565);
    };

    ImageApiConfig image_cfg;
    image_cfg.lcd_width = LCD_WIDTH;
    image_cfg.lcd_height = LCD_HEIGHT;
    image_cfg.max_image_size_bytes = 100 * 1024;
    image_cfg.decode_headroom_bytes = 50 * 1024;
    image_cfg.default_timeout_ms = 10000;
    image_cfg.max_timeout_ms = 86400UL * 1000UL;

    image_api_init(image_cfg, backend);
    image_api_register_routes(server);
    
    // 404 handler
    server->onNotFound([](AsyncWebServerRequest *request) {
        // In AP mode, redirect to root for captive portal
        if (g_web_portal_state.ap_mode_active) {
            request->redirect("/");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
    
    // Start server
    yield();
    delay(100);
    server->begin();
    Logger.logEnd();
}

// Start AP mode with captive portal
void web_portal_start_ap() {
    Logger.logBegin("AP Mode");
    
    // Generate AP name with chip ID
    uint32_t chipId = 0;
    for(int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    
    // Convert PROJECT_NAME to uppercase for AP SSID
    String apPrefix = String(PROJECT_NAME);
    apPrefix.toUpperCase();
    String apName = apPrefix + "-" + String(chipId, HEX);
    
    Logger.logLinef("SSID: %s", apName.c_str());
    
    // Configure AP
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(CAPTIVE_PORTAL_IP, CAPTIVE_PORTAL_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName.c_str());
    
    // Start DNS server for captive portal (redirect all DNS queries to our IP)
    dnsServer.start(DNS_PORT, "*", CAPTIVE_PORTAL_IP);
    
    WiFi.softAPsetHostname(apName.c_str());

    // Mark AP mode active so watchdog/DNS handling knows we're in captive portal
    g_web_portal_state.ap_mode_active = true;

    Logger.logLinef("IP: %s", WiFi.softAPIP().toString().c_str());
    Logger.logEnd("Captive portal active");
}

// Stop AP mode
void web_portal_stop_ap() {
    if (g_web_portal_state.ap_mode_active) {
        Logger.logMessage("Portal", "Stopping AP mode");
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        g_web_portal_state.ap_mode_active = false;
    }
}

// Handle web server (call in loop)
void web_portal_handle() {
    if (g_web_portal_state.ap_mode_active) {
        dnsServer.processNextRequest();
    }
}

// Check if in AP mode
bool web_portal_is_ap_mode() {
    return g_web_portal_state.ap_mode_active;
}

// Check if OTA update is in progress
bool web_portal_ota_in_progress() {
    return g_web_portal_state.ota_in_progress;
}

// Process pending image operations (call from main loop)
void web_portal_process_pending() {
    image_api_process_pending(g_web_portal_state.ota_in_progress);
}
