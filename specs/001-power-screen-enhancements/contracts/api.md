# Power Screen API Contracts

**Feature**: Power Screen Enhancements  
**Branch**: `001-power-screen-enhancements`  
**Date**: 2025-12-13

## Overview

This document defines the function signatures, LVGL API usage patterns, and behavioral contracts for implementing power screen enhancements. All functions follow embedded systems best practices with no dynamic allocation in update paths.

---

## Color Calculation Functions

### `get_grid_color()`

```cpp
/**
 * @brief Calculate LVGL color for grid power based on threshold ranges
 * 
 * @param kw Grid power in kilowatts (negative = exporting to grid)
 * @return lv_color_t Color constant for current power state
 * 
 * Thresholds:
 * - NaN → WHITE (no data)
 * - < 0 → BRIGHT_GREEN (exporting)
 * - >= 0 and < 1kW → WHITE (low import)
 * - >= 1kW and < 2.5kW → ORANGE (medium import)
 * - >= 2.5kW → RED (high import)
 * 
 * Performance: O(1), no allocation, called at 1Hz max
 */
lv_color_t PowerScreen::get_grid_color(float kw);
```

**Preconditions**:
- None (handles NaN, negative, and all positive values)

**Postconditions**:
- Returns one of: COLOR_BRIGHT_GREEN, COLOR_WHITE, COLOR_ORANGE, COLOR_RED
- Never returns invalid color

**Example Usage**:
```cpp
float grid_kw = -0.5f;  // Exporting to grid
lv_color_t color = get_grid_color(grid_kw);  // Returns COLOR_BRIGHT_GREEN
lv_obj_set_style_text_color(grid_icon, color, LV_PART_MAIN);
```

---

### `get_home_color()`

```cpp
/**
 * @brief Calculate LVGL color for home power based on consumption thresholds
 * 
 * @param kw Home power consumption in kilowatts (always >= 0)
 * @return lv_color_t Color constant for current consumption level
 * 
 * Thresholds:
 * - NaN → WHITE (no data)
 * - < 0.5kW → BRIGHT_GREEN (very low)
 * - >= 0.5kW and < 1kW → WHITE (low)
 * - >= 1kW and < 2kW → ORANGE (medium)
 * - >= 2kW → RED (high)
 * 
 * Performance: O(1), no allocation, called at 1Hz max
 */
lv_color_t PowerScreen::get_home_color(float kw);
```

**Preconditions**:
- `kw >= 0` (home power calculated as grid + solar, never negative)
- Accepts NaN for "no data" state

**Postconditions**:
- Returns one of: COLOR_BRIGHT_GREEN, COLOR_WHITE, COLOR_ORANGE, COLOR_RED
- Negative values treated as 0 (defensive programming)

**Example Usage**:
```cpp
float home_kw = 1.8f;  // 1.8kW home consumption
lv_color_t color = get_home_color(home_kw);  // Returns COLOR_ORANGE
lv_obj_set_style_text_color(home_value, color, LV_PART_MAIN);
```

---

### `get_solar_color()`

```cpp
/**
 * @brief Calculate LVGL color for solar power based on generation threshold
 * 
 * @param kw Solar power generation in kilowatts (>= 0)
 * @return lv_color_t Color constant for current generation state
 * 
 * Thresholds:
 * - NaN → WHITE (no data)
 * - < 0.5kW → WHITE (low/no generation)
 * - >= 0.5kW → BRIGHT_GREEN (active generation)
 * 
 * Performance: O(1), no allocation, called at 1Hz max
 */
lv_color_t PowerScreen::get_solar_color(float kw);
```

**Preconditions**:
- `kw >= 0` (solar generation never negative)
- Accepts NaN for "no data" state

**Postconditions**:
- Returns either COLOR_WHITE or COLOR_BRIGHT_GREEN
- Negative values treated as 0 (defensive programming)

**Example Usage**:
```cpp
float solar_kw = 2.3f;  // 2.3kW solar generation
lv_color_t color = get_solar_color(solar_kw);  // Returns COLOR_BRIGHT_GREEN
lv_obj_set_style_text_color(solar_icon, color, LV_PART_MAIN);
```

---

## Widget Update Functions

### `update_power_colors()`

```cpp
/**
 * @brief Update all icon/value/label colors based on current power values
 * 
 * Applies color styling to 9 LVGL objects (3 icons, 3 values, 3 labels)
 * based on current solar_kw, grid_kw, and calculated home power.
 * 
 * Called from: set_solar_power(), set_grid_power(), update_home_value()
 * 
 * Performance: 9× lv_obj_set_style_text_color() calls, no allocation
 * Timing: <1ms on ESP32 @ 240MHz
 * 
 * @pre Widgets must be created (solar_icon, home_icon, grid_icon, etc.)
 * @post All widget text colors updated to match current power states
 */
void PowerScreen::update_power_colors();
```

**Implementation Pattern**:
```cpp
void PowerScreen::update_power_colors() {
    // Calculate colors based on current values
    lv_color_t solar_color = get_solar_color(solar_kw);
    lv_color_t grid_color = get_grid_color(grid_kw);
    
    float home_kw = NAN;
    if (!isnan(solar_kw) && !isnan(grid_kw)) {
        home_kw = solar_kw + grid_kw;
    }
    lv_color_t home_color = get_home_color(home_kw);
    
    // Apply colors to all widgets (null checks for safety)
    if (solar_icon) lv_obj_set_style_text_color(solar_icon, solar_color, LV_PART_MAIN);
    if (solar_value) lv_obj_set_style_text_color(solar_value, solar_color, LV_PART_MAIN);
    if (solar_unit) lv_obj_set_style_text_color(solar_unit, solar_color, LV_PART_MAIN);
    
    // ... repeat for home and grid widgets
}
```

---

### `update_power_bars()`

```cpp
/**
 * @brief Update all bar chart values based on current power measurements
 * 
 * Converts power values (kW) to bar range (0-3000mW) and updates LVGL
 * bar widgets. Uses absolute values for negative grid power (exports).
 * 
 * Called from: set_solar_power(), set_grid_power(), update_home_value()
 * 
 * Performance: 3× lv_bar_set_value() calls, no allocation
 * Timing: <0.5ms on ESP32 @ 240MHz
 * 
 * @pre Bar widgets must be created and range configured (0-3000)
 * @post All bar values updated to reflect 0-3kW power scale
 */
void PowerScreen::update_power_bars();
```

**Implementation Pattern**:
```cpp
void PowerScreen::update_power_bars() {
    // Helper to convert kW to bar value (0-3000mW)
    auto kw_to_bar = [](float kw) -> int16_t {
        if (isnan(kw)) return 0;
        float abs_kw = fabsf(kw);  // Absolute value for negative grid
        if (abs_kw > 3.0f) abs_kw = 3.0f;  // Clamp to max
        return (int16_t)(abs_kw * 1000.0f);
    };
    
    if (solar_bar) {
        lv_bar_set_value(solar_bar, kw_to_bar(solar_kw), LV_ANIM_OFF);
    }
    if (grid_bar) {
        lv_bar_set_value(grid_bar, kw_to_bar(grid_kw), LV_ANIM_OFF);
    }
    
    float home_kw = NAN;
    if (!isnan(solar_kw) && !isnan(grid_kw)) {
        home_kw = solar_kw + grid_kw;
    }
    if (home_bar) {
        lv_bar_set_value(home_bar, kw_to_bar(home_kw), LV_ANIM_OFF);
    }
}
```

---

## Widget Creation Functions

### Bar Widget Creation Pattern

```cpp
/**
 * @brief Create and configure a vertical power bar widget
 * 
 * @param parent Parent container (background object)
 * @param x_offset Horizontal offset from center (-107, 0, or +107)
 * @return lv_obj_t* Pointer to created bar widget
 * 
 * Configures:
 * - Size: 12px × 120px (vertical)
 * - Position: Y=120, aligned to column
 * - Range: 0-3000 (0-3kW in milliwatts)
 * - Initial value: 0
 * - Animation: Disabled (LV_ANIM_OFF)
 * - Background: Black (#000000)
 * - Indicator: White (#FFFFFF) by default
 * 
 * Performance: Single allocation via LVGL, stored in PowerScreen members
 */
lv_obj_t* create_power_bar(lv_obj_t* parent, lv_coord_t x_offset);
```

**Example Usage** (in `PowerScreen::create()`):
```cpp
// Create vertical bar widgets below labels
solar_bar = lv_bar_create(background);
lv_obj_set_size(solar_bar, 12, 120);  // 12px wide, 120px tall
lv_bar_set_mode(solar_bar, LV_BAR_MODE_NORMAL);  // Fills from bottom
lv_obj_align(solar_bar, LV_ALIGN_TOP_MID, -107, 120);  // Below label
lv_bar_set_range(solar_bar, 0, 3000);  // 0-3kW in milliwatts
lv_bar_set_value(solar_bar, 0, LV_ANIM_OFF);

// Style: black background, white indicator
lv_obj_set_style_bg_color(solar_bar, lv_color_hex(0x000000), LV_PART_MAIN);
lv_obj_set_style_bg_color(solar_bar, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);

// Repeat for home_bar (x=0) and grid_bar (x=+107)
```

---

### Icon Widget Creation Pattern

```cpp
/**
 * @brief Create and configure an image widget for power icons
 * 
 * @param parent Parent container (background object)
 * @param icon_data Pointer to lv_img_dsc_t icon descriptor
 * @param x_offset Horizontal offset from center (-107, 0, or +107)
 * @return lv_obj_t* Pointer to created image widget
 * 
 * Configures:
 * - Source: PNG icon converted to lv_img_dsc_t
 * - Position: Y=15, aligned to column
 * - Recolor: Enabled for dynamic color changes
 * - Initial color: White (#FFFFFF)
 * 
 * Performance: Single allocation via LVGL, icon data in PROGMEM
 */
lv_obj_t* create_power_icon(lv_obj_t* parent, const lv_img_dsc_t* icon_data, lv_coord_t x_offset);
```

**Example Usage** (in `PowerScreen::create()` - replaces current label-based icons):
```cpp
// Replace: solar_icon = lv_label_create(background);
//          lv_label_set_text(solar_icon, LV_SYMBOL_REFRESH);

// New: solar_icon = lv_img_create(background);
solar_icon = lv_img_create(background);
lv_img_set_src(solar_icon, &icon_solar);  // From icon_assets.h
lv_obj_align(solar_icon, LV_ALIGN_TOP_MID, -107, 15);

// Enable recoloring for dynamic color changes
lv_obj_set_style_img_recolor_opa(solar_icon, 255, LV_PART_MAIN);  // Full recolor
lv_obj_set_style_img_recolor(solar_icon, lv_color_hex(0xFFFFFF), LV_PART_MAIN);  // Initial white

// Repeat for home_icon (&icon_home) and grid_icon (&icon_grid)
```

---

## LVGL API Reference

### Color Setting APIs

```cpp
// For text widgets (labels, existing approach)
void lv_obj_set_style_text_color(lv_obj_t* obj, lv_color_t color, lv_style_selector_t selector);
// selector = LV_PART_MAIN | LV_STATE_DEFAULT (or just 0)

// For image widgets (new icons)
void lv_obj_set_style_img_recolor(lv_obj_t* obj, lv_color_t color, lv_style_selector_t selector);
void lv_obj_set_style_img_recolor_opa(lv_obj_t* obj, lv_opa_t opa, lv_style_selector_t selector);
// opa = 255 for full recolor (replaces all colors), 0 = no recolor
```

### Bar Widget APIs

```cpp
// Create bar widget
lv_obj_t* lv_bar_create(lv_obj_t* parent);

// Configure value range
void lv_bar_set_range(lv_obj_t* bar, int32_t min, int32_t max);
// For 0-3kW: lv_bar_set_range(bar, 0, 3000);  // milliwatts

// Set current value
void lv_bar_set_value(lv_obj_t* bar, int32_t value, lv_anim_enable_t anim);
// For instant updates: lv_bar_set_value(bar, 1500, LV_ANIM_OFF);

// Style bar colors
lv_obj_set_style_bg_color(bar, color, LV_PART_MAIN);  // Background (empty part)
lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);  // Filled part
```

### Image Widget APIs

```cpp
// Create image widget
lv_obj_t* lv_img_create(lv_obj_t* parent);

// Set image source (from lv_img_dsc_t)
void lv_img_set_src(lv_obj_t* img, const void* src);
// src can be: lv_img_dsc_t pointer, file path, or symbol

// Enable color recoloring
void lv_obj_set_style_img_recolor_opa(lv_obj_t* img, lv_opa_t opa, lv_style_selector_t selector);
void lv_obj_set_style_img_recolor(lv_obj_t* img, lv_color_t color, lv_style_selector_t selector);
```

---

## Modified Existing Functions

### `PowerScreen::create()`

**New Responsibilities**:
- Replace `lv_label_create()` with `lv_img_create()` for icons
- Add 3 bar widget creations after label row
- Configure bar ranges and initial values
- Set up recoloring for icons

**Performance Impact**: +3 bar widgets (~600 bytes heap), +3 icon images (~14KB flash)

**Critical Change**: Icons are now `lv_img` objects, not `lv_label` objects
- Remove: `lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0)`
- Add: `lv_obj_set_style_img_recolor_opa(icon, 255, LV_PART_MAIN)`

---

### `PowerScreen::set_solar_power()`

**New Responsibilities**:
- After updating text value, call `update_power_colors()`
- After color update, call `update_power_bars()`

**Example**:
```cpp
void PowerScreen::set_solar_power(float kw) {
    solar_kw = kw;
    
    // Existing: Update text value
    if (solar_value) {
        if (isnan(kw)) {
            lv_label_set_text(solar_value, "--");
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", kw);
            lv_label_set_text(solar_value, buf);
        }
    }
    
    // NEW: Update colors and bars
    update_power_colors();
    update_power_bars();
    
    // Existing: Update home value
    update_home_value();
}
```

**Performance**: +2 function calls per MQTT update (1Hz), negligible overhead

---

### `PowerScreen::destroy()`

**New Responsibilities**:
- Null out bar widget pointers after deletion

**Example**:
```cpp
void PowerScreen::destroy() {
    if (screen_obj) {
        lv_obj_del(screen_obj);  // Deletes all children including bars
        screen_obj = nullptr;
        background = nullptr;
        solar_icon = home_icon = grid_icon = nullptr;
        arrow1 = arrow2 = nullptr;
        solar_value = home_value = grid_value = nullptr;
        solar_unit = home_unit = grid_unit = nullptr;
        
        // NEW: Null bar pointers
        solar_bar = home_bar = grid_bar = nullptr;
    }
    visible = false;
}
```

---

## Build-Time Contracts

### Icon Conversion Script: `tools/convert-icons.sh`

**Input**:
- `assets/icons/solar.png` (48×48 RGB/RGBA PNG)
- `assets/icons/home.png` (48×48 RGB/RGBA PNG)
- `assets/icons/grid.png` (48×48 RGB/RGBA PNG)

**Output**:
- `src/app/icon_assets.h` (header with extern declarations)
- `src/app/icon_assets.c` (C arrays with lv_img_dsc_t structures)

**Validation**:
```bash
#!/bin/bash
# Check PNG exists and is 48x48
for icon in solar home grid; do
    PNG="assets/icons/${icon}.png"
    if [ ! -f "$PNG" ]; then
        echo "ERROR: $PNG not found"
        exit 1
    fi
    
    # Validate dimensions (requires ImageMagick)
    SIZE=$(identify -format "%w %h" "$PNG" 2>/dev/null)
    if [ "$SIZE" != "48 48" ]; then
        echo "ERROR: $PNG is $SIZE, expected 48 48"
        exit 1
    fi
done

# Convert using lv_img_conv CLI
lv_img_conv assets/icons/solar.png -f true_color_alpha -o src/app/icon_solar.c
# ... repeat for home and grid
# ... generate icon_assets.h header

exit 0
```

**Contract**:
- Exit 0 on success → build continues
- Exit 1 on failure → build stops with clear error message
- Generated files must compile without warnings

---

## API Contract Summary

| Function | Input | Output | Side Effects |
|----------|-------|--------|--------------|
| `get_grid_color()` | float kw | lv_color_t | None (pure function) |
| `get_home_color()` | float kw | lv_color_t | None (pure function) |
| `get_solar_color()` | float kw | lv_color_t | None (pure function) |
| `update_power_colors()` | None (uses state) | void | Sets style on 9 widgets |
| `update_power_bars()` | None (uses state) | void | Sets value on 3 bars |
| `create_power_bar()` | parent, x_offset | lv_obj_t* | Allocates widget via LVGL |
| `create_power_icon()` | parent, icon, x | lv_obj_t* | Allocates widget via LVGL |

---

## Performance Contracts

- **Color updates**: <1ms per call (9 style sets)
- **Bar updates**: <0.5ms per call (3 value sets)
- **Total overhead**: <2ms per MQTT update (1Hz)
- **FPS impact**: None (updates during LVGL idle, not in render path)
- **Memory allocation**: Zero in update paths (all allocations in `create()`)

---

## Contracts Complete

All function signatures, LVGL APIs, and behavioral contracts defined. Ready for Phase 1C (implementation quickstart guide).
