# Research: Configurable Power Color Thresholds

**Feature**: 002-power-color-thresholds  
**Phase**: 0 - Research & Technology Decisions  
**Date**: December 13, 2025  
**Revised**: December 13, 2025 (Consistent 4-level structure + configurable colors)

## Overview

This document resolves technical unknowns for making power screen color thresholds and colors configurable. The design provides consistent 4-level color coding (good/ok/attention/warning) across all three power displays (grid/home/solar), with configurable threshold values and color definitions.

## Research Tasks

### 1. Consistent Threshold Structure Design

**Question**: How should threshold configuration be structured to provide consistency across all three power types while maintaining flexibility?

**Decision**: Use identical 3-threshold structure for all power types (grid, home, solar), defining 4 color zones

**Rationale**:
- Consistency improves user mental model (same pattern for all three icons)
- 3 thresholds define 4 zones: value < T0 → good, < T1 → ok, < T2 → attention, else → warning
- Users can "skip" a level by setting adjacent thresholds equal (T0 = T1 skips "ok" zone)
- Grid can use negative thresholds for export scenarios (e.g., T0 = -0.5 kW = strong export)
- Home and solar constrained to non-negative values (consumption/generation cannot be negative)

**Implementation Details**:
```cpp
// In config_manager.h DeviceConfig struct:
float grid_threshold[3];     // T0, T1, T2 (can be negative)
float home_threshold[3];     // T0, T1, T2 (>= 0.0)
float solar_threshold[3];    // T0, T1, T2 (>= 0.0)
```

**Color Determination Logic**:
```cpp
lv_color_t get_color(float value, float thresholds[3], lv_color_t colors[4]) {
    if (value < thresholds[0]) return colors[0];  // Good
    if (value < thresholds[1]) return colors[1];  // OK
    if (value < thresholds[2]) return colors[2];  // Attention
    return colors[3];  // Warning
}
```

---

### 2. Configurable Color Design

**Question**: Should colors be configurable per power type, or globally across all types? How should colors be stored and validated?

**Decision**: 4 global color definitions (good, ok, attention, warning) applied to all power types, stored as RGB hex values

**Rationale**:
- Global colors ensure visual consistency (same "red" means same severity across all displays)
- Simpler mental model for users (configure once, applies everywhere)
- Reduces configuration complexity (4 color fields vs 12 if per-type)
- Storage: 4 × uint32_t (24-bit RGB) = 16 bytes vs strings (saves space)
- Default to current color scheme: Green (#00FF00), White (#FFFFFF), Orange (#FFA500), Red (#FF0000)
- Display hardware uses BGR format, conversion handled in color application (existing pattern)

**Alternatives Considered**:
- Per-icon colors → Rejected: Excessive configuration, inconsistent visual language, confusing UX
- Named colors (string enum) → Rejected: Limited palette, harder to customize, wastes NVS space

**Implementation Details**:
```cpp
// In config_manager.h DeviceConfig struct:
uint32_t color_good;       // RGB hex: 0x00FF00 (green in BGR becomes 0x00FF00)
uint32_t color_ok;         // RGB hex: 0xFFFFFF (white)
uint32_t color_attention;  // RGB hex: 0xFFA500 (orange in BGR becomes 0x00A5FF)
uint32_t color_warning;    // RGB hex: 0xFF0000 (red in BGR becomes 0x0000FF)
```

**Web Portal Input**: HTML color picker (`<input type="color">`) with hex validation

---

### 3. NVS Configuration Extension Strategy

**Question**: How should DeviceConfig be extended for 13 new fields (9 thresholds + 4 colors) while maintaining backward compatibility?

**Decision**: Add 9 float fields (3 arrays) + 4 uint32_t fields to DeviceConfig struct with default fallback

**Rationale**:
- Total storage: 9 floats × 4 bytes + 4 uint32_t × 4 bytes = 52 bytes (acceptable NVS overhead)
- Arrays simplify logic (loop over thresholds vs individual field names)
- Existing pattern: DeviceConfig already uses magic number validation for migration
- Old firmware configs load with size mismatch → new fields initialized to factory defaults
- Default thresholds chosen to match current hardcoded behavior where applicable

**Factory Defaults**:
- Grid thresholds: [0.0, 0.5, 2.5] kW (export boundary, low import, high import)
- Home thresholds: [0.5, 1.0, 2.0] kW (low usage, moderate, high)
- Solar thresholds: [0.5, 1.5, 3.0] kW (minimal generation, moderate, high)
- Colors: Good=0x00FF00, OK=0xFFFFFF, Attention=0xFFA500, Warning=0xFF0000

**Implementation Details**:
```cpp
struct DeviceConfig {
    // ... existing fields ...
    
    // Power thresholds (3 per type, 9 total)
    float grid_threshold[3];    // Default: {0.0, 0.5, 2.5}
    float home_threshold[3];    // Default: {0.5, 1.0, 2.0}
    float solar_threshold[3];   // Default: {0.5, 1.5, 3.0}
    
    // Color configuration (4 global colors, RGB hex)
    uint32_t color_good;        // Default: 0x00FF00
    uint32_t color_ok;          // Default: 0xFFFFFF
    uint32_t color_attention;   // Default: 0xFFA500
    uint32_t color_warning;     // Default: 0xFF0000
    
    uint32_t magic;  // MUST stay last
};
```

---

### 4. Web Portal Integration Pattern

**Question**: How should threshold and color configuration UI be organized on the Home page?

**Decision**: Group by power type (Grid/Home/Solar sections), each with 3 threshold inputs + global color section with 4 color pickers

**Rationale**:
- Existing pattern: Home page shows current power values → logical place for threshold config
- Grouping by power type makes it clear which thresholds affect which display
- Global color section emphasizes that colors apply to all types
- Leverage existing footer save button (no separate save action)
- Standard form inputs: number fields for thresholds (step=0.1), color pickers for colors

**Layout Structure**:
```
Home Page
├── Current Power Values (existing)
├── Power Threshold Configuration (NEW)
│   ├── Grid Power (3 thresholds: T0, T1, T2)
│   ├── Home Power (3 thresholds: T0, T1, T2)
│   └── Solar Power (3 thresholds: T0, T1, T2)
├── Color Configuration (NEW)
│   ├── Good Color (color picker)
│   ├── OK Color (color picker)
│   ├── Attention Color (color picker)
│   └── Warning Color (color picker)
└── Footer Save Button (existing)
```

**API Integration**: Extend `/api/config` JSON with threshold arrays and color hex values

---

### 5. Validation Strategy

**Question**: What validation rules apply to the new consistent structure, and how do we handle negative values and skipped levels?

**Decision**: Dual validation (client + server) with ordering, range, and color format checks

**Client-Side (JavaScript)**:
- Threshold ordering: T[0] <= T[1] <= T[2] (allow equal for skip)
- Grid range: -10.0 to +10.0 kW (allow negative)
- Home/Solar range: 0.0 to +10.0 kW (no negative)
- Color format: Valid 6-digit hex (#RRGGBB)
- Display error alert on failure, preserve input values

**Server-Side (C++)**:
- Same threshold rules as client (security: never trust client)
- Color validation: Ensure 24-bit RGB range (0x000000 to 0xFFFFFF)
- Return 400 Bad Request with specific error message on failure

**Skip Level Detection**: Not validated as error - equal thresholds are intentional feature

**Implementation Details**:
```cpp
bool validate_config(const DeviceConfig* config) {
    // Grid thresholds (can be negative)
    if (config->grid_threshold[0] > config->grid_threshold[1] ||
        config->grid_threshold[1] > config->grid_threshold[2]) return false;
    if (config->grid_threshold[0] < -10.0f || config->grid_threshold[2] > 10.0f) return false;
    
    // Home thresholds (non-negative)
    if (config->home_threshold[0] > config->home_threshold[1] ||
        config->home_threshold[1] > config->home_threshold[2]) return false;
    if (config->home_threshold[0] < 0.0f || config->home_threshold[2] > 10.0f) return false;
    
    // Solar thresholds (non-negative)
    if (config->solar_threshold[0] > config->solar_threshold[1] ||
        config->solar_threshold[1] > config->solar_threshold[2]) return false;
    if (config->solar_threshold[0] < 0.0f || config->solar_threshold[2] > 10.0f) return false;
    
    // Color validation (24-bit RGB)
    if ((config->color_good & 0xFF000000) || (config->color_ok & 0xFF000000) ||
        (config->color_attention & 0xFF000000) || (config->color_warning & 0xFF000000)) return false;
    
    return true;
}
```

---

### 6. Real-Time Display Update Mechanism

**Question**: How does the display update when thresholds or colors change via web portal?

**Decision**: PowerScreen reads thresholds and colors from global DeviceConfig pointer every frame, converts colors from RGB to BGR

**Rationale**:
- Existing pattern: Global `current_config` pointer shared across all subsystems
- Color calculation already occurs every frame in `update()` → adding threshold/color lookups negligible
- BGR conversion needed for display hardware (existing issue documented in screen_power.cpp)
- Config updated in-place by web_portal → display sees changes immediately (<16ms at 60 FPS)

**Color Conversion**:
```cpp
lv_color_t rgb_to_bgr(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    return lv_color_hex((b << 16) | (g << 8) | r);  // Swap R and B
}
```

---

## Technology Stack Confirmation

| Component | Technology | Version | Source |
|-----------|-----------|---------|--------|
| Framework | Arduino ESP32 | 2.0.x | Existing |
| LVGL | LittlevGL | 8.3.0 | arduino-libraries.txt |
| Web Server | ESPAsyncWebServer | 1.2.3 | arduino-libraries.txt |
| JSON | ArduinoJson | 6.21.3 | arduino-libraries.txt |
| Storage | ESP32 NVS | Built-in | ESP32 IDF |
| Color Input | HTML5 color picker | Browser | No library needed |

**No new library dependencies required**

---

## Performance Considerations

### Memory Impact
- NVS storage: 52 bytes (9 floats + 4 uint32_t)
- Heap impact: Zero (config struct is global static)
- Flash impact: ~400 bytes (HTML color pickers, validation logic, conversion functions)

### Latency Budget
- Web portal save → NVS write: ~10-50ms
- NVS write → Display update: <16ms (one frame)
- Total user-perceived latency: <100ms (well under 1s requirement)

### Display Performance
- Color calculation: Adds 3 comparisons + 1 array lookup + 1 RGB→BGR conversion per icon per frame
- Impact: Negligible (~100 CPU cycles, well under frame budget at 240MHz)
- No heap allocations, no SPI contention

---

## Risk Analysis

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Color picker browser compatibility | Low | Medium | HTML5 color input widely supported, fallback to text input |
| RGB↔BGR conversion errors | Low | High | Test on all board targets, validate against known colors |
| Users set all thresholds equal (skip all levels) | Medium | Low | Valid configuration, display shows single color (intentional) |
| Negative threshold on home/solar | Low | Low | Validation prevents, clear error message shown |
| Color values outside 24-bit range | Very Low | Low | Validation masks to 0xFFFFFF, rejects invalid hex |

---

## Open Questions

**None** - All technical unknowns resolved. Consistent 4-level structure with configurable colors is well-defined and implementable using existing infrastructure.

---

## Conclusion

The revised design provides:
- **Consistency**: All 3 power types use identical 3-threshold, 4-color structure
- **Flexibility**: Skip levels via equal thresholds, negative values for grid export
- **Customization**: User-defined colors applied globally
- **Simplicity**: 13 total configuration fields (9 thresholds + 4 colors)
- **Compatibility**: Backward-compatible NVS migration with sensible defaults

**Storage**: 52 bytes NVS  
**Dependencies**: None (uses existing libraries)  
**Performance**: Zero measurable impact on display FPS

**Ready to proceed to Phase 1: Design & Contracts**
