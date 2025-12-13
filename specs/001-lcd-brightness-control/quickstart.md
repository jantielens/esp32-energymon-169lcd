# Quickstart: LCD Brightness Control Implementation

**Phase 1 Output** | **Date**: December 13, 2025 | **Branch**: `001-lcd-brightness-control`

## Prerequisites

Before starting implementation, ensure you have:

- [x] Read [spec.md](../spec.md) - Feature requirements and user stories
- [x] Read [research.md](../research.md) - Technical decisions and patterns
- [x] Read [data-model.md](../data-model.md) - Data structures and flows
- [x] Read [contracts/api-brightness.md](../contracts/api-brightness.md) - API specification
- [x] Development environment set up (see main README.md)
- [x] ESP32-C3 device with 1.69" LCD for testing

## Implementation Order

Follow this sequence for minimal integration risk:

### Step 1: Backend - Configuration Storage (30 min)

**Files to modify**:
- `src/app/config_manager.h`
- `src/app/config_manager.cpp`

**Changes**:

1. **Add field to DeviceConfig struct** (config_manager.h):
```cpp
struct DeviceConfig {
    // ... existing fields ...
    
    // LCD brightness (0-100%, default 100)
    uint8_t lcd_brightness;
    
    // ... magic field ...
};
```

2. **Add NVS key constant** (config_manager.cpp, near top with other KEY_ defines):
```cpp
#define KEY_LCD_BRIGHTNESS "lcd_bright"
```

3. **Load brightness from NVS** (config_manager.cpp, in `config_manager_load()` function):
```cpp
// After loading MQTT settings, before final validation:
config->lcd_brightness = preferences.getUChar(KEY_LCD_BRIGHTNESS, 100);
```

4. **Save brightness to NVS** (config_manager.cpp, in `config_manager_save()` function):
```cpp
// After saving MQTT settings:
preferences.putUChar(KEY_LCD_BRIGHTNESS, config->lcd_brightness);
```

5. **Print brightness in debug output** (config_manager.cpp, in `config_manager_print()` function):
```cpp
// After printing MQTT settings:
Serial.printf("  LCD Brightness: %d%%\n", config->lcd_brightness);
```

**Verification**:
```bash
./build.sh esp32c3  # Must compile without errors
```

---

### Step 2: Backend - REST API (45 min)

**Files to modify**:
- `src/app/web_portal.h`
- `src/app/web_portal.cpp`

**Changes**:

1. **Add forward declarations** (web_portal.h, near other handle* declarations):
```cpp
void handleGetBrightness(AsyncWebServerRequest *request);
void handlePostBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
```

2. **Add global variable** (web_portal.cpp, near other static variables):
```cpp
// Current runtime brightness (may differ from saved value)
static uint8_t current_brightness = 100;
```

3. **Implement GET handler** (web_portal.cpp, before `web_portal_init()` function):
```cpp
// GET /api/brightness - Return current brightness
void handleGetBrightness(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["brightness"] = current_brightness;
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}
```

4. **Implement POST handler** (web_portal.cpp, after GET handler):
```cpp
// POST /api/brightness - Update brightness in real-time (not persisted)
void handlePostBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Parse JSON from request body
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    if (!doc.containsKey("brightness")) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing brightness field\"}");
        return;
    }
    
    // Extract and validate brightness value
    int brightness = doc["brightness"] | 100;
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    
    // Update hardware immediately
    current_brightness = (uint8_t)brightness;
    lcd_set_backlight(current_brightness);
    
    // Return success response
    JsonDocument response_doc;
    response_doc["success"] = true;
    response_doc["brightness"] = current_brightness;
    
    String response;
    serializeJson(response_doc, response);
    
    request->send(200, "application/json", response);
}
```

5. **Register endpoints** (web_portal.cpp, in `web_portal_init()`, after `/api/health` registration):
```cpp
server->on("/api/brightness", HTTP_GET, handleGetBrightness);

server->on("/api/brightness", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    NULL,
    handlePostBrightness
);
```

6. **Update handlePostConfig** (web_portal.cpp, in `handlePostConfig()`, after MQTT field handling):
```cpp
// LCD brightness
if (doc.containsKey("lcd_brightness")) {
    int brightness = doc["lcd_brightness"] | 100;
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    current_config->lcd_brightness = (uint8_t)brightness;
    current_brightness = current_config->lcd_brightness;
    lcd_set_backlight(current_brightness);
}
```

7. **Update handleGetConfig** (web_portal.cpp, in `handleGetConfig()`, after MQTT fields):
```cpp
// LCD brightness
doc["lcd_brightness"] = current_config->lcd_brightness;
```

**Verification**:
```bash
./build.sh esp32c3  # Must compile without errors
```

---

### Step 3: Backend - Boot Initialization (15 min)

**Files to modify**:
- `src/app/app.ino` OR `src/app/display_manager.cpp` (choose one approach)

**Option A: Modify app.ino** (simpler):

In `setup()` function, after `config_manager_load()` and before `display_init()`:
```cpp
void setup() {
    // ... existing code ...
    
    if (config_manager_load(&device_config)) {
        Logger.logMessage("Config", "Loaded from NVS");
        config_manager_print(&device_config);
        
        // Apply saved brightness
        lcd_set_backlight(device_config.lcd_brightness);
    }
    
    // ... rest of setup ...
}
```

**Option B: Modify display_manager.cpp** (better separation of concerns):

Add parameter to `display_init()`:
```cpp
// display_manager.h
void display_init(uint8_t brightness);

// display_manager.cpp
void display_init(uint8_t brightness) {
    // ... existing init code ...
    
    // At the end, after lcd_init():
    lcd_set_backlight(brightness);
}

// app.ino - call with config value
display_init(device_config.lcd_brightness);
```

**Verification**:
```bash
./build.sh esp32c3
./upload.sh esp32c3
./monitor.sh
# Check serial output: "LCD Brightness: XX%"
```

---

### Step 4: Frontend - HTML UI (20 min)

**File to modify**:
- `src/app/web/home.html`

**Changes**:

Find the section after "Sample Settings" (around line 40-60) and add:

```html
<!-- Display Settings Section -->
<section class="card">
    <h2>Display Settings</h2>
    
    <div class="form-group">
        <label for="lcd_brightness">LCD Brightness</label>
        <div style="display: flex; align-items: center; gap: 10px;">
            <input type="range" id="lcd_brightness" name="lcd_brightness" 
                   min="0" max="100" step="1" value="100" 
                   style="flex: 1;">
            <span id="lcd_brightness_value" style="min-width: 50px; text-align: right; font-weight: bold;">100%</span>
        </div>
        <small>Adjust screen brightness (0% = off, 100% = maximum). Changes apply immediately but are not saved until you click Save.</small>
    </div>
</section>
```

**Verification**:
```bash
# Rebuild web assets
cd /home/jtielens/dev/esp32-energymon-169lcd
./tools/minify-web-assets.sh
# Check that web_assets.h was updated
git diff src/app/web_assets.h
```

---

### Step 5: Frontend - JavaScript Logic (30 min)

**File to modify**:
- `src/app/web/portal.js`

**Changes**:

1. **Add brightness to fields array** (around line 484-486):
```javascript
const fields = [
    'wifi_ssid', 'wifi_password',
    'device_name',
    'fixed_ip', 'subnet_mask', 'gateway', 'dns1', 'dns2',
    'dummy_setting',
    'mqtt_broker', 'mqtt_port', 'mqtt_username', 'mqtt_password',
    'mqtt_topic_solar', 'mqtt_topic_grid',
    'lcd_brightness'  // ADD THIS LINE
];
```

2. **Add brightness loading** (in `loadConfig()` function, after MQTT field loading):
```javascript
// LCD brightness
setValueIfExists('lcd_brightness', config.lcd_brightness);
if (config.lcd_brightness !== undefined) {
    const valueSpan = document.getElementById('lcd_brightness_value');
    if (valueSpan) {
        valueSpan.textContent = config.lcd_brightness + '%';
    }
}
```

3. **Add real-time brightness event listener** (at the end of `DOMContentLoaded`, after form setup):
```javascript
// Real-time brightness control
const brightnessSlider = document.getElementById('lcd_brightness');
const brightnessValue = document.getElementById('lcd_brightness_value');

if (brightnessSlider && brightnessValue) {
    brightnessSlider.addEventListener('input', function() {
        const value = parseInt(this.value);
        brightnessValue.textContent = value + '%';
        
        // POST to /api/brightness for immediate hardware update
        fetch('/api/brightness', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({brightness: value})
        })
        .then(response => response.json())
        .then(data => {
            if (!data.success) {
                console.error('Brightness update failed:', data.message);
            }
        })
        .catch(error => {
            console.error('Brightness update error:', error);
        });
    });
    
    // Load current brightness on page load
    fetch('/api/brightness')
        .then(response => response.json())
        .then(data => {
            brightnessSlider.value = data.brightness;
            brightnessValue.textContent = data.brightness + '%';
        })
        .catch(error => {
            console.error('Failed to load brightness:', error);
        });
}
```

**Verification**:
```bash
# Rebuild web assets
./tools/minify-web-assets.sh
# Check JavaScript is minified correctly
grep -i "lcd_brightness" src/app/web_assets.h
```

---

### Step 6: Build & Deploy (15 min)

**Commands**:

```bash
# Clean build
./clean.sh

# Build for target board
./build.sh esp32c3

# Upload to device
./upload.sh esp32c3

# Monitor serial output
./monitor.sh
```

**Or use convenience script**:
```bash
./bum.sh esp32c3  # Build + Upload + Monitor
```

**Expected serial output**:
```
[Config] Loaded from NVS
  WiFi SSID: MyNetwork
  Device Name: energy-monitor-abc123
  LCD Brightness: 75%
  ...
```

---

### Step 7: Testing (30 min)

**Manual Test Plan**:

1. **Test Default Brightness (First Boot)**:
   - Flash firmware to device without saved config
   - Expected: LCD at 100% brightness
   - Serial output: "LCD Brightness: 100%"

2. **Test Real-Time Adjustment**:
   - Open http://[device-ip]/home.html
   - Move brightness slider to 50%
   - Expected: LCD dims immediately, value label shows "50%"

3. **Test Persistence**:
   - Set brightness to 30% via slider
   - Click "Save" button
   - Wait for device to reboot
   - Expected: After reboot, LCD is at 30% brightness
   - Verify: Reload web page, slider shows 30%

4. **Test Edge Cases**:
   - Set brightness to 0% → LCD backlight completely off
   - Set brightness to 100% → LCD at maximum brightness
   - Rapidly move slider back and forth → No lag or glitching

5. **Test API Directly** (using curl):
```bash
# Get current brightness
curl http://[device-ip]/api/brightness

# Set brightness to 60%
curl -X POST http://[device-ip]/api/brightness \
  -H "Content-Type: application/json" \
  -d '{"brightness": 60}'

# Verify change
curl http://[device-ip]/api/brightness
```

6. **Test Backward Compatibility**:
   - Save config with old firmware (without brightness field)
   - Upgrade to new firmware
   - Expected: Device boots with 100% brightness (default)
   - Save new config with brightness 50%
   - Reboot
   - Expected: Device boots with 50% brightness

---

## Troubleshooting

### Build Errors

**Error**: `'lcd_brightness' was not declared in this scope`
- **Cause**: Forgot to add field to DeviceConfig struct
- **Fix**: Add `uint8_t lcd_brightness;` to config_manager.h

**Error**: `undefined reference to 'handleGetBrightness'`
- **Cause**: Missing forward declaration
- **Fix**: Add declaration to web_portal.h

### Runtime Errors

**Issue**: Brightness slider doesn't change hardware
- **Check**: Serial monitor for errors
- **Check**: Browser console for failed POST requests
- **Fix**: Verify `/api/brightness` endpoint is registered in web_portal_init()

**Issue**: Brightness resets to 100% after reboot
- **Check**: NVS save is being called (serial output during save)
- **Check**: `lcd_brightness` is included in `portal.js` fields array
- **Fix**: Verify field is in buildConfigFromForm() and POST /api/config

**Issue**: Web assets not updating
- **Cause**: Forgot to run minify script
- **Fix**: Run `./tools/minify-web-assets.sh` before building

---

## Completion Checklist

- [ ] Step 1: Config manager modified, builds successfully
- [ ] Step 2: API endpoints implemented, builds successfully
- [ ] Step 3: Boot initialization added
- [ ] Step 4: HTML UI added, minify script run
- [ ] Step 5: JavaScript logic added, minify script run
- [ ] Step 6: Firmware deployed to device
- [ ] Step 7: All manual tests pass
- [ ] Version bumped in src/version.h (MINOR increment)
- [ ] CHANGELOG.md updated with feature entry
- [ ] Documentation updated (if needed)
- [ ] Commit changes to branch: `git commit -am "Add LCD brightness control"`
- [ ] Push branch: `git push origin 001-lcd-brightness-control`
- [ ] Create pull request

---

## Next Steps

After implementation is complete:

1. **Create Pull Request**: From branch `001-lcd-brightness-control` to `main`
2. **CI Verification**: Wait for GitHub Actions to build all board targets
3. **Code Review**: Address any feedback
4. **Merge**: Merge to main branch
5. **Release**: Follow release process to create tagged version with binaries

See `docs/build-and-release-process.md` for detailed release workflow.
