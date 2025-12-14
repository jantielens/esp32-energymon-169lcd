# Data Model: Configurable Power Color Thresholds

**Feature**: 002-power-color-thresholds  
**Phase**: 1 - Design & Contracts  
**Date**: December 13, 2025  
**Revised**: December 14, 2025 (Added rolling statistics entities)

## Overview

This document defines entities, relationships, validation rules, and state transitions for the configurable power threshold and color system, plus rolling statistics overlay. The design uses consistent 3-threshold, 4-color structure across all power types, enhanced with 10-minute min/max visual indicators.

---

## Entity Definitions

### 1. PowerThresholdConfiguration

**Description**: Configuration for power display color thresholds and colors, persisted in NVS

**Attributes**:

| Field | Type | Constraints | Default | Description |
|-------|------|-------------|---------|-------------|
| `grid_threshold[3]` | `float[]` | T[0] <= T[1] <= T[2], -10.0 to +10.0 kW | `[0.0, 0.5, 2.5]` | Grid power boundaries (can be negative) |
| `home_threshold[3]` | `float[]` | T[0] <= T[1] <= T[2], 0.0 to +10.0 kW | `[0.5, 1.0, 2.0]` | Home consumption boundaries |
| `solar_threshold[3]` | `float[]` | T[0] <= T[1] <= T[2], 0.0 to +10.0 kW | `[0.5, 1.5, 3.0]` | Solar generation boundaries |
| `color_good` | `uint32_t` | 0x000000 to 0xFFFFFF (24-bit RGB) | `0x00FF00` | Color for values below T[0] |
| `color_ok` | `uint32_t` | 0x000000 to 0xFFFFFF (24-bit RGB) | `0xFFFFFF` | Color for values T[0] to T[1] |
| `color_attention` | `uint32_t` | 0x000000 to 0xFFFFFF (24-bit RGB) | `0xFFA500` | Color for values T[1] to T[2] |
| `color_warning` | `uint32_t` | 0x000000 to 0xFFFFFF (24-bit RGB) | `0xFF0000` | Color for values >= T[2] |

**Storage**: Part of `DeviceConfig` struct in NVS  
**Lifecycle**: Created with factory defaults on first boot, persisted across reboots, updated via web portal  
**Relationships**: Referenced by `PowerScreen` for color determination

---

### 2. ColorCodedPowerValue

**Description**: Runtime power value with computed color based on thresholds

**Attributes**:

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| `value` | `float` | Any (grid can be negative) | Current power reading in kW |
| `color` | `lv_color_t` | LVGL color (BGR format) | Computed color based on threshold zones |
| `zone` | `enum` | `GOOD`, `OK`, `ATTENTION`, `WARNING` | Semantic zone for analytics |

**Lifecycle**: Computed every frame (60 FPS) in `PowerScreen::update()`  
**Relationships**: Derived from `PowerThresholdConfiguration` + live MQTT data

---

### 4. PowerStatistics

**Description**: Rolling statistics tracker for a single power type over 10-minute window

**Attributes**:

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| `buffer[600]` | `float[]` | Any float value | Circular buffer storing power samples |
| `buffer_index` | `uint16_t` | 0 to 599 | Current write position in circular buffer |
| `sample_count` | `uint16_t` | 0 to 600 | Number of valid samples collected |
| `cached_min` | `float` | Any float value | Cached minimum value (recalculated on new sample) |
| `cached_max` | `float` | Any float value | Cached maximum value (recalculated on new sample) |
| `cached_avg` | `float` | Any float value | Cached average value (recalculated on new sample) |

**Methods**:

| Method | Description | Complexity |
|--------|-------------|------------|
| `addSample(float)` | Add new sample, overwrite oldest if buffer full | O(1) |
| `recalculate()` | Scan buffer to compute min/max/avg, excluding NaN | O(n) where n=600 |
| `getMin()` | Return cached minimum value | O(1) |
| `getMax()` | Return cached maximum value | O(1) |
| `getAvg()` | Return cached average value | O(1) |
| `hasData()` | Check if any valid samples exist | O(1) |
| `getSampleCount()` | Return number of valid samples | O(1) |

**Storage**: Stack/heap allocation (not persisted), 600 samples × 4 bytes = 2400 bytes per instance  
**Lifecycle**: Created in `PowerScreen::create()`, destroyed in `PowerScreen::destroy()`, reset on device reboot  
**Relationships**: Three instances created (solar_stats, home_stats, grid_stats) in PowerScreen

---

### 5. StatisticsOverlayLine

**Description**: LVGL line object representing min or max value on power bar

**Attributes**:

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| `line_object` | `lv_obj_t*` | LVGL object pointer | Line widget |
| `points[2]` | `lv_point_t[]` | Relative coordinates | Start and end points (horizontal line) |
| `value_kw` | `float` | 0.0 to 3.0 kW | Power value this line represents |
| `bar_x_offset` | `int32_t` | -107, 0, or 107 | X position relative to screen center |

**Visual Properties**:
- Width: 1px
- Color: White (0xFFFFFF)
- Opacity: 70% (LV_OPA_70)
- Length: 12px (bar width) + 7px extension (2px left, 5px right)
- Position: Aligned to bar using `lv_obj_align(LV_ALIGN_TOP_MID, bar_x_offset + 1, y_pos)`

**Lifecycle**: Created in `PowerScreen::create()`, updated in `update_stat_overlays()`, destroyed in `PowerScreen::destroy()`  
**Relationships**: Six instances (solar_line_min, solar_line_max, home_line_min, home_line_max, grid_line_min, grid_line_max)

---

### 3. WebPortalThresholdForm

**Description**: HTML form for configuring thresholds and colors

**Attributes**:

| Field | Type | HTML Input | Validation |
|-------|------|------------|------------|
| Grid T0, T1, T2 | `float` | `<input type="number" step="0.1">` | -10.0 to +10.0, T[i] <= T[i+1] |
| Home T0, T1, T2 | `float` | `<input type="number" step="0.1">` | 0.0 to +10.0, T[i] <= T[i+1] |
| Solar T0, T1, T2 | `float` | `<input type="number" step="0.1">` | 0.0 to +10.0, T[i] <= T[i+1] |
| Good Color | `string` | `<input type="color">` | 6-digit hex (#RRGGBB) |
| OK Color | `string` | `<input type="color">` | 6-digit hex (#RRGGBB) |
| Attention Color | `string` | `<input type="color">` | 6-digit hex (#RRGGBB) |
| Warning Color | `string` | `<input type="color">` | 6-digit hex (#RRGGBB) |

**Lifecycle**: User fills form → Submit → Client validation → POST `/api/config` → Server validation → NVS persist  
**Relationships**: Serializes to `PowerThresholdConfiguration`

---

## Validation Rules

### 1. Threshold Ordering

**Rule**: For each power type, `T[0] <= T[1] <= T[2]`

**Rationale**: Defines sequential color zones, allows equal values to skip levels

**Examples**:
- Valid: `[0.5, 1.0, 2.0]` → 4 distinct zones
- Valid: `[0.5, 0.5, 2.0]` → 3 zones (skips "ok")
- Valid: `[0.5, 0.5, 0.5]` → 1 zone (all "warning" color)
- Invalid: `[1.0, 0.5, 2.0]` → T[0] > T[1]

**Implementation**:
```cpp
bool validate_threshold_ordering(float t[3]) {
    return (t[0] <= t[1]) && (t[1] <= t[2]);
}
```

---

### 2. Range Constraints

**Grid Power**:
- Minimum: `-10.0 kW` (strong export to grid)
- Maximum: `+10.0 kW` (strong import from grid)
- Rationale: Supports bidirectional power flow

**Home and Solar Power**:
- Minimum: `0.0 kW` (no consumption/generation)
- Maximum: `+10.0 kW` (maximum expected usage/generation)
- Rationale: Unidirectional consumption/generation

**Implementation**:
```cpp
bool validate_range(float t[3], float min, float max) {
    return (t[0] >= min && t[2] <= max);
}
```

---

### 3. Color Format

**Rule**: Colors must be valid 24-bit RGB values (0x000000 to 0xFFFFFF)

**Web Input**: 6-digit hex string (`#RRGGBB`)  
**Storage**: `uint32_t` with top byte zero (`0x00RRGGBB`)

**Validation**:
```cpp
bool validate_color(uint32_t color) {
    return (color & 0xFF000000) == 0;  // Top byte must be 0
}
```

**Conversion from Hex String**:
```javascript
function hexToRgb(hex) {
    const value = parseInt(hex.substring(1), 16);  // Strip # and parse
    if (isNaN(value) || value < 0 || value > 0xFFFFFF) return null;
    return value;
}
```

---

## State Transitions

### PowerThresholdConfiguration States

```
┌──────────────────┐
│ Factory Defaults │  (First boot)
└────────┬─────────┘
         │
         v
┌──────────────────┐
│  Loaded from NVS │◄──────────────┐
└────────┬─────────┘               │
         │                         │
         │ User edits via web      │
         │                         │
         v                         │
┌──────────────────┐               │
│  Form Submitted  │               │
└────────┬─────────┘               │
         │                         │
         │ Validation              │
         │                         │
    ┌────v────┐                    │
    │ Valid?  │                    │
    └─┬────┬──┘                    │
      │    │                       │
   NO │    │ YES                   │
      │    │                       │
      v    v                       │
  ┌────┐ ┌──────────────┐          │
  │Err │ │ Save to NVS  │──────────┘
  └────┘ └──────────────┘
           │
           v
      ┌──────────────┐
      │Display Update│  (Next frame)
      └──────────────┘
```

**States**:
1. **Factory Defaults**: Initial values on first boot or after reset
2. **Loaded from NVS**: Current active configuration
3. **Form Submitted**: User POST to `/api/config`
4. **Valid?**: Dual validation (client + server)
5. **Error**: 400 Bad Request with error message
6. **Save to NVS**: Persist and update global pointer
7. **Display Update**: PowerScreen reads new values next frame

---

### Color Zone Determination

**State Machine** (per power type):

```
Input: power_value (float), thresholds[3] (float[])
Output: color_zone (enum)

           ┌───────────────────────┐
           │   power_value < T[0]  │──YES──► GOOD
           └───────────┬───────────┘
                       │ NO
                       v
           ┌───────────────────────┐
           │   power_value < T[1]  │──YES──► OK
           └───────────┬───────────┘
                       │ NO
                       v
           ┌───────────────────────┐
           │   power_value < T[2]  │──YES──► ATTENTION
           └───────────┬───────────┘
                       │ NO
                       v
                   WARNING
```

**Implementation**:
```cpp
enum ColorZone {
    GOOD,
    OK,
    ATTENTION,
    WARNING
};

ColorZone get_zone(float value, float thresholds[3]) {
    if (value < thresholds[0]) return GOOD;
    if (value < thresholds[1]) return OK;
    if (value < thresholds[2]) return ATTENTION;
    return WARNING;
}

lv_color_t get_color_from_zone(ColorZone zone, const DeviceConfig* config) {
    uint32_t rgb;
    switch (zone) {
        case GOOD:      rgb = config->color_good; break;
        case OK:        rgb = config->color_ok; break;
        case ATTENTION: rgb = config->color_attention; break;
        case WARNING:   rgb = config->color_warning; break;
    }
    return rgb_to_bgr(rgb);  // Convert RGB to BGR for display
}
```

**Special Cases**:
- **Equal Thresholds**: If `T[0] == T[1]`, zone `OK` is skipped (value < T[0] → GOOD, else → ATTENTION)
- **All Equal**: If `T[0] == T[1] == T[2]`, only `GOOD` and `WARNING` zones exist
- **Negative Grid**: Grid can have negative thresholds (e.g., `T[0] = -0.5`), negative values → `GOOD` zone

---

## Relationships

```
┌────────────────────────────┐
│     DeviceConfig (NVS)     │
│  ┌──────────────────────┐  │
│  │ PowerThresholdConfig │  │
│  │  - grid_threshold[3] │  │
│  │  - home_threshold[3] │  │
│  │  - solar_threshold[3]│  │
│  │  - color_good        │  │
│  │  - color_ok          │  │
│  │  - color_attention   │  │
│  │  - color_warning     │  │
│  └──────────┬───────────┘  │
└─────────────┼──────────────┘
              │
              │ global pointer
              │
       ┌──────v────────────────────┐
       │     PowerScreen           │
       │  update() {               │
       │    zone = get_zone(...)   │
       │    color = get_color(...) │
       │  }                        │
       └───────────────────────────┘
              ^
              │ live power data
              │
       ┌──────┴──────────┐
       │  MQTTManager    │
       │  - grid_power   │
       │  - home_power   │
       │  - solar_power  │
       └─────────────────┘
```

**Data Flow**:
1. `WebPortal` receives form submission → validates → updates `DeviceConfig`
2. `DeviceConfig` persists to NVS → updates global pointer
3. `PowerScreen` reads `current_config` pointer every frame
4. `PowerScreen` combines config thresholds + MQTT power values → computes color
5. LVGL renders updated colors to display

---

## Data Integrity

### NVS Migration

**Problem**: Existing configs lack new fields  
**Solution**: Check config size on load, use defaults for missing fields

```cpp
if (loaded_size < sizeof(DeviceConfig)) {
    // Old version - apply defaults for new fields
    config->grid_threshold[0] = 0.0f;
    config->grid_threshold[1] = 0.5f;
    config->grid_threshold[2] = 2.5f;
    config->home_threshold[0] = 0.5f;
    config->home_threshold[1] = 1.0f;
    config->home_threshold[2] = 2.0f;
    config->solar_threshold[0] = 0.5f;
    config->solar_threshold[1] = 1.5f;
    config->solar_threshold[2] = 3.0f;
    config->color_good = 0x00FF00;
    config->color_ok = 0xFFFFFF;
    config->color_attention = 0xFFA500;
    config->color_warning = 0xFF0000;
}
```

### Validation Enforcement

**Dual Validation Strategy**:
- **Client**: JavaScript validates form before submit → instant feedback
- **Server**: C++ validates POST data before NVS write → security (never trust client)

**Consistency**: Both use identical validation logic (ordering, range, color format)

---

## Performance Characteristics

| Operation | Frequency | Latency | Memory |
|-----------|-----------|---------|--------|
| Load from NVS | Once per boot | ~10ms | 52 bytes |
| Save to NVS | On user submit | ~50ms | 52 bytes |
| Color calculation | 3× per frame (60 FPS) | <10µs | 0 bytes (stack only) |
| RGB→BGR conversion | 3× per frame | <5µs | 0 bytes |

**No heap allocations**, all operations use stack or global static memory.

---

## Example Configurations

### 1. Default Configuration

```json
{
  "grid_threshold": [0.0, 0.5, 2.5],
  "home_threshold": [0.5, 1.0, 2.0],
  "solar_threshold": [0.5, 1.5, 3.0],
  "color_good": "0x00FF00",
  "color_ok": "0xFFFFFF",
  "color_attention": "0xFFA500",
  "color_warning": "0xFF0000"
}
```

**Behavior**:
- Grid: Export (negative) → green, 0-0.5 kW → white, 0.5-2.5 kW → orange, >2.5 kW → red
- Home: 0-0.5 kW → green, 0.5-1.0 kW → white, 1.0-2.0 kW → orange, >2.0 kW → red
- Solar: 0-0.5 kW → green, 0.5-1.5 kW → white, 1.5-3.0 kW → orange, >3.0 kW → red

---

### 2. Grid Export Emphasis

```json
{
  "grid_threshold": [-1.0, 0.0, 1.0],
  "home_threshold": [0.5, 1.0, 2.0],
  "solar_threshold": [0.5, 1.5, 3.0],
  "color_good": "0x00FF00",
  "color_ok": "0xFFFFFF",
  "color_attention": "0xFFA500",
  "color_warning": "0xFF0000"
}
```

**Behavior**:
- Grid: Export >1 kW → green, export 0-1 kW → white, import 0-1 kW → orange, import >1 kW → red
- Highlights strong export as "good" (green)

---

### 3. Simplified 2-Level (Skip Intermediate Zones)

```json
{
  "grid_threshold": [0.0, 0.0, 2.0],
  "home_threshold": [0.5, 0.5, 2.0],
  "solar_threshold": [0.5, 0.5, 3.0],
  "color_good": "0x00FF00",
  "color_ok": "0xFFFFFF",
  "color_attention": "0xFFA500",
  "color_warning": "0xFF0000"
}
```

**Behavior**:
- Grid: Export or zero → green, 0-2 kW → orange (skips white), >2 kW → red
- Home: 0-0.5 kW → green, 0.5-2.0 kW → orange (skips white), >2.0 kW → red
- Solar: 0-0.5 kW → green, 0.5-3.0 kW → orange (skips white), >3.0 kW → red

---

### 4. Custom Color Scheme (Blue/Yellow/Pink/Purple)

```json
{
  "grid_threshold": [0.0, 0.5, 2.5],
  "home_threshold": [0.5, 1.0, 2.0],
  "solar_threshold": [0.5, 1.5, 3.0],
  "color_good": "0x0000FF",
  "color_ok": "0xFFFF00",
  "color_attention": "0xFF69B4",
  "color_warning": "0x800080"
}
```

**Behavior**: Same threshold logic as default, but with custom colors (blue/yellow/pink/purple)

---

## Conclusion

The data model provides:
- **Consistency**: Unified 3-threshold, 4-zone structure for all power types
- **Flexibility**: Negative thresholds (grid export), skip zones (equal thresholds)
- **Customization**: User-defined colors (global palette)
- **Integrity**: Dual validation, backward-compatible NVS migration
- **Performance**: Zero heap, <100µs per frame for all 3 color calculations

**Entity count**: 3 (PowerThresholdConfiguration, ColorCodedPowerValue, WebPortalThresholdForm)  
**Validation rules**: 3 (ordering, range, color format)  
**State machines**: 2 (configuration lifecycle, color zone determination)

**Ready for API contract definition**
