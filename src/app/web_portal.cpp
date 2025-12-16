/*
 * Web Configuration Portal Implementation
 * 
 * Async web server with captive portal support.
 * Serves static files and provides REST API for configuration.
 */

// Increase AsyncTCP task stack size to prevent overflow
// Default is 8192, increase to 16384 for web assets
#define CONFIG_ASYNC_TCP_STACK_SIZE 16384

#include "web_portal.h"
#include "web_assets.h"
#include "config_manager.h"
#include "log_manager.h"
#include "lcd_driver.h"
#include "display_manager.h"
#include "board_config.h"
#include "../version.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ===== JPEG preflight (best-effort) =====
// TJpgDec supports only a subset of baseline JPEGs (sampling/layout).
// We do a lightweight header scan to fail fast with a descriptive error.
struct JpegSofInfo {
    bool found = false;
    bool progressive = false;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t components = 0;
    // Sampling factors (h,v) for component IDs 1(Y),2(Cb),3(Cr)
    uint8_t y_h = 0, y_v = 0;
    uint8_t cb_h = 0, cb_v = 0;
    uint8_t cr_h = 0, cr_v = 0;
};

static bool jpeg_parse_sof_best_effort(const uint8_t* data, size_t size, JpegSofInfo& out) {
    if (!data || size < 4) return false;
    // Must start with SOI
    if (!(data[0] == 0xFF && data[1] == 0xD8)) return false;

    size_t i = 2;
    while (i + 3 < size) {
        // Find marker prefix 0xFF
        if (data[i] != 0xFF) {
            i++;
            continue;
        }

        // Skip fill bytes 0xFF
        while (i < size && data[i] == 0xFF) i++;
        if (i >= size) break;

        const uint8_t marker = data[i++];

        // Standalone markers without length
        if (marker == 0xD8 || marker == 0xD9) continue; // SOI/EOI
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue; // TEM / RSTn

        // Start of Scan: header ends; no more metadata segments reliably parseable
        if (marker == 0xDA) break;

        if (i + 1 >= size) break;
        const uint16_t seg_len = (uint16_t)((data[i] << 8) | data[i + 1]);
        if (seg_len < 2) return false;
        if (i + seg_len > size) return false;

        // SOF0 (baseline DCT) or SOF2 (progressive DCT)
        if (marker == 0xC0 || marker == 0xC2) {
            out.found = true;
            out.progressive = (marker == 0xC2);
            if (seg_len < 8) return false;
            const size_t p = i + 2;
            // p+0: precision
            out.height = (uint16_t)((data[p + 1] << 8) | data[p + 2]);
            out.width  = (uint16_t)((data[p + 3] << 8) | data[p + 4]);
            out.components = data[p + 5];
            size_t cpos = p + 6;
            for (uint8_t c = 0; c < out.components; c++) {
                if (cpos + 2 >= i + seg_len) break;
                const uint8_t cid = data[cpos + 0];
                const uint8_t hv  = data[cpos + 1];
                const uint8_t h = hv >> 4;
                const uint8_t v = hv & 0x0F;
                if (cid == 1) { out.y_h = h; out.y_v = v; }
                else if (cid == 2) { out.cb_h = h; out.cb_v = v; }
                else if (cid == 3) { out.cr_h = h; out.cr_v = v; }
                cpos += 3;
            }
            return true;
        }

        // Move to next segment
        i += seg_len;
    }

    return out.found;
}

static bool jpeg_preflight_tjpgd_supported(const uint8_t* data, size_t size, char* err, size_t err_sz) {
    JpegSofInfo info;
    if (!jpeg_parse_sof_best_effort(data, size, info) || !info.found) {
        snprintf(err, err_sz, "Invalid JPEG header (missing SOF marker)");
        return false;
    }

    if (info.progressive) {
        snprintf(err, err_sz, "Unsupported JPEG: progressive encoding (use baseline JPEG)");
        return false;
    }

    // Enforce exact panel dimensions for now (simplest D1 behavior)
    if (info.width != LCD_WIDTH || info.height != LCD_HEIGHT) {
        snprintf(err, err_sz, "Unsupported JPEG dimensions: got %ux%u, expected %dx%d", (unsigned)info.width, (unsigned)info.height, LCD_WIDTH, LCD_HEIGHT);
        return false;
    }

    // Allow grayscale
    if (info.components == 1) {
        return true;
    }

    // Only accept 3-component JPEG with standard sampling patterns
    if (info.components != 3) {
        snprintf(err, err_sz, "Unsupported JPEG: expected 1 (grayscale) or 3 components, got %u", (unsigned)info.components);
        return false;
    }

    // TJpgDec expects Cb/Cr to be 1x1
    if (!(info.cb_h == 1 && info.cb_v == 1 && info.cr_h == 1 && info.cr_v == 1)) {
        snprintf(err, err_sz, "Unsupported JPEG sampling: Cb/Cr must be 1x1 (got Cb %ux%u, Cr %ux%u)",
                 (unsigned)info.cb_h, (unsigned)info.cb_v, (unsigned)info.cr_h, (unsigned)info.cr_v);
        return false;
    }

    // Y can be 1x1 (4:4:4), 2x1 (4:2:2) or 2x2 (4:2:0). Reject uncommon layouts like 1x2.
    const bool y_ok = (info.y_h == 1 && info.y_v == 1) || (info.y_h == 2 && info.y_v == 1) || (info.y_h == 2 && info.y_v == 2);
    if (!y_ok) {
        snprintf(err, err_sz, "Unsupported JPEG sampling: Y must be 1x1, 2x1, or 2x2 (got %ux%u)", (unsigned)info.y_h, (unsigned)info.y_v);
        return false;
    }

    return true;
}

static bool jpeg_preflight_tjpgd_fragment_supported(const uint8_t* data, size_t size, int expected_width, int max_height, char* err, size_t err_sz) {
    JpegSofInfo info;
    if (!jpeg_parse_sof_best_effort(data, size, info) || !info.found) {
        snprintf(err, err_sz, "Invalid JPEG header (missing SOF marker)");
        return false;
    }

    if (info.progressive) {
        snprintf(err, err_sz, "Unsupported JPEG: progressive encoding (use baseline JPEG)");
        return false;
    }

    if ((int)info.width != expected_width) {
        snprintf(err, err_sz, "Unsupported JPEG fragment width: got %u, expected %d", (unsigned)info.width, expected_width);
        return false;
    }

    if (info.height == 0 || (int)info.height > max_height || (int)info.height > LCD_HEIGHT) {
        snprintf(err, err_sz, "Unsupported JPEG fragment height: got %u (max %d)", (unsigned)info.height, max_height);
        return false;
    }

    // Allow grayscale
    if (info.components == 1) {
        return true;
    }

    // Only accept 3-component JPEG with standard sampling patterns
    if (info.components != 3) {
        snprintf(err, err_sz, "Unsupported JPEG: expected 1 (grayscale) or 3 components, got %u", (unsigned)info.components);
        return false;
    }

    // TJpgDec expects Cb/Cr to be 1x1
    if (!(info.cb_h == 1 && info.cb_v == 1 && info.cr_h == 1 && info.cr_v == 1)) {
        snprintf(err, err_sz, "Unsupported JPEG sampling: Cb/Cr must be 1x1 (got Cb %ux%u, Cr %ux%u)",
                 (unsigned)info.cb_h, (unsigned)info.cb_v, (unsigned)info.cr_h, (unsigned)info.cr_v);
        return false;
    }

    // Y can be 1x1 (4:4:4), 2x1 (4:2:2) or 2x2 (4:2:0).
    const bool y_ok = (info.y_h == 1 && info.y_v == 1) || (info.y_h == 2 && info.y_v == 1) || (info.y_h == 2 && info.y_v == 2);
    if (!y_ok) {
        snprintf(err, err_sz, "Unsupported JPEG sampling: Y must be 1x1, 2x1, or 2x2 (got %ux%u)", (unsigned)info.y_h, (unsigned)info.y_v);
        return false;
    }

    return true;
}

// Temperature sensor support (ESP32-C3, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C6, ESP32-H2)
#if SOC_TEMP_SENSOR_SUPPORTED
#include "driver/temperature_sensor.h"
#endif

// Forward declarations
void handleRoot(AsyncWebServerRequest *request);
void handleHome(AsyncWebServerRequest *request);
void handleNetwork(AsyncWebServerRequest *request);
void handleFirmware(AsyncWebServerRequest *request);
void handleCSS(AsyncWebServerRequest *request);
void handleJS(AsyncWebServerRequest *request);
void handleGetConfig(AsyncWebServerRequest *request);
void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleDeleteConfig(AsyncWebServerRequest *request);
void handleGetVersion(AsyncWebServerRequest *request);
void handleGetMode(AsyncWebServerRequest *request);
void handleGetHealth(AsyncWebServerRequest *request);
void handleReboot(AsyncWebServerRequest *request);
void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleGetBrightness(AsyncWebServerRequest *request);
void handlePostBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleImageUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleImageDelete(AsyncWebServerRequest *request);
void handleStripUpload(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

// Web server on port 80 (pointer to avoid constructor issues)
AsyncWebServer *server = nullptr;

// DNS server for captive portal (port 53)
DNSServer dnsServer;

// AP configuration
#define DNS_PORT 53
#define CAPTIVE_PORTAL_IP IPAddress(192, 168, 4, 1)

// State
static bool ap_mode_active = false;
static DeviceConfig *current_config = nullptr;
static bool ota_in_progress = false;
static size_t ota_progress = 0;
static size_t ota_total = 0;

// LCD brightness (current runtime value, may differ from saved)
static uint8_t current_brightness = 100;

// Image upload buffer (allocated temporarily during upload)
static uint8_t* image_upload_buffer = nullptr;
static size_t image_upload_size = 0;
static unsigned long image_upload_timeout_ms = 10000;  // Timeout for current upload
static const size_t MAX_IMAGE_SIZE = 100 * 1024;  // 100KB limit

// Upload state tracking
enum UploadState {
    UPLOAD_IDLE = 0,
    UPLOAD_IN_PROGRESS,
    UPLOAD_READY_TO_DISPLAY
};
static volatile UploadState upload_state = UPLOAD_IDLE;
static volatile unsigned long pending_op_id = 0;  // Incremented when new op is queued

// Pending image display operation (processed by main loop)
struct PendingImageOp {
    uint8_t* buffer;
    size_t size;
    bool dismiss;  // true = dismiss current image, false = show new image
    unsigned long timeout_ms;  // Display timeout in milliseconds
    unsigned long start_time;  // Time when upload completed (for accurate timeout)
};
static volatile PendingImageOp pending_image_op = {nullptr, 0, false, 10000, 0};

// Current strip being uploaded (stateless - metadata comes with each request)
static uint8_t* current_strip_buffer = nullptr;
static size_t current_strip_size = 0;
static int current_strip_index = -1;
static int current_strip_total = 0;

// CPU usage tracking
static uint32_t last_idle_runtime = 0;
static uint32_t last_total_runtime = 0;
static unsigned long last_cpu_check = 0;

// ===== WEB SERVER HANDLERS =====

// Handle root - redirect to network.html in AP mode, serve home in full mode
void handleRoot(AsyncWebServerRequest *request) {
    if (ap_mode_active) {
        // In AP mode, redirect to network configuration page
        request->redirect("/network.html");
    } else {
        // In full mode, serve home page
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", 
                                                                   home_html_gz, 
                                                                   home_html_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    }
}

// Serve home page
void handleHome(AsyncWebServerRequest *request) {
    if (ap_mode_active) {
        // In AP mode, redirect to network configuration page
        request->redirect("/network.html");
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", 
                                                               home_html_gz, 
                                                               home_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// Serve network configuration page
void handleNetwork(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", 
                                                               network_html_gz, 
                                                               network_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// Serve firmware update page
void handleFirmware(AsyncWebServerRequest *request) {
    if (ap_mode_active) {
        // In AP mode, redirect to network configuration page
        request->redirect("/network.html");
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", 
                                                               firmware_html_gz, 
                                                               firmware_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// Serve CSS
void handleCSS(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/css", 
                                                               portal_css_gz, 
                                                               portal_css_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// Serve JavaScript
void handleJS(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", 
                                                               portal_js_gz, 
                                                               portal_js_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// GET /api/mode - Return portal mode (core vs full)
void handleGetMode(AsyncWebServerRequest *request) {
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"mode\":\"");
    response->print(ap_mode_active ? "core" : "full");
    response->print("\",\"ap_active\":");
    response->print(ap_mode_active ? "true" : "false");
    response->print("}");
    request->send(response);
}

// Helper: Parse hex color string (#RRGGBB) to uint32_t
uint32_t parse_hex_color(const char* hex_str) {
    if (!hex_str || hex_str[0] != '#' || strlen(hex_str) != 7) {
        return 0xFFFFFF;  // Default to white on invalid input
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
            return 0xFFFFFF;  // Invalid character
        }
        
        color = (color << 4) | digit;
    }
    
    return color & 0xFFFFFF;  // Ensure 24-bit
}

// Helper: Convert uint32_t color to #RRGGBB string
void color_to_hex_string(uint32_t color, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "#%06X", color & 0xFFFFFF);
}

// GET /api/config - Return current configuration
void handleGetConfig(AsyncWebServerRequest *request) {
    
    if (!current_config) {
        request->send(500, "application/json", "{\"error\":\"Config not initialized\"}");
        return;
    }
    
    // Create JSON response (don't include passwords)
    JsonDocument doc;
    doc["wifi_ssid"] = current_config->wifi_ssid;
    doc["wifi_password"] = ""; // Don't send password
    doc["device_name"] = current_config->device_name;
    
    // Sanitized name for display
    char sanitized[CONFIG_DEVICE_NAME_MAX_LEN];
    config_manager_sanitize_device_name(current_config->device_name, sanitized, CONFIG_DEVICE_NAME_MAX_LEN);
    doc["device_name_sanitized"] = sanitized;
    
    // Fixed IP settings
    doc["fixed_ip"] = current_config->fixed_ip;
    doc["subnet_mask"] = current_config->subnet_mask;
    doc["gateway"] = current_config->gateway;
    doc["dns1"] = current_config->dns1;
    doc["dns2"] = current_config->dns2;
    
    // MQTT settings
    doc["mqtt_broker"] = current_config->mqtt_broker;
    doc["mqtt_port"] = current_config->mqtt_port;
    doc["mqtt_username"] = current_config->mqtt_username;
    doc["mqtt_password"] = ""; // Don't send password
    doc["mqtt_topic_solar"] = current_config->mqtt_topic_solar;
    doc["mqtt_topic_grid"] = current_config->mqtt_topic_grid;
    doc["mqtt_solar_value_path"] = current_config->mqtt_solar_value_path;
    doc["mqtt_grid_value_path"] = current_config->mqtt_grid_value_path;
    
    // LCD settings
    doc["lcd_brightness"] = current_config->lcd_brightness;
    
    // Power thresholds
    doc["grid_threshold_0"] = current_config->grid_threshold[0];
    doc["grid_threshold_1"] = current_config->grid_threshold[1];
    doc["grid_threshold_2"] = current_config->grid_threshold[2];
    doc["home_threshold_0"] = current_config->home_threshold[0];
    doc["home_threshold_1"] = current_config->home_threshold[1];
    doc["home_threshold_2"] = current_config->home_threshold[2];
    doc["solar_threshold_0"] = current_config->solar_threshold[0];
    doc["solar_threshold_1"] = current_config->solar_threshold[1];
    doc["solar_threshold_2"] = current_config->solar_threshold[2];
    
    // Color configuration (convert to hex strings)
    char color_buf[8];
    color_to_hex_string(current_config->color_good, color_buf, sizeof(color_buf));
    doc["color_good"] = color_buf;
    color_to_hex_string(current_config->color_ok, color_buf, sizeof(color_buf));
    doc["color_ok"] = color_buf;
    color_to_hex_string(current_config->color_attention, color_buf, sizeof(color_buf));
    doc["color_attention"] = color_buf;
    color_to_hex_string(current_config->color_warning, color_buf, sizeof(color_buf));
    doc["color_warning"] = color_buf;
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

// POST /api/config - Save new configuration
void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    
    if (!current_config) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Config not initialized\"}");
        return;
    }
    
    // Parse JSON body
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        Logger.logMessagef("Portal", "JSON parse error: %s", error.c_str());
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Partial update: only update fields that are present in the request
    // This allows different pages to update only their relevant fields
    
    // WiFi SSID - only update if field exists in JSON
    if (doc.containsKey("wifi_ssid")) {
        strlcpy(current_config->wifi_ssid, doc["wifi_ssid"] | "", CONFIG_SSID_MAX_LEN);
    }
    
    // WiFi password - only update if provided and not empty
    if (doc.containsKey("wifi_password")) {
        const char* wifi_pass = doc["wifi_password"];
        if (wifi_pass && strlen(wifi_pass) > 0) {
            strlcpy(current_config->wifi_password, wifi_pass, CONFIG_PASSWORD_MAX_LEN);
        }
    }
    
    // Device name - only update if field exists
    if (doc.containsKey("device_name")) {
        const char* device_name = doc["device_name"];
        if (device_name && strlen(device_name) > 0) {
            strlcpy(current_config->device_name, device_name, CONFIG_DEVICE_NAME_MAX_LEN);
        }
    }
    
    // Fixed IP settings - only update if fields exist
    if (doc.containsKey("fixed_ip")) {
        strlcpy(current_config->fixed_ip, doc["fixed_ip"] | "", CONFIG_IP_STR_MAX_LEN);
    }
    if (doc.containsKey("subnet_mask")) {
        strlcpy(current_config->subnet_mask, doc["subnet_mask"] | "", CONFIG_IP_STR_MAX_LEN);
    }
    if (doc.containsKey("gateway")) {
        strlcpy(current_config->gateway, doc["gateway"] | "", CONFIG_IP_STR_MAX_LEN);
    }
    if (doc.containsKey("dns1")) {
        strlcpy(current_config->dns1, doc["dns1"] | "", CONFIG_IP_STR_MAX_LEN);
    }
    if (doc.containsKey("dns2")) {
        strlcpy(current_config->dns2, doc["dns2"] | "", CONFIG_IP_STR_MAX_LEN);
    }
    
    // MQTT settings - only update if fields exist
    if (doc.containsKey("mqtt_broker")) {
        strlcpy(current_config->mqtt_broker, doc["mqtt_broker"] | "", CONFIG_MQTT_BROKER_MAX_LEN);
    }
    if (doc.containsKey("mqtt_port")) {
        current_config->mqtt_port = doc["mqtt_port"] | 1883;
    }
    if (doc.containsKey("mqtt_username")) {
        strlcpy(current_config->mqtt_username, doc["mqtt_username"] | "", CONFIG_MQTT_USERNAME_MAX_LEN);
    }
    // MQTT password - only update if provided and not empty
    if (doc.containsKey("mqtt_password")) {
        const char* mqtt_pass = doc["mqtt_password"];
        if (mqtt_pass && strlen(mqtt_pass) > 0) {
            strlcpy(current_config->mqtt_password, mqtt_pass, CONFIG_MQTT_PASSWORD_MAX_LEN);
        }
    }
    if (doc.containsKey("mqtt_topic_solar")) {
        strlcpy(current_config->mqtt_topic_solar, doc["mqtt_topic_solar"] | "", CONFIG_MQTT_TOPIC_MAX_LEN);
    }
    if (doc.containsKey("mqtt_topic_grid")) {
        strlcpy(current_config->mqtt_topic_grid, doc["mqtt_topic_grid"] | "", CONFIG_MQTT_TOPIC_MAX_LEN);
    }
    if (doc.containsKey("mqtt_solar_value_path")) {
        strlcpy(current_config->mqtt_solar_value_path, doc["mqtt_solar_value_path"] | "", 32);
    }
    if (doc.containsKey("mqtt_grid_value_path")) {
        strlcpy(current_config->mqtt_grid_value_path, doc["mqtt_grid_value_path"] | "", 32);
    }
    
    // LCD brightness - update both config and runtime, apply to hardware
    if (doc.containsKey("lcd_brightness")) {
        int brightness = doc["lcd_brightness"] | 100;
        if (brightness < 0) brightness = 0;
        if (brightness > 100) brightness = 100;
        current_config->lcd_brightness = (uint8_t)brightness;
        current_brightness = (uint8_t)brightness;
        lcd_set_backlight(current_brightness);
        Logger.logMessagef("Portal", "Brightness saved: %d%%", current_brightness);
    }
    
    // Power thresholds - update if fields exist
    if (doc.containsKey("grid_threshold_0")) {
        current_config->grid_threshold[0] = doc["grid_threshold_0"];
    }
    if (doc.containsKey("grid_threshold_1")) {
        current_config->grid_threshold[1] = doc["grid_threshold_1"];
    }
    if (doc.containsKey("grid_threshold_2")) {
        current_config->grid_threshold[2] = doc["grid_threshold_2"];
    }
    if (doc.containsKey("home_threshold_0")) {
        current_config->home_threshold[0] = doc["home_threshold_0"];
    }
    if (doc.containsKey("home_threshold_1")) {
        current_config->home_threshold[1] = doc["home_threshold_1"];
    }
    if (doc.containsKey("home_threshold_2")) {
        current_config->home_threshold[2] = doc["home_threshold_2"];
    }
    if (doc.containsKey("solar_threshold_0")) {
        current_config->solar_threshold[0] = doc["solar_threshold_0"];
    }
    if (doc.containsKey("solar_threshold_1")) {
        current_config->solar_threshold[1] = doc["solar_threshold_1"];
    }
    if (doc.containsKey("solar_threshold_2")) {
        current_config->solar_threshold[2] = doc["solar_threshold_2"];
    }
    
    // Color configuration - parse hex strings to uint32_t
    if (doc.containsKey("color_good")) {
        const char* color_str = doc["color_good"];
        if (color_str) {
            current_config->color_good = parse_hex_color(color_str);
        }
    }
    if (doc.containsKey("color_ok")) {
        const char* color_str = doc["color_ok"];
        if (color_str) {
            current_config->color_ok = parse_hex_color(color_str);
        }
    }
    if (doc.containsKey("color_attention")) {
        const char* color_str = doc["color_attention"];
        if (color_str) {
            current_config->color_attention = parse_hex_color(color_str);
        }
    }
    if (doc.containsKey("color_warning")) {
        const char* color_str = doc["color_warning"];
        if (color_str) {
            current_config->color_warning = parse_hex_color(color_str);
        }
    }
    
    current_config->magic = CONFIG_MAGIC;
    
    // Validate basic config structure
    if (!config_manager_is_valid(current_config)) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid configuration\"}");
        return;
    }
    
    // Validate thresholds
    if (!config_manager_validate_thresholds(current_config)) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid threshold values or ordering (T0 <= T1 <= T2 required)\"}");
        return;
    }
    
    // Save to NVS
    if (config_manager_save(current_config)) {
        Logger.logMessage("Portal", "Config saved");
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
        
        // Check for no_reboot parameter
        if (!request->hasParam("no_reboot")) {
            Logger.logMessage("Portal", "Rebooting device");
            // Schedule reboot after response is sent
            delay(100);
            ESP.restart();
        }
    } else {
        Logger.logMessage("Portal", "Config save failed");
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save\"}");
    }
}

// DELETE /api/config - Reset configuration
void handleDeleteConfig(AsyncWebServerRequest *request) {
    
    if (config_manager_reset()) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration reset\"}");
        
        // Schedule reboot after response is sent
        delay(100);
        ESP.restart();
    } else {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to reset\"}");
    }
}

// GET /api/info - Get device information
void handleGetVersion(AsyncWebServerRequest *request) {
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

// GET /api/health - Get device health statistics
void handleGetHealth(AsyncWebServerRequest *request) {
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
    
    // Calculate CPU usage based on delta since last measurement
    unsigned long now = millis();
    int cpu_usage = 0;
    
    if (last_cpu_check > 0 && (now - last_cpu_check) > 100) {  // Minimum 100ms between measurements
        uint32_t idle_delta = idle_runtime - last_idle_runtime;
        uint32_t total_delta = total_runtime - last_total_runtime;
        
        if (total_delta > 0) {
            float idle_percent = ((float)idle_delta / total_delta) * 100.0;
            cpu_usage = (int)(100.0 - idle_percent);
            // Clamp to valid range
            if (cpu_usage < 0) cpu_usage = 0;
            if (cpu_usage > 100) cpu_usage = 100;
        }
    }
    
    // Update tracking variables
    last_idle_runtime = idle_runtime;
    last_total_runtime = total_runtime;
    last_cpu_check = now;
    
    doc["cpu_usage"] = cpu_usage;
    
    // Temperature - Internal sensor (supported on ESP32-C3, S2, S3, C2, C6, H2)
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
    // Original ESP32 and other chips without temp sensor support
    doc["temperature"] = nullptr;
#endif
    
    // Memory
    doc["heap_free"] = ESP.getFreeHeap();
    doc["heap_min"] = ESP.getMinFreeHeap();
    doc["heap_size"] = ESP.getHeapSize();
    
    // Heap fragmentation calculation
    size_t largest_block = ESP.getMaxAllocHeap();
    size_t free_heap = ESP.getFreeHeap();
    float fragmentation = 0;
    if (free_heap > 0) {
        fragmentation = (1.0 - ((float)largest_block / free_heap)) * 100.0;
    }
    doc["heap_fragmentation"] = (int)fragmentation;
    
    // Flash usage
    doc["flash_used"] = ESP.getSketchSize();
    doc["flash_total"] = ESP.getSketchSize() + ESP.getFreeSketchSpace();
    
    // WiFi stats (only if connected)
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

// POST /api/reboot - Reboot device without saving
void handleReboot(AsyncWebServerRequest *request) {
    Logger.logMessage("API", "POST /api/reboot");
    
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting device...\"}");
    
    // Schedule reboot after response is sent
    delay(100);
    Logger.logMessage("Portal", "Rebooting");
    ESP.restart();
}

// POST /api/update - Handle OTA firmware upload
void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    // First chunk - initialize OTA
    if (index == 0) {

        
        Logger.logBegin("OTA Update");
        Logger.logLinef("File: %s", filename.c_str());
        Logger.logLinef("Size: %d bytes", request->contentLength());
        
        ota_in_progress = true;
        ota_progress = 0;
        ota_total = request->contentLength();
        
        // Check if filename ends with .bin
        if (!filename.endsWith(".bin")) {
            Logger.logEnd("Not a .bin file");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Only .bin files are supported\"}");
            ota_in_progress = false;
            return;
        }
        
        // Get OTA partition size
        size_t updateSize = (ota_total > 0) ? ota_total : UPDATE_SIZE_UNKNOWN;
        size_t freeSpace = ESP.getFreeSketchSpace();
        
        Logger.logLinef("Free space: %d bytes", freeSpace);
        
        // Validate size before starting
        if (ota_total > 0 && ota_total > freeSpace) {
            Logger.logEnd("Firmware too large");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Firmware too large\"}");
            ota_in_progress = false;
            return;
        }
        
        // Begin OTA update
        if (!Update.begin(updateSize, U_FLASH)) {
            Logger.logEnd("Begin failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"OTA begin failed\"}");
            ota_in_progress = false;
            return;
        }
    }
    
    // Write chunk to flash
    if (len) {
        if (Update.write(data, len) != len) {
            Logger.logEnd("Write failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Write failed\"}");
            ota_in_progress = false;
            return;
        }
        
        ota_progress += len;
        
        // Print progress every 10%
        static uint8_t last_percent = 0;
        uint8_t percent = (ota_progress * 100) / ota_total;
        if (percent >= last_percent + 10) {
            Logger.logLinef("Progress: %d%%", percent);
            last_percent = percent;
        }
    }
    
    // Final chunk - complete OTA
    if (final) {
        if (Update.end(true)) {
            Logger.logLinef("Written: %d bytes", ota_progress);
            Logger.logEnd("Success - rebooting");
            
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Update successful! Rebooting...\"}");
            
            delay(500);
            ESP.restart();
        } else {
            Logger.logEnd("Update failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Update failed\"}");
        }
        
        ota_in_progress = false;
    }
}

// GET /api/brightness - Return current brightness
void handleGetBrightness(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["brightness"] = current_brightness;
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

// POST /api/brightness - Update brightness in real-time (not persisted)
void handlePostBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Parse JSON body
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        Logger.logMessagef("Portal", "Brightness JSON parse error: %s", error.c_str());
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Validate brightness field exists
    if (!doc.containsKey("brightness")) {
        request->send(400, "application/json", "{\"error\":\"Missing brightness field\"}");
        return;
    }
    
    // Get brightness value and clamp to 0-100 range
    int brightness = doc["brightness"];
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    
    // Update runtime brightness and apply to hardware
    current_brightness = (uint8_t)brightness;
    lcd_set_backlight(current_brightness);
    
    Logger.logMessagef("Portal", "Brightness set to %d%%", current_brightness);
    
    // Return success with current value
    JsonDocument response_doc;
    response_doc["brightness"] = current_brightness;
    
    String response;
    serializeJson(response_doc, response);
    
    request->send(200, "application/json", response);
}

// POST /api/display/image - Upload and display JPEG image
void handleImageUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    // First chunk - initialize upload
    if (index == 0) {
        // Check if upload already in progress - wait for it to complete
        if (upload_state == UPLOAD_IN_PROGRESS) {
            Logger.logMessage("Upload", "Another upload in progress, waiting...");
            
            // Wait for current upload to complete (with 1 second timeout)
            unsigned long wait_start = millis();
            while (upload_state == UPLOAD_IN_PROGRESS && (millis() - wait_start) < 1000) {
                delay(10);  // Yield to other tasks
            }
            
            // Check if we timed out
            if (upload_state == UPLOAD_IN_PROGRESS) {
                Logger.logMessage("Upload", "ERROR: Timeout waiting for previous upload");
                request->send(409, "application/json", "{\"success\":false,\"message\":\"Previous upload still in progress after timeout\"}");
                return;
            }
            
            Logger.logMessage("Upload", "Previous upload completed, proceeding");
        }
        
        Logger.logBegin("Image Upload");
        Logger.logLinef("Filename: %s", filename.c_str());
        Logger.logLinef("Total size: %u bytes", request->contentLength());
        
        // Parse optional timeout parameter from query string (e.g., ?timeout=30)
        unsigned long timeout_seconds = 10;  // Default 10 seconds
        if (request->hasParam("timeout")) {
            String timeout_str = request->getParam("timeout")->value();
            timeout_seconds = timeout_str.toInt();
            
            // Validate timeout range: 0 = no timeout, 1-86400 seconds (24 hours max)
            // 0 means image stays permanently (no auto-dismiss)
            if (timeout_seconds > 86400) timeout_seconds = 86400;
            
            Logger.logLinef("Timeout: %lu seconds (from query parameter)", timeout_seconds);
        }
        unsigned long timeout_ms = timeout_seconds * 1000;
        
        // Store timeout for later use when queuing the operation
        image_upload_timeout_ms = timeout_ms;
        
        Logger.logLinef("Free heap before clear: %u bytes", ESP.getFreeHeap());
        
        // Free any pending image buffer to make room for new upload
        // This handles the case where an upload was queued but not yet displayed
        if (pending_image_op.buffer) {
            Logger.logMessage("Upload", "Freeing pending image buffer");
            free((void*)pending_image_op.buffer);
            pending_image_op.buffer = nullptr;
            pending_image_op.size = 0;
        }
        
        // Hide currently displayed image and return to power screen
        // This clears the buffer to free memory AND prevents showing garbage on screen
        // Note: This does LVGL operations from AsyncWebServer task, but so does
        // display_show_image() later, and the deferred pattern handles the critical parts
        display_hide_strip_image();
        display_hide_image();
        
        Logger.logLinef("Free heap after clear: %u bytes", ESP.getFreeHeap());
        
        // Check file size
        size_t total_size = request->contentLength();
        if (total_size > MAX_IMAGE_SIZE) {
            Logger.logEnd("ERROR: Image too large");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Image too large (max 100KB)\"}");
            return;
        }
        
        // Check available memory
        size_t free_heap = ESP.getFreeHeap();
        size_t required_heap = total_size + 50000;  // Need extra headroom for decoding
        if (free_heap < required_heap) {
            Logger.logLinef("ERROR: Insufficient memory (need %u, have %u)", required_heap, free_heap);
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), 
                     "{\"success\":false,\"message\":\"Insufficient memory: need %uKB, have %uKB. Try reducing image size.\"}", 
                     required_heap / 1024, free_heap / 1024);
            Logger.logEnd("");
            request->send(507, "application/json", error_msg);
            return;
        }
        
        // Allocate buffer
        image_upload_buffer = (uint8_t*)malloc(total_size);
        if (!image_upload_buffer) {
            Logger.logEnd("ERROR: Memory allocation failed");
            request->send(507, "application/json", "{\"success\":false,\"message\":\"Memory allocation failed\"}");
            return;
        }
        
        image_upload_size = 0;
        upload_state = UPLOAD_IN_PROGRESS;
    }
    
    // Receive data chunks
    if (len && image_upload_buffer && upload_state == UPLOAD_IN_PROGRESS) {
        memcpy(image_upload_buffer + image_upload_size, data, len);
        image_upload_size += len;
        
        // Log progress every 10KB
        static size_t last_logged_size = 0;
        if (image_upload_size - last_logged_size >= 10240) {
            Logger.logLinef("Received: %u bytes", image_upload_size);
            last_logged_size = image_upload_size;
        }
    }
    
    // Final chunk - validate and queue for display
    if (final) {
        if (image_upload_buffer && image_upload_size > 0 && upload_state == UPLOAD_IN_PROGRESS) {
            Logger.logLinef("Upload complete: %u bytes", image_upload_size);
            
            // Validate plain JPEG magic bytes
            // JPEG: FF D8 FF
            bool is_jpeg = false;
            
            if (image_upload_size >= 3 && 
                image_upload_buffer[0] == 0xFF && 
                image_upload_buffer[1] == 0xD8 && 
                image_upload_buffer[2] == 0xFF) {
                is_jpeg = true;
                Logger.logMessage("Upload", "Detected JPEG format");
            }
            
            if (!is_jpeg) {
                Logger.logLinef("Invalid header: %02X %02X %02X %02X", 
                    image_upload_buffer[0], image_upload_buffer[1], 
                    image_upload_buffer[2], image_upload_buffer[3]);
                Logger.logEnd("ERROR: Not a valid JPEG file");
                free(image_upload_buffer);
                image_upload_buffer = nullptr;
                image_upload_size = 0;
                upload_state = UPLOAD_IDLE;
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JPEG file\"}");
                return;
            }

            // Best-effort header preflight so we can return a descriptive 400 before queuing
            char preflight_err[160];
            if (!jpeg_preflight_tjpgd_supported(image_upload_buffer, image_upload_size, preflight_err, sizeof(preflight_err))) {
                Logger.logLinef("ERROR: JPEG preflight failed: %s", preflight_err);
                Logger.logEnd();
                free(image_upload_buffer);
                image_upload_buffer = nullptr;
                image_upload_size = 0;
                upload_state = UPLOAD_IDLE;

                char resp[256];
                snprintf(resp, sizeof(resp), "{\"success\":false,\"message\":\"%s\"}", preflight_err);
                request->send(400, "application/json", resp);
                return;
            }
            
            // Queue image for display by main loop (deferred operation)
            // If there's already a pending op, free it (replace with new image)
            if (pending_image_op.buffer) {
                Logger.logMessage("Upload", "Replacing pending image");
                free((void*)pending_image_op.buffer);
            }
            
            pending_image_op.buffer = image_upload_buffer;
            pending_image_op.size = image_upload_size;
            pending_image_op.dismiss = false;
            pending_image_op.timeout_ms = image_upload_timeout_ms;
            pending_image_op.start_time = millis();  // Capture time when upload completes
            pending_op_id++;
            upload_state = UPLOAD_READY_TO_DISPLAY;
            
            // Don't free buffer - main loop will handle it
            image_upload_buffer = nullptr;
            image_upload_size = 0;
            
            Logger.logEnd("Image queued for display");
            
            // Dynamic response message with actual timeout
            char response_msg[128];
            snprintf(response_msg, sizeof(response_msg), 
                     "{\"success\":true,\"message\":\"Image queued for display (%lus timeout)\"}", 
                     image_upload_timeout_ms / 1000);
            request->send(200, "application/json", response_msg);
        } else {
            Logger.logEnd("ERROR: No data received");
            upload_state = UPLOAD_IDLE;
            request->send(400, "application/json", "{\"success\":false,\"message\":\"No data received\"}");
        }
    }
}

// DELETE /api/display/image - Manually dismiss image and return to power screen
void handleImageDelete(AsyncWebServerRequest *request) {
    Logger.logMessage("Portal", "Image dismiss requested");
    
    // Queue dismiss operation for main loop
    if (pending_image_op.buffer) {
        free((void*)pending_image_op.buffer);
    }
    pending_image_op.buffer = nullptr;
    pending_image_op.size = 0;
    pending_image_op.dismiss = true;
    upload_state = UPLOAD_READY_TO_DISPLAY;  // Reuse state to trigger processing
    pending_op_id++;  // Ensure main loop processes this dismiss request
    
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Image dismiss queued\"}");
}

// POST /api/display/image/chunks?index=N&total=T&width=W&height=H[&timeout=MS] - Upload a single JPEG chunk/fragment (stateless/atomic)
void handleStripUpload(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Extract required metadata from query parameters.
    // Be explicit about reading URL (query) params on POST requests.
    if (index == 0) {
        const bool has_required =
            request->hasParam("index", false) &&
            request->hasParam("total", false) &&
            request->hasParam("width", false) &&
            request->hasParam("height", false);

        if (!has_required) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing required parameters: index, total, width, height\"}");
            return;
        }
    }

    if (!request->hasParam("index", false) || !request->hasParam("total", false) ||
        !request->hasParam("width", false) || !request->hasParam("height", false)) {
        // If parameters are missing mid-request, fail closed.
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing required parameters: index, total, width, height\"}");
        return;
    }

    int stripIndex = request->getParam("index", false)->value().toInt();
    int totalStrips = request->getParam("total", false)->value().toInt();
    int imageWidth = request->getParam("width", false)->value().toInt();
    int imageHeight = request->getParam("height", false)->value().toInt();
    unsigned long timeoutMs = request->hasParam("timeout", false) ?
        request->getParam("timeout", false)->value().toInt() * 1000 : 10000;
    
    if (index == 0) {
        // First chunk of this strip
        Logger.logBegin("Strip Upload");
        Logger.logLinef("Strip %d/%d, size: %u bytes, image: %dx%d", 
                       stripIndex, totalStrips - 1, total, imageWidth, imageHeight);
        
        // Validate parameters
        if (stripIndex < 0 || stripIndex >= totalStrips) {
            Logger.logEnd("ERROR: Invalid strip index");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid strip index\"}");
            return;
        }
        
        if (imageWidth <= 0 || imageHeight <= 0 || imageWidth > LCD_WIDTH || imageHeight > LCD_HEIGHT) {
            Logger.logLinef("ERROR: Invalid dimensions %dx%d", imageWidth, imageHeight);
            Logger.logEnd();
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid image dimensions\"}");
            return;
        }
        
        // Initialize display session on first strip
        if (stripIndex == 0) {
            Logger.logLinef("First strip - initializing display session");
            if (!display_start_strip_upload(imageWidth, imageHeight, timeoutMs, millis())) {
                Logger.logEnd("ERROR: Failed to initialize display");
                request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to initialize display\"}");
                return;
            }
        }
        
        // Free any previous strip buffer
        if (current_strip_buffer) {
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
        }
        
        // Allocate buffer for this strip
        current_strip_buffer = (uint8_t*)malloc(total);
        
        if (!current_strip_buffer) {
            Logger.logLinef("ERROR: Out of memory (requested %u bytes, free heap: %u)", 
                           total, ESP.getFreeHeap());
            Logger.logEnd();
            request->send(507, "application/json", "{\"success\":false,\"message\":\"Out of memory\"}");
            return;
        }
        
        Logger.logLinef("Allocated %u bytes at %p (free heap: %u)", 
                       total, current_strip_buffer, ESP.getFreeHeap());
        
        current_strip_size = 0;
        current_strip_index = stripIndex;
    }
    
    // Accumulate strip data
    if (current_strip_buffer && current_strip_size + len <= total) {
        memcpy((uint8_t*)current_strip_buffer + current_strip_size, data, len);
        current_strip_size += len;
    }
    
    // Check if this is the final chunk - decode synchronously before returning
    if (index + len >= total) {
        Logger.logLinef("Strip %d complete: %u bytes received (expected %u)", 
                       stripIndex, current_strip_size, total);
        
        // Verify we received all data
        if (current_strip_size != total) {
            Logger.logLinef("ERROR: Size mismatch! Received %u, expected %u", 
                           current_strip_size, total);
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
            Logger.logEnd();
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Incomplete upload\"}");
            return;
        }
        
        // Validate JPEG header (SOI marker: 0xFFD8)
        if (current_strip_size < 2 || 
            current_strip_buffer[0] != 0xFF || 
            current_strip_buffer[1] != 0xD8) {
            Logger.logLinef("ERROR: Invalid JPEG header: 0x%02X%02X (expected 0xFFD8)", 
                           current_strip_buffer[0], current_strip_buffer[1]);
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
            Logger.logEnd();
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JPEG data\"}");
            return;
        }

        // Best-effort header preflight so we can return a descriptive 400 before decoding
        char preflight_err[160];
        const int remaining_height = imageHeight;  // conservative bound for a single fragment
        if (!jpeg_preflight_tjpgd_fragment_supported(current_strip_buffer, current_strip_size, imageWidth, remaining_height, preflight_err, sizeof(preflight_err))) {
            Logger.logLinef("ERROR: JPEG fragment preflight failed: %s", preflight_err);
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
            Logger.logEnd();

            char resp[256];
            snprintf(resp, sizeof(resp), "{\"success\":false,\"message\":\"%s\"}", preflight_err);
            request->send(400, "application/json", resp);
            return;
        }
        
        // Decode strip synchronously (device pointer bug is fixed)
        bool success = display_decode_strip_ex(
            current_strip_buffer,
            current_strip_size,
            stripIndex,
            false  // output_bgr565=false: accept normal RGB JPEG input
        );
        
        // Free the buffer after decoding
        free((void*)current_strip_buffer);
        current_strip_buffer = nullptr;
        current_strip_size = 0;
        
        bool is_last = (stripIndex == totalStrips - 1);
        
        if (!success) {
            Logger.logLinef("ERROR: Failed to decode strip %d", stripIndex);
            Logger.logEnd();
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Decode failed\"}");
            return;
        }
        
        Logger.logLinef("✓ Strip %d decoded", stripIndex);
        
        if (is_last) {
            Logger.logLinef("✓ All %d strips uploaded and decoded", totalStrips);
        }
        
        Logger.logLinef("Progress: %d/%d", stripIndex + 1, totalStrips);
        Logger.logEnd();
        
        char response[128];
        snprintf(response, sizeof(response), 
                 "{\"success\":true,\"strip\":%d,\"total\":%d,\"complete\":%s}", 
                 stripIndex, totalStrips, is_last ? "true" : "false");
        request->send(200, "application/json", response);
    }
}

// ===== PUBLIC API =====

// Initialize web portal
void web_portal_init(DeviceConfig *config) {
    Logger.logBegin("Portal Init");
    
    current_config = config;
    
    // Initialize current brightness from config (or default 100%)
    if (config && config->magic == CONFIG_MAGIC) {
        current_brightness = config->lcd_brightness;
    } else {
        current_brightness = 100;
    }
    Logger.logMessagef("Portal", "Initial brightness: %d%%", current_brightness);
    
    // Create web server instance (avoid global constructor issues)
    if (server == nullptr) {
        yield();
        delay(100);
        
        server = new AsyncWebServer(80);
        
        yield();
        delay(100);
    }

    // Page routes
    server->on("/", HTTP_GET, handleRoot);
    server->on("/home.html", HTTP_GET, handleHome);
    server->on("/network.html", HTTP_GET, handleNetwork);
    server->on("/firmware.html", HTTP_GET, handleFirmware);
    
    // Asset routes
    server->on("/portal.css", HTTP_GET, handleCSS);
    server->on("/portal.js", HTTP_GET, handleJS);
    
    // API endpoints
    server->on("/api/mode", HTTP_GET, handleGetMode);
    server->on("/api/config", HTTP_GET, handleGetConfig);
    
    server->on("/api/config", HTTP_POST, 
        [](AsyncWebServerRequest *request) {},
        NULL,
        handlePostConfig
    );
    
    server->on("/api/config", HTTP_DELETE, handleDeleteConfig);
    server->on("/api/info", HTTP_GET, handleGetVersion);
    server->on("/api/health", HTTP_GET, handleGetHealth);
    server->on("/api/reboot", HTTP_POST, handleReboot);
    
    // Brightness control endpoints
    server->on("/api/brightness", HTTP_GET, handleGetBrightness);
    
    server->on("/api/brightness", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        handlePostBrightness
    );
    
    // OTA upload endpoint
    server->on("/api/update", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        handleOTAUpload
    );
    
    // Image display endpoints
    // Register the more specific /chunks endpoint before /image.
    // Some handler matching modes are prefix-based; ordering avoids accidental capture.

    // Chunked image display endpoint (stateless/atomic)
    // URL format: /api/display/image/chunks?index=N&total=T&width=W&height=H[&timeout=MS]
    // Each request is self-contained with all metadata
    // Uses body handler since we're sending raw binary data
    server->on(
        "/api/display/image/chunks",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,  // No file upload handler
        handleStripUpload  // Body handler for raw binary data
    );

    server->on("/api/display/image", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        handleImageUpload
    );
    
    server->on("/api/display/image", HTTP_DELETE, handleImageDelete);
    
    // 404 handler
    server->onNotFound([](AsyncWebServerRequest *request) {
        // In AP mode, redirect to root for captive portal
        if (ap_mode_active) {
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
    ap_mode_active = true;

    Logger.logLinef("IP: %s", WiFi.softAPIP().toString().c_str());
    Logger.logEnd("Captive portal active");
}

// Stop AP mode
void web_portal_stop_ap() {
    if (ap_mode_active) {
        Logger.logMessage("Portal", "Stopping AP mode");
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        ap_mode_active = false;
    }
}

// Handle web server (call in loop)
void web_portal_handle() {
    if (ap_mode_active) {
        dnsServer.processNextRequest();
    }
}

// Check if in AP mode
bool web_portal_is_ap_mode() {
    return ap_mode_active;
}

// Check if OTA update is in progress
bool web_portal_ota_in_progress() {
    return ota_in_progress;
}

// Process pending image operations (call from main loop)
void web_portal_process_pending() {
    static unsigned long last_processed_id = 0;
    
    // Process regular image upload (existing code)
    // Only process if upload is ready and no OTA in progress
    if (upload_state != UPLOAD_READY_TO_DISPLAY || ota_in_progress) {
        return;
    }
    
    // Skip if we already processed this operation
    if (pending_op_id == last_processed_id) {
        return;
    }
    
    last_processed_id = pending_op_id;  // Mark as processed
    
    if (pending_image_op.dismiss) {
        // Dismiss current image
        display_hide_strip_image();
        display_hide_image();
        pending_image_op.dismiss = false;
        upload_state = UPLOAD_IDLE;
    } else if (pending_image_op.buffer && pending_image_op.size > 0) {
        // Display new image with custom timeout and accurate start time
        // LVGL image widgets are not used for the image upload endpoints.
        // We render via a direct-to-LCD JPEG decode path (TJpgDec / tjpgd) to keep memory bounded.
        const uint8_t* buf = pending_image_op.buffer;
        const size_t sz = pending_image_op.size;

        bool success = false;
        // /api/display/image is JPEG-only and rendered via TJpgDec direct-to-LCD.
        // Treat full JPEG as a single "strip" and decode directly to LCD.
        if (!display_start_strip_upload(LCD_WIDTH, LCD_HEIGHT, pending_image_op.timeout_ms, pending_image_op.start_time)) {
            Logger.logMessage("Portal", "ERROR: Failed to init direct image screen for JPEG");
            success = false;
        } else {
            // output_bgr565=false: accept normal RGB JPEG input on an LCD configured for RGB.
            success = display_decode_strip_ex(buf, sz, 0, false);
        }

        // Free the upload buffer after synchronous decode
        free((void*)pending_image_op.buffer);
        pending_image_op.buffer = nullptr;
        pending_image_op.size = 0;
        upload_state = UPLOAD_IDLE;

        if (!success) {
            Logger.logMessage("Portal", "ERROR: Failed to display image");
        }
    } else {
        // Invalid state - reset only if not actively uploading
        if (upload_state == UPLOAD_READY_TO_DISPLAY) {
            Logger.logMessage("Portal", "WARNING: Invalid pending state (no dismiss and no buffer), resetting");
            upload_state = UPLOAD_IDLE;
        }
        // If upload_state is UPLOAD_IN_PROGRESS, leave it alone - upload is ongoing
    }
}
