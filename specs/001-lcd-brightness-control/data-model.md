# Data Model: LCD Backlight Brightness Control

**Phase 1 Output** | **Date**: December 13, 2025 | **Branch**: `001-lcd-brightness-control`

## Entities

### Brightness Setting

**Description**: Represents the LCD backlight intensity level (0-100%). Exists in three states: persisted (NVS), in-memory (current runtime value), and temporary (web UI slider position before save).

**Attributes**:
- **Value**: Integer (0-100)
  - 0 = backlight completely off (LCD still powered, no display visible)
  - 100 = maximum brightness (default)
  - Granularity: 1% increments (101 discrete values)
- **State**: Enum {PERSISTED, RUNTIME, TEMPORARY}
  - PERSISTED: Value stored in NVS, restored on boot
  - RUNTIME: Current brightness applied to hardware (may differ from PERSISTED if user adjusted but didn't save)
  - TEMPORARY: Web UI slider value during adjustment (before user releases slider or saves)

**Relationships**:
- **Stored in**: DeviceConfig struct (C++)
- **Applied to**: LCD Backlight hardware (via PWM)
- **Exposed via**: REST API (`/api/brightness`)
- **Persisted in**: ESP32 NVS (key: `lcd_bright`)

**Validation Rules**:
- Value MUST be clamped to 0-100 range on all inputs (web UI, API, NVS load)
- Default value MUST be 100% when no persisted setting exists
- Hardware PWM value calculated as `(brightness * 255) / 100` (0-255 range)

**State Transitions**:
```
TEMPORARY (slider adjusted)
    ↓ (on slider release)
RUNTIME (POST /api/brightness)
    ↓ (lcd_set_backlight called)
HARDWARE (PWM updated)
    ↓ (user clicks save button)
PERSISTED (config_manager_save)
```

**Lifecycle**:
1. **Boot**: Load PERSISTED value from NVS (default 100%), apply to RUNTIME, set HARDWARE
2. **Real-time Adjust**: User moves slider → TEMPORARY → POST /api/brightness → RUNTIME → HARDWARE
3. **Save**: User clicks save → copy RUNTIME to PERSISTED → write to NVS
4. **Reboot**: PERSISTED value restored to RUNTIME on next boot

---

## Storage Schema

### NVS (Non-Volatile Storage)

**Namespace**: `device_config` (existing, shared with WiFi/MQTT settings)

**Key**: `lcd_bright` (8 characters, follows existing naming pattern)

**Data Type**: `uint8_t` (unsigned 8-bit integer, 0-255 range, values >100 clamped)

**Size**: 1 byte per device

**Default**: 100 (if key not found in NVS)

**Migration**: On firmware upgrade from version without this field, NVS read will return default 100%, ensuring backward compatibility. Existing configs remain valid.

---

### In-Memory Representation

**C++ Structure** (src/app/config_manager.h):
```cpp
struct DeviceConfig {
    // ... existing fields (wifi_ssid, mqtt_broker, etc.) ...
    
    // LCD brightness (0-100%, default 100)
    uint8_t lcd_brightness;
    
    // ... magic field ...
};
```

**Global Variable** (src/app/web_portal.cpp):
```cpp
// Current runtime brightness (may differ from saved value)
static uint8_t current_brightness = 100;
```

**Initialization**:
```cpp
// In config_manager_load()
config->lcd_brightness = preferences.getUChar("lcd_bright", 100);

// In web_portal_init() or display_init()
current_brightness = device_config.lcd_brightness;
lcd_set_backlight(current_brightness);
```

---

## Data Flow

### Read Path (GET /api/brightness)

```
User opens home page
    ↓
JavaScript calls GET /api/brightness
    ↓
handleGetBrightness() reads current_brightness
    ↓
Return JSON {"brightness": 75}
    ↓
JavaScript updates slider position
```

### Real-time Update Path (POST /api/brightness)

```
User moves slider
    ↓
JavaScript input event fires
    ↓
POST /api/brightness with {"brightness": 50}
    ↓
handlePostBrightness() validates 0-100
    ↓
Update current_brightness = 50
    ↓
Call lcd_set_backlight(50)
    ↓
analogWrite(LCD_BL_PIN, 127)  // (50 * 255) / 100
    ↓
PWM signal → LCD backlight dims
    ↓
Return JSON {"success": true}
```

### Persistence Path (Save Button)

```
User clicks "Save" button
    ↓
JavaScript calls POST /api/config with all fields
    ↓
handlePostConfig() parses JSON
    ↓
current_config->lcd_brightness = brightness_value
    ↓
config_manager_save(current_config)
    ↓
preferences.putUChar("lcd_bright", brightness)
    ↓
NVS write committed
    ↓
Device reboots
    ↓
On next boot: preferences.getUChar("lcd_bright") restores value
```

### Boot Restoration Path

```
Device powers on
    ↓
config_manager_load(&device_config)
    ↓
device_config.lcd_brightness = NVS read (or default 100)
    ↓
display_init() or app.ino setup()
    ↓
lcd_set_backlight(device_config.lcd_brightness)
    ↓
LCD backlight set to saved brightness
```

---

## Validation & Constraints

**Input Validation**:
- Web UI: HTML range slider `min="0" max="100" step="1"` enforces client-side
- API: Server-side clamp `if (brightness > 100) brightness = 100;`
- NVS Load: Default to 100 if invalid or missing

**Concurrency**:
- No locking required (single-threaded Arduino main loop)
- Concurrent web requests handled sequentially by async web server
- Last write wins (no conflict resolution needed per spec)

**Backward Compatibility**:
- Devices upgrading from firmware without brightness field will:
  1. NVS read returns default 100% (key not found)
  2. Device boots with full brightness (same behavior as before)
  3. User can adjust and save new preference
- No migration script needed (additive change only)

**Forward Compatibility**:
- Future features requiring sub-percent precision can change `uint8_t` to `uint16_t` (0-1000 for 0.1% steps) with MAJOR version bump
- Current API contract (`/api/brightness` with integer 0-100) would require deprecation and replacement

---

## Error Handling

**NVS Read Failure**:
- Action: Use default value 100%
- User Impact: Device boots with full brightness (safe fallback)
- Recovery: User can adjust and save new preference

**NVS Write Failure**:
- Action: Log error, return failure in save response
- User Impact: Settings not persisted, reverts to last saved value on reboot
- Recovery: User retries save operation

**Invalid API Input**:
- Action: Clamp to 0-100 range, log warning
- User Impact: Brightness set to nearest valid value (0 or 100)
- Recovery: Automatic correction, no user action needed

**Hardware PWM Failure**:
- Action: Not detectable (analogWrite has no return value)
- User Impact: Brightness may not change visually
- Recovery: Power cycle device (reinitializes GPIO)

---

## Performance Characteristics

**Storage Overhead**:
- NVS: 1 byte per key-value pair (~16 bytes total with NVS metadata)
- RAM: 1 byte in DeviceConfig struct, 1 byte in current_brightness global
- Flash: ~500 bytes additional HTML/JS (gzipped)

**Runtime Performance**:
- PWM update: <1ms (single GPIO write)
- NVS write: ~10-50ms (flash write operation, happens only on save)
- API response: <5ms (JSON serialization + network transmission)

**Scalability**:
- Single global brightness setting (not per-user or per-page)
- No scaling concerns (embedded single-device deployment)
