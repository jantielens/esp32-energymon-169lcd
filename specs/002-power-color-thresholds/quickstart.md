# Quickstart: Configurable Power Color Thresholds

**Feature**: 002-power-color-thresholds  
**Phase**: 2 - Implementation Guide  
**Date**: December 13, 2025  
**Revised**: December 13, 2025 (Consistent 4-level structure + configurable colors)

## Overview

This guide provides step-by-step implementation instructions for adding configurable power thresholds and colors to the ESP32 Energy Monitor. The design uses a consistent 3-threshold, 4-color structure across all power types (grid, home, solar).

**Implementation Time**: ~3-4 hours  
**Files Modified**: 7  
**Lines Added**: ~250  
**Complexity**: Medium

---

## Prerequisites

✅ Existing codebase understanding:
- DeviceConfig struct in `config_manager.h`
- PowerScreen class in `screen_power.cpp/h`
- Web portal in `web_portal.cpp` and `src/app/web/home.html`
- NVS configuration persistence pattern

✅ Development environment:
- Arduino IDE or PlatformIO configured for ESP32
- Web asset minification tools (optional: `tools/minify-web-assets.sh`)

---

## Implementation Steps

### Step 1: Extend DeviceConfig Struct (NVS)

**File**: `src/app/config_manager.h`

**Location**: Add new fields to `DeviceConfig` struct before the `magic` field (magic MUST stay last)

**Code to add**:
```cpp
struct DeviceConfig {
    // ... existing fields (hostname, mqtt_broker, etc.) ...
    
    // Power Thresholds (3 per type = 9 total)
    float grid_threshold[3];     // Default: {0.0, 0.5, 2.5}
    float home_threshold[3];     // Default: {0.5, 1.0, 2.0}
    float solar_threshold[3];    // Default: {0.5, 1.5, 3.0}
    
    // Color Configuration (4 global colors, RGB hex)
    uint32_t color_good;         // Default: 0x00FF00 (green)
    uint32_t color_ok;           // Default: 0xFFFFFF (white)
    uint32_t color_attention;    // Default: 0xFFA500 (orange)
    uint32_t color_warning;      // Default: 0xFF0000 (red)
    
    uint32_t magic;  // MUST stay last for version detection
};
```

**Storage Impact**: 52 bytes (9 floats × 4 bytes + 4 uint32_t × 4 bytes)

---

### Step 2: Initialize Factory Defaults

**File**: `src/app/config_manager.cpp`

**Location**: In `config_manager_init()` or wherever factory defaults are set

**Code to add**:
```cpp
void config_manager_set_defaults(DeviceConfig* config) {
    // ... existing defaults ...
    
    // Grid thresholds (can be negative for export)
    config->grid_threshold[0] = 0.0f;    // Good → OK boundary
    config->grid_threshold[1] = 0.5f;    // OK → Attention boundary
    config->grid_threshold[2] = 2.5f;    // Attention → Warning boundary
    
    // Home thresholds (non-negative)
    config->home_threshold[0] = 0.5f;
    config->home_threshold[1] = 1.0f;
    config->home_threshold[2] = 2.0f;
    
    // Solar thresholds (non-negative)
    config->solar_threshold[0] = 0.5f;
    config->solar_threshold[1] = 1.5f;
    config->solar_threshold[2] = 3.0f;
    
    // Color configuration (RGB hex values)
    config->color_good = 0x00FF00;       // Green
    config->color_ok = 0xFFFFFF;         // White
    config->color_attention = 0xFFA500;  // Orange
    config->color_warning = 0xFF0000;    // Red
    
    config->magic = 0xDEADBEEF;
}
```

**Backward Compatibility**: If loaded config size < sizeof(DeviceConfig), apply these defaults for new fields

---

### Step 3: Implement Unified Color Calculation

**File**: `src/app/screen_power.cpp`

**Location**: Replace existing `get_grid_color()`, `get_home_color()`, `get_solar_color()` methods with unified algorithm

**Remove** (existing hardcoded logic):
```cpp
// DELETE these three separate methods:
lv_color_t PowerScreen::get_grid_color(float grid_power) { ... }
lv_color_t PowerScreen::get_home_color(float home_power) { ... }
lv_color_t PowerScreen::get_solar_color(float solar_power) { ... }
```

**Add** (unified configurable logic):
```cpp
// Helper: Convert RGB to BGR for display hardware
lv_color_t rgb_to_bgr(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    return lv_color_hex((b << 16) | (g << 8) | r);
}

// Unified color determination for all power types
lv_color_t PowerScreen::get_power_color(float value, float thresholds[3]) {
    extern DeviceConfig *current_config;  // Global config pointer
    
    uint32_t rgb;
    if (value < thresholds[0]) {
        rgb = current_config->color_good;
    } else if (value < thresholds[1]) {
        rgb = current_config->color_ok;
    } else if (value < thresholds[2]) {
        rgb = current_config->color_attention;
    } else {
        rgb = current_config->color_warning;
    }
    
    return rgb_to_bgr(rgb);
}

// Convenience wrappers (optional - can call get_power_color directly)
lv_color_t PowerScreen::get_grid_color(float grid_power) {
    extern DeviceConfig *current_config;
    return get_power_color(grid_power, current_config->grid_threshold);
}

lv_color_t PowerScreen::get_home_color(float home_power) {
    extern DeviceConfig *current_config;
    return get_power_color(home_power, current_config->home_threshold);
}

lv_color_t PowerScreen::get_solar_color(float solar_power) {
    extern DeviceConfig *current_config;
    return get_power_color(solar_power, current_config->solar_threshold);
}
```

**File**: `src/app/screen_power.h`

**Update header**:
```cpp
class PowerScreen : public ScreenBase {
private:
    lv_color_t rgb_to_bgr(uint32_t rgb);
    lv_color_t get_power_color(float value, float thresholds[3]);
    lv_color_t get_grid_color(float grid_power);
    lv_color_t get_home_color(float home_power);
    lv_color_t get_solar_color(float solar_power);
    // ... rest of class ...
};
```

---

### Step 4: Add Configuration Validation

**File**: `src/app/config_manager.cpp`

**Add validation function**:
```cpp
bool config_manager_validate(const DeviceConfig* config) {
    // Grid thresholds (can be negative)
    if (config->grid_threshold[0] > config->grid_threshold[1] ||
        config->grid_threshold[1] > config->grid_threshold[2]) {
        return false;
    }
    if (config->grid_threshold[0] < -10.0f || config->grid_threshold[2] > 10.0f) {
        return false;
    }
    
    // Home thresholds (non-negative)
    if (config->home_threshold[0] > config->home_threshold[1] ||
        config->home_threshold[1] > config->home_threshold[2]) {
        return false;
    }
    if (config->home_threshold[0] < 0.0f || config->home_threshold[2] > 10.0f) {
        return false;
    }
    
    // Solar thresholds (non-negative)
    if (config->solar_threshold[0] > config->solar_threshold[1] ||
        config->solar_threshold[1] > config->solar_threshold[2]) {
        return false;
    }
    if (config->solar_threshold[0] < 0.0f || config->solar_threshold[2] > 10.0f) {
        return false;
    }
    
    // Color validation (24-bit RGB, top byte must be 0)
    if ((config->color_good & 0xFF000000) ||
        (config->color_ok & 0xFF000000) ||
        (config->color_attention & 0xFF000000) ||
        (config->color_warning & 0xFF000000)) {
        return false;
    }
    
    return true;
}
```

**File**: `src/app/config_manager.h`

**Add declaration**:
```cpp
bool config_manager_validate(const DeviceConfig* config);
```

---

### Step 5: Extend Web API Handlers

**File**: `src/app/web_portal.cpp`

#### 5a. Add Color Parsing Helper

```cpp
// Parse hex color string "#RRGGBB" to uint32_t
uint32_t parse_hex_color(const char* hex) {
    if (strlen(hex) != 7 || hex[0] != '#') return 0xFFFFFFFF;
    
    char* endptr;
    uint32_t value = strtoul(hex + 1, &endptr, 16);
    
    if (*endptr != '\0' || value > 0xFFFFFF) return 0xFFFFFFFF;
    return value;
}
```

#### 5b. Extend GET Handler

**Find**: `void handleGetConfig(AsyncWebServerRequest *request)`

**Add before `serializeJson()`**:
```cpp
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
    
    // Colors (convert uint32_t to hex string)
    char color_buf[8];
    sprintf(color_buf, "#%06X", current_config->color_good);
    doc["color_good"] = color_buf;
    sprintf(color_buf, "#%06X", current_config->color_ok);
    doc["color_ok"] = color_buf;
    sprintf(color_buf, "#%06X", current_config->color_attention);
    doc["color_attention"] = color_buf;
    sprintf(color_buf, "#%06X", current_config->color_warning);
    doc["color_warning"] = color_buf;
```

#### 5c. Extend POST Handler

**Find**: `void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len)`

**Add after parsing existing fields**:
```cpp
    // Parse threshold fields
    if (doc.containsKey("grid_threshold_0")) new_config.grid_threshold[0] = doc["grid_threshold_0"];
    if (doc.containsKey("grid_threshold_1")) new_config.grid_threshold[1] = doc["grid_threshold_1"];
    if (doc.containsKey("grid_threshold_2")) new_config.grid_threshold[2] = doc["grid_threshold_2"];
    if (doc.containsKey("home_threshold_0")) new_config.home_threshold[0] = doc["home_threshold_0"];
    if (doc.containsKey("home_threshold_1")) new_config.home_threshold[1] = doc["home_threshold_1"];
    if (doc.containsKey("home_threshold_2")) new_config.home_threshold[2] = doc["home_threshold_2"];
    if (doc.containsKey("solar_threshold_0")) new_config.solar_threshold[0] = doc["solar_threshold_0"];
    if (doc.containsKey("solar_threshold_1")) new_config.solar_threshold[1] = doc["solar_threshold_1"];
    if (doc.containsKey("solar_threshold_2")) new_config.solar_threshold[2] = doc["solar_threshold_2"];
    
    // Parse color fields
    if (doc.containsKey("color_good")) {
        uint32_t color = parse_hex_color(doc["color_good"]);
        if (color <= 0xFFFFFF) new_config.color_good = color;
    }
    if (doc.containsKey("color_ok")) {
        uint32_t color = parse_hex_color(doc["color_ok"]);
        if (color <= 0xFFFFFF) new_config.color_ok = color;
    }
    if (doc.containsKey("color_attention")) {
        uint32_t color = parse_hex_color(doc["color_attention"]);
        if (color <= 0xFFFFFF) new_config.color_attention = color;
    }
    if (doc.containsKey("color_warning")) {
        uint32_t color = parse_hex_color(doc["color_warning"]);
        if (color <= 0xFFFFFF) new_config.color_warning = color;
    }
    
    // Validate before saving
    if (!config_manager_validate(&new_config)) {
        request->send(400, "application/json", 
                     "{\"status\":\"error\",\"message\":\"Validation failed\"}");
        return;
    }
```

---

### Step 6: Add Web Portal UI

**File**: `src/app/web/home.html`

**Location**: Add new section after existing power display section, before footer

**HTML to add**:
```html
    <!-- Power Threshold Configuration -->
    <section>
        <h2>Power Thresholds</h2>
        
        <div class="form-group">
            <h3>Grid Power</h3>
            <label>
                Threshold 0 (Good → OK):
                <input type="number" id="grid_threshold_0" step="0.1" min="-10.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
            <label>
                Threshold 1 (OK → Attention):
                <input type="number" id="grid_threshold_1" step="0.1" min="-10.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
            <label>
                Threshold 2 (Attention → Warning):
                <input type="number" id="grid_threshold_2" step="0.1" min="-10.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
        </div>
        
        <div class="form-group">
            <h3>Home Power</h3>
            <label>
                Threshold 0 (Good → OK):
                <input type="number" id="home_threshold_0" step="0.1" min="0.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
            <label>
                Threshold 1 (OK → Attention):
                <input type="number" id="home_threshold_1" step="0.1" min="0.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
            <label>
                Threshold 2 (Attention → Warning):
                <input type="number" id="home_threshold_2" step="0.1" min="0.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
        </div>
        
        <div class="form-group">
            <h3>Solar Power</h3>
            <label>
                Threshold 0 (Good → OK):
                <input type="number" id="solar_threshold_0" step="0.1" min="0.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
            <label>
                Threshold 1 (OK → Attention):
                <input type="number" id="solar_threshold_1" step="0.1" min="0.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
            <label>
                Threshold 2 (Attention → Warning):
                <input type="number" id="solar_threshold_2" step="0.1" min="0.0" max="10.0" required>
                <span class="unit">kW</span>
            </label>
        </div>
    </section>
    
    <!-- Color Configuration -->
    <section>
        <h2>Colors</h2>
        <div class="form-group">
            <label>
                Good (Zone 0):
                <input type="color" id="color_good" required>
            </label>
            <label>
                OK (Zone 1):
                <input type="color" id="color_ok" required>
            </label>
            <label>
                Attention (Zone 2):
                <input type="color" id="color_attention" required>
            </label>
            <label>
                Warning (Zone 3):
                <input type="color" id="color_warning" required>
            </label>
        </div>
    </section>
```

---

### Step 7: Add JavaScript Validation

**File**: `src/app/web/portal.js`

**Add validation function**:
```javascript
function validateThresholds() {
    // Grid thresholds
    const gridT = [
        parseFloat(document.getElementById('grid_threshold_0').value),
        parseFloat(document.getElementById('grid_threshold_1').value),
        parseFloat(document.getElementById('grid_threshold_2').value)
    ];
    
    if (gridT[0] > gridT[1] || gridT[1] > gridT[2]) {
        alert('Grid thresholds must be in order: T0 ≤ T1 ≤ T2');
        return false;
    }
    
    if (gridT[0] < -10.0 || gridT[2] > 10.0) {
        alert('Grid thresholds must be between -10.0 and +10.0 kW');
        return false;
    }
    
    // Home thresholds
    const homeT = [
        parseFloat(document.getElementById('home_threshold_0').value),
        parseFloat(document.getElementById('home_threshold_1').value),
        parseFloat(document.getElementById('home_threshold_2').value)
    ];
    
    if (homeT[0] > homeT[1] || homeT[1] > homeT[2]) {
        alert('Home thresholds must be in order: T0 ≤ T1 ≤ T2');
        return false;
    }
    
    if (homeT[0] < 0.0 || homeT[2] > 10.0) {
        alert('Home thresholds must be between 0.0 and +10.0 kW');
        return false;
    }
    
    // Solar thresholds
    const solarT = [
        parseFloat(document.getElementById('solar_threshold_0').value),
        parseFloat(document.getElementById('solar_threshold_1').value),
        parseFloat(document.getElementById('solar_threshold_2').value)
    ];
    
    if (solarT[0] > solarT[1] || solarT[1] > solarT[2]) {
        alert('Solar thresholds must be in order: T0 ≤ T1 ≤ T2');
        return false;
    }
    
    if (solarT[0] < 0.0 || solarT[2] > 10.0) {
        alert('Solar thresholds must be between 0.0 and +10.0 kW');
        return false;
    }
    
    return true;
}
```

**Update existing save function**:
```javascript
function saveConfig() {
    if (!validateThresholds()) {
        return;  // Don't submit if validation fails
    }
    
    // ... existing save logic ...
}
```

**Update existing loadConfig function** to populate new fields:
```javascript
function loadConfig() {
    fetch('/api/config')
        .then(response => response.json())
        .then(data => {
            // ... existing field population ...
            
            // Populate threshold fields
            document.getElementById('grid_threshold_0').value = data.grid_threshold_0 || 0.0;
            document.getElementById('grid_threshold_1').value = data.grid_threshold_1 || 0.5;
            document.getElementById('grid_threshold_2').value = data.grid_threshold_2 || 2.5;
            document.getElementById('home_threshold_0').value = data.home_threshold_0 || 0.5;
            document.getElementById('home_threshold_1').value = data.home_threshold_1 || 1.0;
            document.getElementById('home_threshold_2').value = data.home_threshold_2 || 2.0;
            document.getElementById('solar_threshold_0').value = data.solar_threshold_0 || 0.5;
            document.getElementById('solar_threshold_1').value = data.solar_threshold_1 || 1.5;
            document.getElementById('solar_threshold_2').value = data.solar_threshold_2 || 3.0;
            
            // Populate color fields
            document.getElementById('color_good').value = data.color_good || '#00FF00';
            document.getElementById('color_ok').value = data.color_ok || '#FFFFFF';
            document.getElementById('color_attention').value = data.color_attention || '#FFA500';
            document.getElementById('color_warning').value = data.color_warning || '#FF0000';
        });
}
```

---

## Testing Checklist

### 1. NVS Migration Test
- [ ] Flash new firmware to device with existing config
- [ ] Verify factory defaults applied for new fields
- [ ] Verify existing config (hostname, MQTT, etc.) preserved

### 2. Web Portal Test
- [ ] Access `http://<device-ip>/` in browser
- [ ] Verify all 9 threshold fields + 4 color pickers appear
- [ ] Load config → verify default values populated

### 3. Validation Test
- [ ] Try invalid ordering (T0 > T1) → expect client alert
- [ ] Try out-of-range grid threshold (-15 kW) → expect alert
- [ ] Try negative home threshold (-0.5 kW) → expect alert
- [ ] Try valid equal thresholds (skip level) → expect success

### 4. Configuration Save Test
- [ ] Change grid thresholds → save → reload page → verify persisted
- [ ] Change colors → save → reload page → verify persisted
- [ ] Verify NVS write latency <100ms (check serial logs)

### 5. Display Update Test
- [ ] Monitor power screen via MQTT data changes
- [ ] Verify grid icon color changes at configured thresholds
- [ ] Verify home icon color changes at configured thresholds
- [ ] Verify solar icon color changes at configured thresholds
- [ ] Verify colors match configured RGB values

### 6. Edge Case Test
- [ ] Set all thresholds equal (T0=T1=T2) → verify single color shown
- [ ] Set T0=T1 (skip "ok" zone) → verify 3-color display
- [ ] Set negative grid threshold → verify export shows "good" color
- [ ] Test custom color scheme (blue/yellow/pink/purple) → verify applied

### 7. Performance Test
- [ ] Measure display FPS (should stay at 60 FPS)
- [ ] Measure MQTT latency (should stay <100ms)
- [ ] Verify no heap fragmentation after multiple saves

---

## Troubleshooting

### Issue: Colors appear swapped (red shows as blue)

**Cause**: Display hardware uses BGR format, not RGB  
**Solution**: Verify `rgb_to_bgr()` conversion in `get_power_color()`

---

### Issue: Thresholds not persisting after reboot

**Cause**: NVS write failure or validation rejection  
**Solution**: Check serial logs for validation errors, verify `config_manager_save()` called

---

### Issue: Web portal shows NaN or blank threshold values

**Cause**: GET handler not serializing new fields  
**Solution**: Verify `handleGetConfig()` includes threshold serialization

---

### Issue: Validation errors on every save attempt

**Cause**: Client/server validation mismatch  
**Solution**: Verify both use same ordering logic (T[i] <= T[i+1], not T[i] < T[i+1])

---

## Performance Expectations

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| NVS write latency | <100ms | Serial logs during POST /api/config |
| Display update latency | <16ms | One frame at 60 FPS |
| Color calculation overhead | <10µs per icon | 3 comparisons + 1 array lookup |
| Heap usage | 0 bytes | All operations use stack or global static |
| Flash usage | ~400 bytes | HTML + validation + conversion functions |

---

## Rollback Plan

If issues arise post-deployment:

1. **Revert to previous firmware**: Flash last known good version
2. **NVS migration**: Old firmware will ignore new fields (magic number validates struct size)
3. **Web portal**: Old firmware serves old home.html without threshold fields

**No data loss**: Existing config fields unaffected by rollback

---

## Next Steps After Implementation

1. **Test on all board targets**: ESP32, ESP32-C3, ESP32-C6
2. **Update CHANGELOG.md**: Document new configurable thresholds feature
3. **Create release**: Tag version with `create-release.sh`
4. **User documentation**: Update README.md with threshold configuration instructions

---

## Summary

**Files Modified**: 7
1. `config_manager.h` - DeviceConfig struct (+52 bytes)
2. `config_manager.cpp` - Defaults + validation
3. `screen_power.cpp` - Unified color algorithm
4. `screen_power.h` - Method declarations
5. `web_portal.cpp` - API handlers + color parsing
6. `web/home.html` - Threshold + color form fields
7. `web/portal.js` - Validation + load/save logic

**Backward Compatibility**: ✅ Old configs auto-migrate to defaults  
**Performance Impact**: ✅ Zero measurable impact on FPS or MQTT  
**Storage Cost**: ✅ 52 bytes NVS (0.05% of typical 100KB partition)

**Implementation complete** when all 7 testing checklist sections pass.
