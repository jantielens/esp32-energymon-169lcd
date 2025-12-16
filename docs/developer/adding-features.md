# Adding Features

Complete guide to extending the ESP32 Energy Monitor with new functionality. Learn the project architecture, code patterns, and step-by-step instructions for common feature additions.

## Table of Contents

- [Project Architecture](#project-architecture)
- [Code Organization](#code-organization)
- [Adding Configuration Fields](#adding-configuration-fields)
- [Adding MQTT Topics](#adding-mqtt-topics)
- [Adding LVGL Screens](#adding-lvgl-screens)
- [Adding Web Portal Pages](#adding-web-portal-pages)
- [Adding REST API Endpoints](#adding-rest-api-endpoints)
- [LVGL Best Practices](#lvgl-best-practices)
- [Contributing Workflow](#contributing-workflow)

## Project Architecture

### Component Overview

```
┌────────────────────────────────────────────────┐
│              app.ino (Main Loop)               │
│  - System init, WiFi, MQTT, display updates    │
└─────┬──────────────────────────────────────────┘
      │
      ├─► ConfigManager ────► NVS Storage
      │    (persistent settings)
      │
      ├─► WebPortal ────────► AsyncWebServer
      │    (HTTP/REST API)       (port 80)
      │
      ├─► MQTTManager ──────► PubSubClient
      │    (messaging)          (broker connection)
      │
      └─► DisplayManager ───► LVGL + LCD Driver
           (screen lifecycle)   (ST7789V2 SPI)
                │
                ├─► SplashScreen (boot progress)
                └─► PowerScreen (live data)
```

### File Responsibilities

| File | Purpose | When to Modify |
|------|---------|----------------|
| **app.ino** | Main program flow, WiFi, system initialization | Adding new managers, changing boot sequence |
| **config_manager.cpp/h** | NVS storage for settings | Adding new configuration fields |
| **web_portal.cpp/h** | Web portal composition root (server + captive portal) | Wiring modules, portal lifecycle |
| **web_portal_pages.cpp/h** | Web UI routes (HTML/CSS/JS) | Adding/changing pages or assets |
| **web_portal_api_*.cpp/h** | REST API modules (config/system/brightness/OTA) | Adding/changing API endpoints |
| **image_api.cpp/h** | Image Display API routes (copyable module) | Image upload/display behavior |
| **mqtt_manager.cpp/h** | Subscribe to MQTT topics, handle messages | Adding new MQTT subscriptions |
| **display_manager.cpp/h** | Screen lifecycle, transitions | Adding new LVGL screens |
| **lcd_driver.cpp/h** | Low-level SPI display driver | Display hardware changes |
| **log_manager.cpp/h** | Serial logging with web streaming | New logging categories |
| **screen_*.cpp/h** | Individual LVGL screen implementations | Creating new screens |
| **board_config.h** | GPIO pin defaults | Adding new hardware peripherals |

## Code Organization

### Source Structure

```
src/
├── app/
│   ├── app.ino                    # Main program entry point
│   ├── config_manager.{cpp,h}     # Configuration persistence (NVS)
│   ├── web_portal.{cpp,h}         # Web server & REST API
│   ├── mqtt_manager.{cpp,h}       # MQTT client & message handling
│   ├── display_manager.{cpp,h}    # LVGL screen lifecycle
│   ├── lcd_driver.{cpp,h}         # ST7789V2 SPI driver
│   ├── log_manager.{cpp,h}        # Logging with web streaming
│   ├── board_config.h             # Hardware pin configuration
│   ├── lv_conf.h                  # LVGL configuration
│   │
│   ├── screen_base.h              # Abstract screen interface
│   ├── screen_splash.{cpp,h}      # Boot splash screen
│   ├── screen_power.{cpp,h}       # Power monitoring screen
│   │
│   ├── icons.{c,h}                # LVGL icon assets (auto-generated)
│   ├── web_assets.h               # Embedded HTML/CSS/JS (auto-generated)
│   │
│   └── web/                       # Web portal source files
│       ├── _header.html           # HTML head template
│       ├── _nav.html              # Navigation template
│       ├── _footer.html           # Footer template
│       ├── home.html              # Home page
│       ├── network.html           # Network settings page
│       ├── firmware.html          # OTA update page
│       ├── portal.css             # Styles
│       └── portal.js              # Client-side JavaScript
│
├── boards/                        # Board-specific overrides
│   └── esp32c3/
│       └── board_overrides.h      # ESP32-C3 pin mappings
│
└── version.h                      # Firmware version macros
```

### Build-Time Asset Generation

**Icons** (`assets/icons/*.png` → `src/app/icons.c`):
- Converted by `tools/png2icons.py`
- LVGL TRUE_COLOR_ALPHA format (RGB565 + Alpha)
- 48×48 PNG recommended

**Web Assets** (`src/app/web/*` → `src/app/web_assets.h`):
- Minified by `tools/minify-web-assets.sh`
- Gzip compressed for smaller storage
- Template substitution for branding

See [icon-system.md](icon-system.md) and [building-from-source.md](building-from-source.md) for details.

## Adding Configuration Fields

### Step-by-Step Example: Add Temperature Offset

#### 1. Update DeviceConfig Structure

**File:** `src/app/config_manager.h`

```cpp
// Configuration structure
struct DeviceConfig {
    // ... existing fields ...
    
    // Temperature settings
    float temperature_offset;  // Calibration offset in °C
    
    // Validation flag (magic number to detect valid config)
    uint32_t magic;
};
```

**Important:** Add new fields **before** the `magic` field.

#### 2. Initialize Default Value

**File:** `src/app/config_manager.cpp`

```cpp
bool config_manager_load(DeviceConfig *config) {
    // ... existing load code ...
    
    // If config doesn't have new field, initialize it
    if (config->magic == CONFIG_MAGIC) {
        // Config loaded but may be from older version
        if (config->temperature_offset == 0.0f) {
            config->temperature_offset = 0.0f;  // Default: no offset
        }
    }
    
    return true;
}
```

#### 3. Add to REST API (GET)

**File:** `src/app/web_portal_api_config.cpp` in `handleGetConfig()`

```cpp
void handleGetConfig(AsyncWebServerRequest *request) {
    // ... existing config ...
    
    doc["temperature_offset"] = current_config->temperature_offset;
    
    // ... send response ...
}
```

#### 4. Add to REST API (POST)

**File:** `src/app/web_portal_api_config.cpp` in `handlePostConfig()`

```cpp
void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, ...) {
    // ... parse JSON ...
    
    if (doc.containsKey("temperature_offset")) {
        temp_config.temperature_offset = doc["temperature_offset"].as<float>();
    }
    
    // ... save config ...
}
```

#### 5. Add to Web UI

**File:** `src/app/web/home.html`

```html
<div class="card">
    <h2>Temperature Settings</h2>
    <div class="form-group">
        <label for="temperature_offset">Temperature Offset (°C)</label>
        <input type="number" id="temperature_offset" 
               step="0.1" min="-10" max="10" value="0.0">
        <small>Calibration offset for temperature readings</small>
    </div>
</div>
```

**File:** `src/app/web/portal.js`

```javascript
// In loadConfig()
document.getElementById('temperature_offset').value = 
    config.temperature_offset || 0.0;

// In saveConfig()
temperature_offset: parseFloat(document.getElementById('temperature_offset').value)
```

#### 6. Use in Application

**File:** `src/app/app.ino` (or wherever you read temperature)

```cpp
#include "config_manager.h"

float read_temperature() {
    float raw_temp = read_sensor();  // Your sensor code
    float calibrated = raw_temp + device_config.temperature_offset;
    return calibrated;
}
```

#### 7. Test and Document

```bash
# Build and upload
./build.sh esp32c3
./upload.sh esp32c3

# Test via web portal
# Open http://<device-ip>/
# Set temperature offset to 1.5
# Save configuration
# Verify persistence across reboot
```

**Update CHANGELOG.md:**
```markdown
### Added
- Temperature calibration offset in configuration
```

## Adding MQTT Topics

### Step-by-Step Example: Add Battery Voltage Topic

#### 1. Add Configuration Field

**File:** `src/app/config_manager.h`

```cpp
struct DeviceConfig {
    // ... existing MQTT topics ...
    char mqtt_topic_solar[CONFIG_MQTT_TOPIC_MAX_LEN];
    char mqtt_topic_grid[CONFIG_MQTT_TOPIC_MAX_LEN];
    char mqtt_topic_battery[CONFIG_MQTT_TOPIC_MAX_LEN];  // New topic
    // ...
};
```

#### 2. Add to REST API

**File:** `src/app/web_portal_api_config.cpp`

```cpp
// In handleGetConfig()
doc["mqtt_topic_battery"] = current_config->mqtt_topic_battery;

// In handlePostConfig()
if (doc.containsKey("mqtt_topic_battery")) {
    strlcpy(temp_config.mqtt_topic_battery, 
            doc["mqtt_topic_battery"], 
            CONFIG_MQTT_TOPIC_MAX_LEN);
}
```

#### 3. Add to Web UI

**File:** `src/app/web/network.html`

```html
<div class="form-group">
    <label for="mqtt_topic_battery">Battery Voltage Topic</label>
    <input type="text" id="mqtt_topic_battery" 
           placeholder="home/battery/voltage">
</div>
```

**File:** `src/app/web/portal.js`

```javascript
// Load
document.getElementById('mqtt_topic_battery').value = 
    config.mqtt_topic_battery || '';

// Save
mqtt_topic_battery: document.getElementById('mqtt_topic_battery').value
```

#### 4. Subscribe to Topic

**File:** `src/app/mqtt_manager.cpp`

```cpp
// Global variable for battery voltage
static float battery_voltage = NAN;

// In mqtt_manager_init()
void mqtt_manager_init(DeviceConfig *config) {
    // ... existing subscriptions ...
    
    if (strlen(config->mqtt_topic_battery) > 0) {
        Logger.logMessagef("MQTT", "Subscribing to: %s", 
                          config->mqtt_topic_battery);
        mqtt_client.subscribe(config->mqtt_topic_battery);
    }
}

// In mqtt_callback()
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    // ... existing topic handlers ...
    
    if (strcmp(topic, mqtt_config->mqtt_topic_battery) == 0) {
        String payload_str = String((char*)payload).substring(0, length);
        battery_voltage = payload_str.toFloat();
        Logger.logMessagef("MQTT", "Battery: %.2fV", battery_voltage);
    }
}

// Getter function
float mqtt_manager_get_battery_voltage() {
    return battery_voltage;
}
```

**File:** `src/app/mqtt_manager.h`

```cpp
float mqtt_manager_get_battery_voltage();  // Add declaration
```

#### 5. Display Value

**File:** `src/app/screen_power.cpp`

```cpp
void PowerScreen::update() {
    float battery_v = mqtt_manager_get_battery_voltage();
    
    if (!isnan(battery_v)) {
        // Update battery display element
        lv_label_set_text_fmt(battery_label, "%.1fV", battery_v);
    }
}
```

## Adding LVGL Screens

### Screen Lifecycle

All screens inherit from `ScreenBase`:

```cpp
class ScreenBase {
public:
    virtual void create() = 0;   // Build UI elements
    virtual void destroy() = 0;  // Clean up UI elements
    virtual void update() = 0;   // Refresh data (called every frame)
    virtual void show() = 0;     // Make screen visible
    virtual void hide() = 0;     // Hide screen
};
```

### Step-by-Step Example: Add Settings Screen

#### 1. Create Header File

**File:** `src/app/screen_settings.h`

```cpp
#ifndef SCREEN_SETTINGS_H
#define SCREEN_SETTINGS_H

#include "screen_base.h"

class SettingsScreen : public ScreenBase {
public:
    SettingsScreen();
    ~SettingsScreen();
    
    void create() override;
    void destroy() override;
    void update() override;
    void show() override;
    void hide() override;

private:
    // UI elements
    lv_obj_t* background = nullptr;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* brightness_slider = nullptr;
    lv_obj_t* brightness_value = nullptr;
    
    // Event handlers (must be static for LVGL callbacks)
    static void brightness_changed_cb(lv_event_t* e);
};

#endif
```

#### 2. Create Implementation File

**File:** `src/app/screen_settings.cpp`

```cpp
#include "screen_settings.h"
#include "lcd_driver.h"
#include <lvgl.h>

SettingsScreen::SettingsScreen() {}

SettingsScreen::~SettingsScreen() {
    if (screen_obj) {
        destroy();
    }
}

void SettingsScreen::create() {
    // Create root screen object
    screen_obj = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x000000), 0);
    
    // Title label
    title_label = lv_label_create(screen_obj);
    lv_label_set_text(title_label, "Settings");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Brightness slider
    brightness_slider = lv_slider_create(screen_obj);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, 100, LV_ANIM_OFF);
    lv_obj_set_width(brightness_slider, 200);
    lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(brightness_slider, brightness_changed_cb, 
                        LV_EVENT_VALUE_CHANGED, this);
    
    // Brightness value label
    brightness_value = lv_label_create(screen_obj);
    lv_label_set_text(brightness_value, "100%");
    lv_obj_align_to(brightness_value, brightness_slider, 
                     LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

void SettingsScreen::destroy() {
    if (screen_obj) {
        lv_obj_del(screen_obj);
        screen_obj = nullptr;
        background = nullptr;
        title_label = nullptr;
        brightness_slider = nullptr;
        brightness_value = nullptr;
    }
}

void SettingsScreen::update() {
    // Called every frame - update dynamic content here
}

void SettingsScreen::show() {
    if (screen_obj) {
        lv_scr_load(screen_obj);
        visible = true;
    }
}

void SettingsScreen::hide() {
    visible = false;
}

// Static callback - extract 'this' from user_data
void SettingsScreen::brightness_changed_cb(lv_event_t* e) {
    SettingsScreen* screen = (SettingsScreen*)lv_event_get_user_data(e);
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    // Update label
    lv_label_set_text_fmt(screen->brightness_value, "%d%%", (int)value);
    
    // Apply brightness
    lcd_set_backlight((uint8_t)value);
}
```

#### 3. Register Screen in DisplayManager

**File:** `src/app/display_manager.h`

```cpp
#include "screen_settings.h"

extern SettingsScreen* settings_screen;

void display_show_settings_screen();  // New function
```

**File:** `src/app/display_manager.cpp`

```cpp
#include "screen_settings.h"

SettingsScreen* settings_screen = nullptr;

void display_init() {
    // ... existing init ...
    
    settings_screen = new SettingsScreen();
    settings_screen->create();
}

void display_show_settings_screen() {
    if (current_screen) {
        current_screen->hide();
    }
    current_screen = settings_screen;
    settings_screen->show();
}
```

#### 4. Add Navigation

**Option A: Button on existing screen**

```cpp
// In screen_power.cpp
lv_obj_t* settings_btn = lv_btn_create(screen_obj);
lv_obj_add_event_cb(settings_btn, settings_btn_cb, LV_EVENT_CLICKED, nullptr);

static void settings_btn_cb(lv_event_t* e) {
    display_show_settings_screen();
}
```

**Option B: Touch gesture**

```cpp
// In app.ino loop()
if (touchscreen_swipe_left_detected()) {
    display_show_settings_screen();
}
```

## Adding Web Portal Pages

### Step-by-Step Example: Add Statistics Page

#### 1. Create HTML File

**File:** `src/app/web/statistics.html`

```html
{{_header.html}}
<body>
{{_nav.html}}

<main class="main-content">
    <div class="page-header">
        <h1>Statistics</h1>
        <p class="subtitle">Energy usage history and analytics</p>
    </div>

    <div class="card">
        <h2>Daily Summary</h2>
        <div class="form-group">
            <label>Total Energy Today</label>
            <div id="energy_today">Loading...</div>
        </div>
    </div>

    <div class="card">
        <h2>Peak Power</h2>
        <div class="form-group">
            <label>Maximum Grid Import</label>
            <div id="peak_grid">Loading...</div>
        </div>
    </div>
</main>

{{_footer.html}}
</body>
</html>
```

#### 2. Add Navigation Link

**File:** `src/app/web/_nav.html`

```html
<nav class="nav-menu">
    <a href="/home.html">Home</a>
    <a href="/network.html">Network</a>
    <a href="/statistics.html">Statistics</a>  <!-- New -->
    <a href="/firmware.html">Firmware</a>
</nav>
```

#### 3. Add JavaScript Logic

**File:** `src/app/web/portal.js`

```javascript
// Add to window.onload or DOMContentLoaded
if (window.location.pathname === '/statistics.html') {
    loadStatistics();
}

async function loadStatistics() {
    try {
        const response = await fetch('/api/statistics');
        const data = await response.json();
        
        document.getElementById('energy_today').textContent = 
            data.energy_today + ' kWh';
        document.getElementById('peak_grid').textContent = 
            data.peak_grid + ' kW';
    } catch (error) {
        console.error('Failed to load statistics:', error);
    }
}
```

#### 4. Add REST API Endpoint

**Files:**
- `src/app/web_portal_pages.cpp` (page routes)
- `src/app/web_portal_api_*.cpp` (API routes; add to an existing module or create a new one)

```cpp
void handleGetStatistics(AsyncWebServerRequest *request);

void web_portal_init(DeviceConfig *config) {
    // ... existing routes ...
    
    server->on("/statistics.html", HTTP_GET, handleStatistics);
    server->on("/api/statistics", HTTP_GET, handleGetStatistics);
}

void handleStatistics(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/html", 
        statistics_html_gz, 
        statistics_html_gz_len
    );
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

void handleGetStatistics(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["energy_today"] = calculate_energy_today();
    doc["peak_grid"] = get_peak_grid_power();
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}
```

#### 5. Rebuild Assets

```bash
# Rebuild to regenerate web_assets.h
./build.sh esp32c3
./upload.sh esp32c3
```

## Adding REST API Endpoints

### Step-by-Step Example: Add `/api/uptime` Endpoint

#### 1. Add Handler Function

**File:** `src/app/web_portal_api_system.cpp`

```cpp
// Add handler + route inside web_portal_system_register_routes(server)

void handleGetUptime(AsyncWebServerRequest *request) {
    unsigned long uptime_ms = millis();
    unsigned long uptime_sec = uptime_ms / 1000;
    unsigned long days = uptime_sec / 86400;
    unsigned long hours = (uptime_sec % 86400) / 3600;
    unsigned long minutes = (uptime_sec % 3600) / 60;
    unsigned long seconds = uptime_sec % 60;
    
    JsonDocument doc;
    doc["uptime_ms"] = uptime_ms;
    doc["uptime_seconds"] = uptime_sec;
    doc["days"] = days;
    doc["hours"] = hours;
    doc["minutes"] = minutes;
    doc["seconds"] = seconds;
    doc["formatted"] = String(days) + "d " + String(hours) + "h " + 
                       String(minutes) + "m " + String(seconds) + "s";
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}
```

#### 2. Test Endpoint

```bash
curl http://<device-ip>/api/uptime
```

**Expected Response:**
```json
{
  "uptime_ms": 1234567890,
  "uptime_seconds": 1234567,
  "days": 14,
  "hours": 6,
  "minutes": 56,
  "seconds": 7,
  "formatted": "14d 6h 56m 7s"
}
```

#### 3. Document in API Guide

**File:** `docs/developer/web-portal-api.md`

Add new endpoint to documentation.

## LVGL Best Practices

### Memory Management

```cpp
// ❌ BAD: Stack-allocated objects get destroyed
void create() {
    lv_point_t points[2] = {{0, 0}, {100, 100}};
    lv_obj_t* line = lv_line_create(screen_obj);
    lv_line_set_points(line, points, 2);  // CRASH! points go out of scope
}

// ✅ GOOD: Use class members or heap allocation
class MyScreen : public ScreenBase {
private:
    lv_point_t line_points[2];  // Persists with object
};

void MyScreen::create() {
    line_points[0] = {0, 0};
    line_points[1] = {100, 100};
    lv_obj_t* line = lv_line_create(screen_obj);
    lv_line_set_points(line, line_points, 2);  // Safe!
}
```

### Event Callbacks

```cpp
// LVGL callbacks must be static functions
class MyScreen : public ScreenBase {
private:
    static void button_clicked_cb(lv_event_t* e);  // Static
};

void MyScreen::create() {
    lv_obj_t* btn = lv_btn_create(screen_obj);
    
    // Pass 'this' as user_data to access instance
    lv_obj_add_event_cb(btn, button_clicked_cb, LV_EVENT_CLICKED, this);
}

void MyScreen::button_clicked_cb(lv_event_t* e) {
    // Extract instance from user_data
    MyScreen* screen = (MyScreen*)lv_event_get_user_data(e);
    
    // Now you can access instance members
    screen->handle_button_click();
}
```

### Color Handling (BGR Displays)

The ST7789V2 panel uses BGR color order. Always use the RGB→BGR conversion:

```cpp
// ❌ BAD: Direct RGB colors appear wrong
lv_obj_set_style_bg_color(obj, lv_color_hex(0xFF0000), 0);  // Red shows as blue!

// ✅ GOOD: Use conversion helper
lv_color_t rgb_to_bgr(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    return lv_color_make(b, g, r);  // Swap R and B
}

lv_obj_set_style_bg_color(obj, rgb_to_bgr(0xFF0000), 0);  // Correct red
```

See [lvgl-display-system.md](lvgl-display-system.md) for details.

### Styles and Animations

```cpp
// Reusable style object
static lv_style_t button_style;

void create() {
    // Initialize style once
    lv_style_init(&button_style);
    lv_style_set_bg_color(&button_style, lv_color_hex(0x00FF00));
    lv_style_set_radius(&button_style, 10);
    
    // Apply to multiple objects
    lv_obj_add_style(btn1, &button_style, 0);
    lv_obj_add_style(btn2, &button_style, 0);
}

// Smooth animations
lv_anim_t anim;
lv_anim_init(&anim);
lv_anim_set_var(&anim, obj);
lv_anim_set_values(&anim, 0, 100);
lv_anim_set_time(&anim, 500);  // 500ms
lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
lv_anim_start(&anim);
```

## Contributing Workflow

### Development Cycle

```bash
# 1. Create feature branch
git checkout -b feature/battery-monitor

# 2. Make changes
# - Edit source files
# - Add tests if applicable
# - Update documentation

# 3. Build and test locally
./build.sh esp32c3
./upload.sh esp32c3
./monitor.sh

# 4. Update CHANGELOG.md
# Add entry under [Unreleased] section

# 5. Commit with conventional commit message
git add .
git commit -m "feat: add battery voltage monitoring"

# 6. Push and create PR
git push origin feature/battery-monitor
# Create PR on GitHub
```

### Commit Message Convention

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, no logic change)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Adding tests
- `chore`: Maintenance tasks

**Examples:**
```
feat(mqtt): add battery voltage topic subscription
fix(display): correct BGR color conversion for anti-aliasing
docs(api): document new /api/uptime endpoint
refactor(config): simplify threshold validation logic
```

### Code Style Guidelines

**C++ Conventions:**
- Snake_case for functions: `mqtt_manager_init()`
- CamelCase for classes: `PowerScreen`
- ALL_CAPS for macros: `CONFIG_MAGIC`
- Prefix private members with `_` (optional)
- 4-space indentation
- Braces on same line for functions

**LVGL Naming:**
- Descriptive widget names: `solar_value_label` not `label1`
- Group related widgets: `grid_*` for grid power elements
- Use `nullptr` not `NULL`

**Comments:**
```cpp
// Brief single-line comment

/*
 * Multi-line block comment
 * for complex explanations.
 */

/**
 * Doxygen-style comment for functions
 * 
 * @param kw Power in kilowatts
 * @return Color based on threshold
 */
lv_color_t get_power_color(float kw);
```

### Testing Checklist

Before submitting PR:

- [ ] Code compiles without warnings: `./build.sh`
- [ ] Firmware uploads successfully: `./upload.sh`
- [ ] Device boots without errors: `./monitor.sh`
- [ ] New feature works as expected
- [ ] Existing features still work (regression test)
- [ ] Configuration persists across reboots
- [ ] Web portal loads correctly
- [ ] MQTT messages handled properly
- [ ] Display updates correctly
- [ ] CHANGELOG.md updated
- [ ] Documentation updated (if needed)

## Related Documentation

- [Building from Source](building-from-source.md) - Build system and scripts
- [Web Portal API](web-portal-api.md) - REST API reference
- [Multi-Board Support](multi-board-support.md) - Board variants
- [Icon System](icon-system.md) - LVGL icon generation
- [LVGL Display System](lvgl-display-system.md) - Display architecture
- [Release Process](release-process.md) - Creating releases

---

**Ready to contribute?** Fork the repository, make your changes, and submit a pull request!
