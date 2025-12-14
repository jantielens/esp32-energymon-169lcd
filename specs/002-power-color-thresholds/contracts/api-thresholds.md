# API Contract: Power Threshold Configuration

**Feature**: 002-power-color-thresholds  
**Phase**: 1 - Design & Contracts  
**Date**: December 13, 2025  
**Revised**: December 13, 2025 (Consistent 4-level structure + configurable colors)

## Overview

This document defines the REST API contract for configuring power thresholds and colors via the web portal. The API extends the existing `/api/config` endpoint with 13 new fields (9 thresholds + 4 colors).

---

## Endpoint Extension

### `GET /api/config`

**Description**: Retrieve current device configuration including power thresholds and colors

**Method**: `GET`  
**Authentication**: None (assumes local network access)  
**Content-Type**: `application/json`

#### Response Schema

```json
{
  "hostname": "string",
  "mqtt_broker": "string",
  "mqtt_port": number,
  "mqtt_user": "string",
  
  // Power Thresholds (NEW - 9 fields)
  "grid_threshold_0": number,
  "grid_threshold_1": number,
  "grid_threshold_2": number,
  "home_threshold_0": number,
  "home_threshold_1": number,
  "home_threshold_2": number,
  "solar_threshold_0": number,
  "solar_threshold_1": number,
  "solar_threshold_2": number,
  
  // Color Configuration (NEW - 4 fields)
  "color_good": "string",       // Hex format: "#RRGGBB"
  "color_ok": "string",          // Hex format: "#RRGGBB"
  "color_attention": "string",   // Hex format: "#RRGGBB"
  "color_warning": "string"      // Hex format: "#RRGGBB"
}
```

#### Example Response (200 OK)

```json
{
  "hostname": "esp32-energy",
  "mqtt_broker": "192.168.1.100",
  "mqtt_port": 1883,
  "mqtt_user": "energymon",
  
  "grid_threshold_0": 0.0,
  "grid_threshold_1": 0.5,
  "grid_threshold_2": 2.5,
  "home_threshold_0": 0.5,
  "home_threshold_1": 1.0,
  "home_threshold_2": 2.0,
  "solar_threshold_0": 0.5,
  "solar_threshold_1": 1.5,
  "solar_threshold_2": 3.0,
  
  "color_good": "#00FF00",
  "color_ok": "#FFFFFF",
  "color_attention": "#FFA500",
  "color_warning": "#FF0000"
}
```

#### Field Descriptions

| Field | Type | Range | Default | Description |
|-------|------|-------|---------|-------------|
| `grid_threshold_0` | float | -10.0 to +10.0 | 0.0 | Grid good→ok boundary (kW) |
| `grid_threshold_1` | float | -10.0 to +10.0 | 0.5 | Grid ok→attention boundary (kW) |
| `grid_threshold_2` | float | -10.0 to +10.0 | 2.5 | Grid attention→warning boundary (kW) |
| `home_threshold_0` | float | 0.0 to +10.0 | 0.5 | Home good→ok boundary (kW) |
| `home_threshold_1` | float | 0.0 to +10.0 | 1.0 | Home ok→attention boundary (kW) |
| `home_threshold_2` | float | 0.0 to +10.0 | 2.0 | Home attention→warning boundary (kW) |
| `solar_threshold_0` | float | 0.0 to +10.0 | 0.5 | Solar good→ok boundary (kW) |
| `solar_threshold_1` | float | 0.0 to +10.0 | 1.5 | Solar ok→attention boundary (kW) |
| `solar_threshold_2` | float | 0.0 to +10.0 | 3.0 | Solar attention→warning boundary (kW) |
| `color_good` | string | #000000 to #FFFFFF | #00FF00 | Color for "good" zone (green) |
| `color_ok` | string | #000000 to #FFFFFF | #FFFFFF | Color for "ok" zone (white) |
| `color_attention` | string | #000000 to #FFFFFF | #FFA500 | Color for "attention" zone (orange) |
| `color_warning` | string | #000000 to #FFFFFF | #FF0000 | Color for "warning" zone (red) |

---

### `POST /api/config`

**Description**: Update device configuration including power thresholds and colors

**Method**: `POST`  
**Authentication**: None (assumes local network access)  
**Content-Type**: `application/json`

#### Request Schema

Same as GET response schema (all fields optional except those changing)

#### Request Example

```json
{
  "grid_threshold_0": -0.5,
  "grid_threshold_1": 0.0,
  "grid_threshold_2": 1.5,
  "home_threshold_0": 0.8,
  "home_threshold_1": 1.5,
  "home_threshold_2": 2.5,
  "solar_threshold_0": 1.0,
  "solar_threshold_1": 2.0,
  "solar_threshold_2": 4.0,
  
  "color_good": "#0000FF",
  "color_ok": "#FFFF00",
  "color_attention": "#FF69B4",
  "color_warning": "#800080"
}
```

#### Success Response (200 OK)

```json
{
  "status": "success",
  "message": "Configuration saved"
}
```

#### Error Response (400 Bad Request)

```json
{
  "status": "error",
  "message": "Validation failed: <specific error>"
}
```

**Error Messages**:
- `"Grid thresholds out of order (T0 > T1 or T1 > T2)"`
- `"Home thresholds out of order (T0 > T1 or T1 > T2)"`
- `"Solar thresholds out of order (T0 > T1 or T1 > T2)"`
- `"Grid threshold out of range (-10.0 to +10.0 kW)"`
- `"Home threshold out of range (0.0 to +10.0 kW)"`
- `"Solar threshold out of range (0.0 to +10.0 kW)"`
- `"Invalid color format (must be #RRGGBB)"`
- `"Color value out of range (0x000000 to 0xFFFFFF)"`

---

## Validation Rules

### Server-Side Validation (C++)

**Priority**: All validation enforced before NVS write (security: never trust client)

#### 1. Threshold Ordering

```cpp
// Grid thresholds
if (config->grid_threshold[0] > config->grid_threshold[1] ||
    config->grid_threshold[1] > config->grid_threshold[2]) {
    return error("Grid thresholds out of order (T0 > T1 or T1 > T2)");
}

// Home thresholds
if (config->home_threshold[0] > config->home_threshold[1] ||
    config->home_threshold[1] > config->home_threshold[2]) {
    return error("Home thresholds out of order (T0 > T1 or T1 > T2)");
}

// Solar thresholds
if (config->solar_threshold[0] > config->solar_threshold[1] ||
    config->solar_threshold[1] > config->solar_threshold[2]) {
    return error("Solar thresholds out of order (T0 > T1 or T1 > T2)");
}
```

**Note**: Equal values (`T[i] == T[i+1]`) are allowed to skip color levels

#### 2. Range Validation

```cpp
// Grid can be negative (export scenarios)
if (config->grid_threshold[0] < -10.0f || config->grid_threshold[2] > 10.0f) {
    return error("Grid threshold out of range (-10.0 to +10.0 kW)");
}

// Home and solar must be non-negative
if (config->home_threshold[0] < 0.0f || config->home_threshold[2] > 10.0f) {
    return error("Home threshold out of range (0.0 to +10.0 kW)");
}

if (config->solar_threshold[0] < 0.0f || config->solar_threshold[2] > 10.0f) {
    return error("Solar threshold out of range (0.0 to +10.0 kW)");
}
```

#### 3. Color Validation

**Hex String → uint32_t Conversion**:
```cpp
uint32_t parse_hex_color(const char* hex) {
    // Expected format: "#RRGGBB" (7 chars)
    if (strlen(hex) != 7 || hex[0] != '#') return 0xFFFFFFFF;  // Invalid
    
    char* endptr;
    uint32_t value = strtoul(hex + 1, &endptr, 16);
    
    if (*endptr != '\0' || value > 0xFFFFFF) return 0xFFFFFFFF;  // Invalid
    return value;
}
```

**Validation**:
```cpp
uint32_t color = parse_hex_color(json["color_good"]);
if (color > 0xFFFFFF) {
    return error("Invalid color format (must be #RRGGBB)");
}
```

---

### Client-Side Validation (JavaScript)

**Priority**: Validate before POST to provide instant user feedback

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
    
    // Color validation (HTML color input handles format automatically)
    // Browser ensures #RRGGBB format from <input type="color">
    
    return true;
}
```

---

## Implementation Details

### C++ Handler (web_portal.cpp)

#### GET Handler Extension

```cpp
void handleGetConfig(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(2048);
    
    // ... existing fields ...
    
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
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
```

#### POST Handler Extension

```cpp
void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, (const char*)data);
    
    DeviceConfig new_config = *current_config;  // Copy current
    
    // Parse thresholds (if present)
    if (doc.containsKey("grid_threshold_0")) new_config.grid_threshold[0] = doc["grid_threshold_0"];
    if (doc.containsKey("grid_threshold_1")) new_config.grid_threshold[1] = doc["grid_threshold_1"];
    if (doc.containsKey("grid_threshold_2")) new_config.grid_threshold[2] = doc["grid_threshold_2"];
    if (doc.containsKey("home_threshold_0")) new_config.home_threshold[0] = doc["home_threshold_0"];
    if (doc.containsKey("home_threshold_1")) new_config.home_threshold[1] = doc["home_threshold_1"];
    if (doc.containsKey("home_threshold_2")) new_config.home_threshold[2] = doc["home_threshold_2"];
    if (doc.containsKey("solar_threshold_0")) new_config.solar_threshold[0] = doc["solar_threshold_0"];
    if (doc.containsKey("solar_threshold_1")) new_config.solar_threshold[1] = doc["solar_threshold_1"];
    if (doc.containsKey("solar_threshold_2")) new_config.solar_threshold[2] = doc["solar_threshold_2"];
    
    // Parse colors (if present)
    if (doc.containsKey("color_good")) {
        new_config.color_good = parse_hex_color(doc["color_good"]);
    }
    if (doc.containsKey("color_ok")) {
        new_config.color_ok = parse_hex_color(doc["color_ok"]);
    }
    if (doc.containsKey("color_attention")) {
        new_config.color_attention = parse_hex_color(doc["color_attention"]);
    }
    if (doc.containsKey("color_warning")) {
        new_config.color_warning = parse_hex_color(doc["color_warning"]);
    }
    
    // Validate before saving
    if (!validate_config(&new_config)) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Validation failed\"}");
        return;
    }
    
    // Save to NVS
    config_manager_save(&new_config);
    *current_config = new_config;  // Update global
    
    request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved\"}");
}
```

---

## Example Workflows

### Workflow 1: Load Default Configuration

**Client**:
```
GET /api/config
```

**Server Response (200 OK)**:
```json
{
  "grid_threshold_0": 0.0,
  "grid_threshold_1": 0.5,
  "grid_threshold_2": 2.5,
  "home_threshold_0": 0.5,
  "home_threshold_1": 1.0,
  "home_threshold_2": 2.0,
  "solar_threshold_0": 0.5,
  "solar_threshold_1": 1.5,
  "solar_threshold_2": 3.0,
  "color_good": "#00FF00",
  "color_ok": "#FFFFFF",
  "color_attention": "#FFA500",
  "color_warning": "#FF0000"
}
```

---

### Workflow 2: Update Grid Thresholds Only

**Client**:
```
POST /api/config
Content-Type: application/json

{
  "grid_threshold_0": -1.0,
  "grid_threshold_1": 0.0,
  "grid_threshold_2": 2.0
}
```

**Server Response (200 OK)**:
```json
{
  "status": "success",
  "message": "Configuration saved"
}
```

---

### Workflow 3: Update Colors Only

**Client**:
```
POST /api/config
Content-Type: application/json

{
  "color_good": "#0000FF",
  "color_warning": "#800080"
}
```

**Server Response (200 OK)**:
```json
{
  "status": "success",
  "message": "Configuration saved"
}
```

---

### Workflow 4: Validation Error (Ordering)

**Client**:
```
POST /api/config
Content-Type: application/json

{
  "home_threshold_0": 2.0,
  "home_threshold_1": 1.0,
  "home_threshold_2": 3.0
}
```

**Server Response (400 Bad Request)**:
```json
{
  "status": "error",
  "message": "Home thresholds out of order (T0 > T1 or T1 > T2)"
}
```

---

### Workflow 5: Validation Error (Range)

**Client**:
```
POST /api/config
Content-Type: application/json

{
  "home_threshold_0": -0.5,
  "home_threshold_1": 1.0,
  "home_threshold_2": 2.0
}
```

**Server Response (400 Bad Request)**:
```json
{
  "status": "error",
  "message": "Home threshold out of range (0.0 to +10.0 kW)"
}
```

---

### Workflow 6: Skip Color Level (Valid)

**Client**:
```
POST /api/config
Content-Type: application/json

{
  "solar_threshold_0": 1.0,
  "solar_threshold_1": 1.0,
  "solar_threshold_2": 3.0
}
```

**Server Response (200 OK)**:
```json
{
  "status": "success",
  "message": "Configuration saved"
}
```

**Result**: Solar display skips "ok" zone (white), uses only green/orange/red

---

## Data Types and Precision

| Field | C++ Type | JSON Type | Precision | Notes |
|-------|----------|-----------|-----------|-------|
| Thresholds | `float` | `number` | 0.1 kW | Web form uses `step="0.1"` |
| Colors | `uint32_t` | `string` | 24-bit RGB | Hex format `#RRGGBB` |

**Storage Efficiency**:
- Thresholds: 9 × 4 bytes = 36 bytes
- Colors: 4 × 4 bytes = 16 bytes
- Total: 52 bytes NVS

---

## Backward Compatibility

**Existing Configs**: Devices with old firmware versions (no threshold fields) will:
1. Load existing config from NVS
2. Detect size mismatch (`loaded_size < sizeof(DeviceConfig)`)
3. Apply factory defaults for new fields
4. Continue operating normally

**API Compatibility**: GET/POST endpoints maintain existing field structure, new fields additive only

---

## Security Considerations

1. **No Authentication**: Assumes trusted local network (standard for embedded web portals)
2. **Dual Validation**: Client + server validation prevents invalid data persistence
3. **Input Sanitization**: Color parsing rejects invalid hex formats
4. **Range Limits**: All thresholds clamped to ±10 kW maximum
5. **Injection Prevention**: ArduinoJson library handles JSON parsing safely

---

## Performance Impact

| Operation | Latency | Notes |
|-----------|---------|-------|
| GET /api/config | <10ms | JSON serialization of 20+ fields |
| POST /api/config | <100ms | Validation + NVS write + global update |
| Display update | <16ms | One frame at 60 FPS |

**No impact on MQTT latency or display FPS**

---

## Conclusion

The API contract extends `/api/config` with 13 new fields (9 thresholds + 4 colors) while maintaining backward compatibility. Dual validation (client + server) ensures data integrity. Color zones update in real-time (<100ms user-perceived latency).

**Endpoints**: 2 (GET/POST /api/config extensions)  
**New Fields**: 13 (9 float thresholds, 4 RGB colors)  
**Validation Rules**: 3 (ordering, range, color format)  
**Error Messages**: 8 (specific failure reasons)

**Ready for implementation quickstart guide**
