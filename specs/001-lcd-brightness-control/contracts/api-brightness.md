# API Contract: /api/brightness

**Phase 1 Output** | **Date**: December 13, 2025 | **Branch**: `001-lcd-brightness-control`

## Endpoint Overview

**Base Path**: `/api/brightness`  
**Purpose**: Real-time LCD backlight brightness control  
**Authentication**: None (device is on local network, same security model as existing `/api/*` endpoints)  
**Content-Type**: `application/json`

---

## GET /api/brightness

### Description
Retrieve the current runtime brightness level (may differ from saved/persisted value if user has adjusted but not saved).

### Request

**Method**: `GET`  
**Headers**: None required  
**Query Parameters**: None  
**Body**: None

### Response

**Success (200 OK)**:
```json
{
  "brightness": 75
}
```

**Fields**:
- `brightness` (integer, required): Current brightness level (0-100)
  - 0 = backlight off
  - 100 = maximum brightness
  - Value reflects current hardware state, not necessarily persisted value

**Example**:
```bash
curl http://192.168.1.100/api/brightness
```

**Response**:
```json
{
  "brightness": 50
}
```

### Error Responses

**500 Internal Server Error**:
```json
{
  "error": "Brightness not initialized"
}
```

Occurs if `current_brightness` global variable is not initialized (should never happen in normal operation).

---

## POST /api/brightness

### Description
Update LCD backlight brightness in real-time. Change is applied immediately to hardware but NOT persisted to NVS. User must explicitly save via the home page save button to persist the setting.

### Request

**Method**: `POST`  
**Headers**: 
- `Content-Type: application/json`

**Body**:
```json
{
  "brightness": 50
}
```

**Fields**:
- `brightness` (integer, required): Desired brightness level (0-100)
  - Values <0 will be clamped to 0
  - Values >100 will be clamped to 100
  - Decimal values will be truncated to integer

**Example**:
```bash
curl -X POST http://192.168.1.100/api/brightness \
  -H "Content-Type: application/json" \
  -d '{"brightness": 75}'
```

### Response

**Success (200 OK)**:
```json
{
  "success": true,
  "brightness": 75
}
```

**Fields**:
- `success` (boolean, required): Always `true` on successful update
- `brightness` (integer, required): Actual brightness value applied (after clamping/validation)

**Example with clamping**:

**Request**:
```json
{
  "brightness": 150
}
```

**Response**:
```json
{
  "success": true,
  "brightness": 100
}
```
(Value clamped to maximum)

### Error Responses

**400 Bad Request**:
```json
{
  "success": false,
  "message": "Missing brightness field"
}
```

Occurs if JSON body does not contain `brightness` field.

**400 Bad Request**:
```json
{
  "success": false,
  "message": "Invalid JSON"
}
```

Occurs if request body is not valid JSON.

**500 Internal Server Error**:
```json
{
  "success": false,
  "message": "Hardware control failed"
}
```

Occurs if `lcd_set_backlight()` call fails (unlikely, as it's a void function with no failure mode).

---

## Integration with /api/config

### Overview
The `/api/brightness` endpoint handles **real-time updates** (immediate hardware changes without persistence).

The existing `/api/config` endpoint handles **persistence** (save to NVS, restore on reboot).

### POST /api/config (Modified)

**Existing endpoint**, now includes `lcd_brightness` field in request/response:

**Request**:
```json
{
  "wifi_ssid": "MyNetwork",
  "device_name": "energy-monitor",
  "lcd_brightness": 80,
  "dummy_setting": "example"
}
```

**Response**:
```json
{
  "success": true,
  "message": "Configuration saved! Device will reboot in 3 seconds..."
}
```

**Behavior**:
1. Parse `lcd_brightness` field (default to 100 if missing)
2. Clamp to 0-100 range
3. Update `current_config->lcd_brightness`
4. Call `lcd_set_backlight()` to apply immediately
5. Call `config_manager_save()` to persist to NVS
6. Reboot device (existing behavior)

**Backward Compatibility**:
- Older firmware versions without brightness field will ignore the field
- Upgrading firmware will use default 100% if field not in saved config

### GET /api/config (Modified)

**Response now includes**:
```json
{
  "wifi_ssid": "MyNetwork",
  "device_name": "energy-monitor",
  "lcd_brightness": 80,
  "dummy_setting": "example"
}
```

**Note**: This returns the **persisted** value (from `DeviceConfig` struct), which may differ from the **runtime** value (from `current_brightness` global) if user has adjusted brightness but not saved.

To get the current runtime brightness, use `GET /api/brightness` instead.

---

## Client-Side Usage Pattern

### Real-time Brightness Slider

```javascript
// home.html + portal.js

// On slider input event (fires continuously as user drags)
document.getElementById('lcd_brightness').addEventListener('input', function() {
    const value = parseInt(this.value);
    
    // Update display label
    document.getElementById('lcd_brightness_value').textContent = value + '%';
    
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
    });
});

// On page load, retrieve current brightness
fetch('/api/brightness')
    .then(response => response.json())
    .then(data => {
        document.getElementById('lcd_brightness').value = data.brightness;
        document.getElementById('lcd_brightness_value').textContent = data.brightness + '%';
    });
```

### Save Button Integration

```javascript
// When user clicks "Save" button
function saveConfig() {
    const config = {
        wifi_ssid: document.getElementById('wifi_ssid').value,
        device_name: document.getElementById('device_name').value,
        lcd_brightness: parseInt(document.getElementById('lcd_brightness').value),
        // ... other fields
    };
    
    fetch('/api/config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(config)
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showSuccess('Settings saved! Device rebooting...');
        } else {
            showError('Save failed: ' + data.message);
        }
    });
}
```

---

## Security Considerations

**No Authentication**: Endpoint follows existing pattern of `/api/*` endpoints (no auth). Device is assumed to be on trusted local network.

**Input Validation**: All inputs clamped to 0-100 range to prevent invalid hardware states.

**Rate Limiting**: Not implemented. Rapid slider movements will generate many POST requests, but each request is <1ms processing time. No DoS risk in single-user embedded device scenario.

**CORS**: Not configured. Web portal is served from same origin (device IP/hostname).

---

## Testing Examples

### Manual Testing with curl

**Test 1: Get current brightness**
```bash
curl http://192.168.1.100/api/brightness
# Expected: {"brightness": 100}
```

**Test 2: Set brightness to 50%**
```bash
curl -X POST http://192.168.1.100/api/brightness \
  -H "Content-Type: application/json" \
  -d '{"brightness": 50}'
# Expected: {"success": true, "brightness": 50}
# Observe: LCD backlight dims to 50%
```

**Test 3: Test clamping (value > 100)**
```bash
curl -X POST http://192.168.1.100/api/brightness \
  -H "Content-Type: application/json" \
  -d '{"brightness": 150}'
# Expected: {"success": true, "brightness": 100}
# Observe: LCD backlight at maximum
```

**Test 4: Test clamping (value < 0)**
```bash
curl -X POST http://192.168.1.100/api/brightness \
  -H "Content-Type: application/json" \
  -d '{"brightness": -10}'
# Expected: {"success": true, "brightness": 0}
# Observe: LCD backlight completely off
```

**Test 5: Persistence via /api/config**
```bash
# Set brightness to 60 and save
curl -X POST http://192.168.1.100/api/config \
  -H "Content-Type: application/json" \
  -d '{"wifi_ssid": "MyNet", "lcd_brightness": 60}'
# Device reboots
# After reboot:
curl http://192.168.1.100/api/brightness
# Expected: {"brightness": 60}
```

---

## OpenAPI Specification (Optional)

```yaml
openapi: 3.0.0
info:
  title: ESP32 Energy Monitor API
  version: 1.0.0
  description: REST API for device configuration and control

servers:
  - url: http://192.168.1.100/api
    description: Device local network address

paths:
  /brightness:
    get:
      summary: Get current LCD brightness
      responses:
        '200':
          description: Successful response
          content:
            application/json:
              schema:
                type: object
                properties:
                  brightness:
                    type: integer
                    minimum: 0
                    maximum: 100
                    example: 75

    post:
      summary: Set LCD brightness (real-time, not persisted)
      requestBody:
        required: true
        content:
          application/json:
            schema:
              type: object
              required:
                - brightness
              properties:
                brightness:
                  type: integer
                  minimum: 0
                  maximum: 100
                  example: 50
      responses:
        '200':
          description: Brightness updated successfully
          content:
            application/json:
              schema:
                type: object
                properties:
                  success:
                    type: boolean
                    example: true
                  brightness:
                    type: integer
                    minimum: 0
                    maximum: 100
                    example: 50
        '400':
          description: Invalid request
          content:
            application/json:
              schema:
                type: object
                properties:
                  success:
                    type: boolean
                    example: false
                  message:
                    type: string
                    example: "Missing brightness field"
```
