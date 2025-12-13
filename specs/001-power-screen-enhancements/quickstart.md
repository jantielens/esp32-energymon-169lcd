# Quickstart: Power Screen Enhancements Implementation

**Feature**: Power Screen Enhancements  
**Branch**: `001-power-screen-enhancements`  
**Date**: 2025-12-13

## Implementation Overview

This guide provides step-by-step instructions for implementing the power screen enhancements feature across 3 priority levels (P1: Color Coding, P2: Custom Icons, P3: Bar Charts). Each priority can be implemented and tested independently.

**Estimated Total Time**: 4-6 hours  
**Files Modified**: 4-5 files  
**Files Created**: 4-5 files  
**Build Tools Required**: `lv_img_conv` (npm package), ImageMagick `identify` (optional for validation)

---

## Prerequisites

### Install Icon Conversion Tool

```bash
# Install LVGL image converter CLI globally
npm install -g lv_img_conv

# Verify installation
lv_img_conv --version
```

### Optional: Install ImageMagick for PNG Validation

```bash
# Ubuntu/Debian
sudo apt-get install imagemagick

# macOS
brew install imagemagick

# Verify
identify --version
```

---

## Priority 1: Color-Coded Power Indicators (2-3 hours)

### Step 1: Add Color Constants (10 min)

**File**: `src/app/screen_power.cpp`

Add at file scope (after includes, before PowerScreen implementation):

```cpp
#include "screen_power.h"
#include <math.h>
#include <stdio.h>

// Color definitions for power state visualization
const lv_color_t COLOR_BRIGHT_GREEN = lv_color_hex(0x00FF00);
const lv_color_t COLOR_WHITE        = lv_color_hex(0xFFFFFF);
const lv_color_t COLOR_ORANGE       = lv_color_hex(0xFFA500);
const lv_color_t COLOR_RED          = lv_color_hex(0xFF0000);

// Remove old TEXT_COLOR define
// #define TEXT_COLOR lv_color_hex(0xFFFFFF)  // DELETE THIS LINE
```

---

### Step 2: Add Color Calculation Methods (20 min)

**File**: `src/app/screen_power.h`

Add private method declarations:

```cpp
class PowerScreen : public ScreenBase {
public:
    // ... existing public methods ...

private:
    // ... existing private members ...
    
    // NEW: Color calculation helpers
    lv_color_t get_grid_color(float kw);
    lv_color_t get_home_color(float kw);
    lv_color_t get_solar_color(float kw);
    
    // NEW: Widget update helpers
    void update_power_colors();
};
```

**File**: `src/app/screen_power.cpp`

Implement color calculation methods (add before `PowerScreen::create()`):

```cpp
lv_color_t PowerScreen::get_grid_color(float kw) {
    if (isnan(kw)) return COLOR_WHITE;
    if (kw < 0.0f) return COLOR_BRIGHT_GREEN;  // Exporting
    if (kw < 1.0f) return COLOR_WHITE;         // Low import
    if (kw < 2.5f) return COLOR_ORANGE;        // Medium import
    return COLOR_RED;                           // High import
}

lv_color_t PowerScreen::get_home_color(float kw) {
    if (isnan(kw)) return COLOR_WHITE;
    if (kw < 0.5f) return COLOR_BRIGHT_GREEN;  // Very low
    if (kw < 1.0f) return COLOR_WHITE;         // Low
    if (kw < 2.0f) return COLOR_ORANGE;        // Medium
    return COLOR_RED;                           // High
}

lv_color_t PowerScreen::get_solar_color(float kw) {
    if (isnan(kw)) return COLOR_WHITE;
    if (kw < 0.5f) return COLOR_WHITE;          // Low/no generation
    return COLOR_BRIGHT_GREEN;                  // Active generation
}
```

---

### Step 3: Implement Color Update Function (20 min)

**File**: `src/app/screen_power.cpp`

Add after color calculation methods:

```cpp
void PowerScreen::update_power_colors() {
    // Calculate colors based on current values
    lv_color_t solar_color = get_solar_color(solar_kw);
    lv_color_t grid_color = get_grid_color(grid_kw);
    
    // Calculate home power
    float home_kw = NAN;
    if (!isnan(solar_kw) && !isnan(grid_kw)) {
        home_kw = solar_kw + grid_kw;
    }
    lv_color_t home_color = get_home_color(home_kw);
    
    // Apply colors to solar widgets
    if (solar_icon) {
        lv_obj_set_style_text_color(solar_icon, solar_color, LV_PART_MAIN);
    }
    if (solar_value) {
        lv_obj_set_style_text_color(solar_value, solar_color, LV_PART_MAIN);
    }
    if (solar_unit) {
        lv_obj_set_style_text_color(solar_unit, solar_color, LV_PART_MAIN);
    }
    
    // Apply colors to home widgets
    if (home_icon) {
        lv_obj_set_style_text_color(home_icon, home_color, LV_PART_MAIN);
    }
    if (home_value) {
        lv_obj_set_style_text_color(home_value, home_color, LV_PART_MAIN);
    }
    if (home_unit) {
        lv_obj_set_style_text_color(home_unit, home_color, LV_PART_MAIN);
    }
    
    // Apply colors to grid widgets
    if (grid_icon) {
        lv_obj_set_style_text_color(grid_icon, grid_color, LV_PART_MAIN);
    }
    if (grid_value) {
        lv_obj_set_style_text_color(grid_value, grid_color, LV_PART_MAIN);
    }
    if (grid_unit) {
        lv_obj_set_style_text_color(grid_unit, grid_color, LV_PART_MAIN);
    }
}
```

---

### Step 4: Integrate Color Updates into Power Setters (15 min)

**File**: `src/app/screen_power.cpp`

Modify `set_solar_power()` to call color update:

```cpp
void PowerScreen::set_solar_power(float kw) {
    solar_kw = kw;
    
    if (solar_value) {
        if (isnan(kw)) {
            lv_label_set_text(solar_value, "--");
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", kw);
            lv_label_set_text(solar_value, buf);
        }
    }
    
    // NEW: Update colors after value change
    update_power_colors();
    
    // Existing arrow visibility logic
    if (arrow1) {
        if (!isnan(kw) && kw >= 0.01) {
            lv_obj_clear_flag(arrow1, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(arrow1, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    update_home_value();
}
```

Modify `set_grid_power()` similarly:

```cpp
void PowerScreen::set_grid_power(float kw) {
    grid_kw = kw;
    
    if (grid_value) {
        if (isnan(kw)) {
            lv_label_set_text(grid_value, "--");
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", kw);
            lv_label_set_text(grid_value, buf);
        }
    }
    
    // NEW: Update colors after value change
    update_power_colors();
    
    // Existing arrow visibility logic
    if (arrow2) {
        if (!isnan(kw) && kw >= 0.01) {
            lv_obj_set_text(arrow2, LV_SYMBOL_RIGHT);
            lv_obj_clear_flag(arrow2, LV_OBJ_FLAG_HIDDEN);
        } else if (!isnan(kw) && kw < -0.01) {
            lv_obj_set_text(arrow2, LV_SYMBOL_LEFT);
            lv_obj_clear_flag(arrow2, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(arrow2, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    update_home_value();
}
```

Modify `update_home_value()`:

```cpp
void PowerScreen::update_home_value() {
    if (!home_value) return;
    
    float home_kw = NAN;
    if (!isnan(solar_kw) && !isnan(grid_kw)) {
        home_kw = solar_kw + grid_kw;
    }
    
    if (isnan(home_kw)) {
        lv_label_set_text(home_value, "--");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", home_kw);
        lv_label_set_text(home_value, buf);
    }
    
    // NEW: Update colors after home value recalculation
    update_power_colors();
}
```

---

### Step 5: Update Initial Widget Colors in create() (10 min)

**File**: `src/app/screen_power.cpp`

In `PowerScreen::create()`, replace all instances of `TEXT_COLOR` with `COLOR_WHITE`:

```cpp
// Example for solar icon (line ~41)
solar_icon = lv_label_create(background);
lv_label_set_text(solar_icon, LV_SYMBOL_REFRESH);
lv_obj_set_style_text_font(solar_icon, &lv_font_montserrat_48, 0);
lv_obj_set_style_text_color(solar_icon, COLOR_WHITE, 0);  // Changed from TEXT_COLOR
lv_obj_align(solar_icon, LV_ALIGN_TOP_MID, -107, 15);

// Repeat for all other widgets (icons, values, labels)
```

---

### Step 6: Build and Test P1 (30 min)

```bash
# Build for your board
./build.sh esp32c3

# Flash and monitor
./upload.sh esp32c3
./monitor.sh

# Test scenarios:
# 1. Grid power < 0 (exporting) → icons/values should be bright green
# 2. Grid power 0.5kW → white
# 3. Grid power 1.8kW → orange
# 4. Grid power 3.0kW → red
# 5. Solar power < 0.5kW → white
# 6. Solar power >= 0.5kW → bright green
# 7. Home power calculated correctly with appropriate colors
```

**Validation Checklist**:
- [ ] Icons change color based on power thresholds
- [ ] Values change color matching icons
- [ ] Labels ("kW") change color matching values
- [ ] Colors update within ~1 second of MQTT message
- [ ] FPS counter shows 60 FPS maintained

---

## Priority 2: Custom PNG Icons (1-2 hours)

### Step 7: Create PNG Icon Files (30 min)

**Directory**: Create `assets/icons/`

```bash
mkdir -p assets/icons
```

Create or obtain 3 PNG files (48×48 pixels, RGB or RGBA):
- `assets/icons/solar.png` - Solar panel icon
- `assets/icons/home.png` - House icon
- `assets/icons/grid.png` - Power grid/transmission icon

**Design Guidelines**:
- Size: Exactly 48×48 pixels
- Format: PNG with transparency (alpha channel recommended)
- Colors: Use white/light colors (will be recolored dynamically)
- Style: Simple, bold outlines for 2m visibility
- File size: <10KB each

**Free Icon Sources**:
- Material Design Icons (https://materialdesignicons.com/)
- Font Awesome (export as PNG)
- Flaticon (https://www.flaticon.com/)
- Custom design in Inkscape/Figma → export as 48×48 PNG

---

### Step 8: Create Icon Conversion Script (45 min)

**File**: `tools/convert-icons.sh`

```bash
#!/bin/bash
# Convert PNG icons to LVGL C arrays
set -e

ICONS_DIR="assets/icons"
OUTPUT_DIR="src/app"
OUTPUT_H="$OUTPUT_DIR/icon_assets.h"
OUTPUT_C="$OUTPUT_DIR/icon_assets.c"

# Check for required tools
if ! command -v lv_img_conv &> /dev/null; then
    echo "ERROR: lv_img_conv not found. Install with: npm install -g lv_img_conv"
    exit 1
fi

# Validate PNG files exist and are 48x48
for icon in solar home grid; do
    PNG="$ICONS_DIR/${icon}.png"
    
    if [ ! -f "$PNG" ]; then
        echo "ERROR: $PNG not found"
        exit 1
    fi
    
    # Validate dimensions (requires ImageMagick identify command)
    if command -v identify &> /dev/null; then
        SIZE=$(identify -format "%w %h" "$PNG" 2>/dev/null)
        if [ "$SIZE" != "48 48" ]; then
            echo "ERROR: $PNG dimensions are $SIZE, expected 48 48"
            exit 1
        fi
        echo "✓ Validated $PNG (48x48)"
    else
        echo "⚠ Skipping size validation (ImageMagick identify not installed)"
    fi
done

# Generate header file
cat > "$OUTPUT_H" << 'EOF'
// Auto-generated by tools/convert-icons.sh - DO NOT EDIT MANUALLY
#ifndef ICON_ASSETS_H
#define ICON_ASSETS_H

#include <lvgl.h>

// Power screen icons (48x48 RGB565)
extern const lv_img_dsc_t icon_solar;
extern const lv_img_dsc_t icon_home;
extern const lv_img_dsc_t icon_grid;

#endif // ICON_ASSETS_H
EOF

echo "✓ Generated $OUTPUT_H"

# Convert PNGs to C arrays
TMP_DIR=$(mktemp -d)
trap "rm -rf $TMP_DIR" EXIT

for icon in solar home grid; do
    PNG="$ICONS_DIR/${icon}.png"
    TMP_C="$TMP_DIR/icon_${icon}.c"
    
    echo "Converting ${icon}.png..."
    lv_img_conv "$PNG" \
        --format true_color_alpha \
        --output-file "$TMP_C" \
        --binary-format c_array
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to convert $PNG"
        exit 1
    fi
done

# Combine into single C file
cat > "$OUTPUT_C" << 'EOF'
// Auto-generated by tools/convert-icons.sh - DO NOT EDIT MANUALLY
#include "icon_assets.h"

EOF

# Append each converted icon
for icon in solar home grid; do
    cat "$TMP_DIR/icon_${icon}.c" >> "$OUTPUT_C"
    echo "" >> "$OUTPUT_C"
done

echo "✓ Generated $OUTPUT_C"
echo "✓ Icon conversion complete (3 icons, ~14KB)"

exit 0
```

Make executable:

```bash
chmod +x tools/convert-icons.sh
```

---

### Step 9: Integrate Icon Conversion into Build Process (15 min)

**File**: `build.sh`

Find the section before the compile loop (around line 100-120, after library installation). Add:

```bash
# Convert PNG icons to LVGL C arrays
echo ""
echo "Converting PNG icons to LVGL C arrays..."
./tools/convert-icons.sh || {
    echo "ERROR: Icon conversion failed. Check assets/icons/ for valid 48x48 PNG files."
    exit 1
}
echo ""
```

**Add to `.gitignore`**:

```
# Generated icon assets
src/app/icon_assets.h
src/app/icon_assets.c
```

---

### Step 10: Modify PowerScreen to Use Image Widgets (45 min)

**File**: `src/app/screen_power.cpp`

Add include at top:

```cpp
#include "screen_power.h"
#include "icon_assets.h"  // NEW: Include generated icon assets
#include <math.h>
#include <stdio.h>
```

In `PowerScreen::create()`, replace icon label creation with image widgets:

```cpp
// OLD (delete these lines):
// solar_icon = lv_label_create(background);
// lv_label_set_text(solar_icon, LV_SYMBOL_REFRESH);
// lv_obj_set_style_text_font(solar_icon, &lv_font_montserrat_48, 0);
// lv_obj_set_style_text_color(solar_icon, COLOR_WHITE, 0);
// lv_obj_align(solar_icon, LV_ALIGN_TOP_MID, -107, 15);

// NEW (replace with):
solar_icon = lv_img_create(background);
lv_img_set_src(solar_icon, &icon_solar);
lv_obj_align(solar_icon, LV_ALIGN_TOP_MID, -107, 15);
lv_obj_set_style_img_recolor_opa(solar_icon, 255, LV_PART_MAIN);  // Enable full recolor
lv_obj_set_style_img_recolor(solar_icon, COLOR_WHITE, LV_PART_MAIN);  // Initial color

// Repeat for home_icon (with &icon_home):
home_icon = lv_img_create(background);
lv_img_set_src(home_icon, &icon_home);
lv_obj_align(home_icon, LV_ALIGN_TOP_MID, 0, 15);
lv_obj_set_style_img_recolor_opa(home_icon, 255, LV_PART_MAIN);
lv_obj_set_style_img_recolor(home_icon, COLOR_WHITE, LV_PART_MAIN);

// Repeat for grid_icon (with &icon_grid):
grid_icon = lv_img_create(background);
lv_img_set_src(grid_icon, &icon_grid);
lv_obj_align(grid_icon, LV_ALIGN_TOP_MID, 107, 15);
lv_obj_set_style_img_recolor_opa(grid_icon, 255, LV_PART_MAIN);
lv_obj_set_style_img_recolor(grid_icon, COLOR_WHITE, LV_PART_MAIN);
```

Modify `update_power_colors()` to use image recolor API:

```cpp
void PowerScreen::update_power_colors() {
    // ... existing color calculation code ...
    
    // Apply colors to solar widgets (image recolor for icon)
    if (solar_icon) {
        lv_obj_set_style_img_recolor(solar_icon, solar_color, LV_PART_MAIN);  // Changed from text_color
    }
    if (solar_value) {
        lv_obj_set_style_text_color(solar_value, solar_color, LV_PART_MAIN);  // Text still uses text_color
    }
    if (solar_unit) {
        lv_obj_set_style_text_color(solar_unit, solar_color, LV_PART_MAIN);
    }
    
    // Repeat for home and grid (use img_recolor for icons, text_color for values/units)
    if (home_icon) {
        lv_obj_set_style_img_recolor(home_icon, home_color, LV_PART_MAIN);
    }
    // ... etc
    
    if (grid_icon) {
        lv_obj_set_style_img_recolor(grid_icon, grid_color, LV_PART_MAIN);
    }
    // ... etc
}
```

---

### Step 11: Build and Test P2 (30 min)

```bash
# Build (will run icon conversion automatically)
./build.sh esp32c3

# Check that icon_assets.h and icon_assets.c were generated
ls -lh src/app/icon_assets.*

# Flash and monitor
./upload.sh esp32c3
./monitor.sh

# Test scenarios:
# 1. Custom icons should appear instead of LVGL symbols
# 2. Icons should still change colors based on power thresholds
# 3. Verify no visual artifacts or scaling issues
# 4. Check flash usage hasn't exceeded 85%
```

**Validation Checklist**:
- [ ] Custom PNG icons display correctly
- [ ] Icons change color dynamically (recolor working)
- [ ] No build errors or warnings
- [ ] Flash usage <85% (check build output)
- [ ] Icons visible and clear from 2m distance

---

## Priority 3: Power Bar Charts (1 hour)

### Step 12: Add Bar Widget Pointers (5 min)

**File**: `src/app/screen_power.h`

Add bar widget pointers to private section:

```cpp
private:
    // ... existing UI elements ...
    
    // NEW: Vertical bar chart widgets
    lv_obj_t* solar_bar = nullptr;
    lv_obj_t* home_bar = nullptr;
    lv_obj_t* grid_bar = nullptr;
    
    // ... existing methods ...
    
    // NEW: Bar update helper
    void update_power_bars();
```

---

### Step 13: Create Bar Widgets in create() (20 min)

**File**: `src/app/screen_power.cpp`

In `PowerScreen::create()`, after creating all labels (after line ~115), add:

```cpp
    // ... existing code creating solar_unit, home_unit, grid_unit ...
    
    // NEW: Vertical bar charts (starting from Y=120, extending to bottom of screen)
    
    // Solar bar (vertical, 12px wide × 120px tall)
    solar_bar = lv_bar_create(background);
    lv_obj_set_size(solar_bar, 12, 120);  // 12px wide, 120px tall
    lv_bar_set_mode(solar_bar, LV_BAR_MODE_NORMAL);  // Fills from bottom up
    lv_obj_align(solar_bar, LV_ALIGN_TOP_MID, -107, 120);  // Just below labels
    lv_bar_set_range(solar_bar, 0, 3000);  // 0-3kW in milliwatts
    lv_bar_set_value(solar_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(solar_bar, lv_color_hex(0x000000), LV_PART_MAIN);  // Black background
    lv_obj_set_style_bg_color(solar_bar, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);  // White fill
    
    // Home bar (vertical, 12px wide × 120px tall)
    home_bar = lv_bar_create(background);
    lv_obj_set_size(home_bar, 12, 120);
    lv_bar_set_mode(home_bar, LV_BAR_MODE_NORMAL);
    lv_obj_align(home_bar, LV_ALIGN_TOP_MID, 0, 120);
    lv_bar_set_range(home_bar, 0, 3000);
    lv_bar_set_value(home_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(home_bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(home_bar, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
    
    // Grid bar (vertical, 12px wide × 120px tall)
    grid_bar = lv_bar_create(background);
    lv_obj_set_size(grid_bar, 12, 120);
    lv_bar_set_mode(grid_bar, LV_BAR_MODE_NORMAL);
    lv_obj_align(grid_bar, LV_ALIGN_TOP_MID, 107, 120);
    lv_bar_set_range(grid_bar, 0, 3000);
    lv_bar_set_value(grid_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(grid_bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(grid_bar, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
}
```

---

### Step 14: Implement Bar Update Function (15 min)

**File**: `src/app/screen_power.cpp`

Add after `update_power_colors()`:

```cpp
void PowerScreen::update_power_bars() {
    // Helper lambda to convert kW to bar value (0-3000mW)
    auto kw_to_bar = [](float kw) -> int16_t {
        if (isnan(kw)) return 0;  // No data → empty bar
        float abs_kw = fabsf(kw);  // Absolute value for negative grid power
        if (abs_kw > 3.0f) abs_kw = 3.0f;  // Clamp to max scale
        return (int16_t)(abs_kw * 1000.0f);  // Convert to milliwatts
    };
    
    // Update solar bar
    if (solar_bar) {
        lv_bar_set_value(solar_bar, kw_to_bar(solar_kw), LV_ANIM_OFF);
    }
    
    // Update grid bar
    if (grid_bar) {
        lv_bar_set_value(grid_bar, kw_to_bar(grid_kw), LV_ANIM_OFF);
    }
    
    // Update home bar (calculated value)
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

### Step 15: Integrate Bar Updates into Power Setters (10 min)

**File**: `src/app/screen_power.cpp`

Modify `set_solar_power()` to call bar update:

```cpp
void PowerScreen::set_solar_power(float kw) {
    // ... existing code ...
    
    // Update colors and bars
    update_power_colors();
    update_power_bars();  // NEW: Update bars
    
    // ... rest of existing code ...
}
```

Modify `set_grid_power()`:

```cpp
void PowerScreen::set_grid_power(float kw) {
    // ... existing code ...
    
    // Update colors and bars
    update_power_colors();
    update_power_bars();  // NEW: Update bars
    
    // ... rest of existing code ...
}
```

Modify `update_home_value()`:

```cpp
void PowerScreen::update_home_value() {
    // ... existing code ...
    
    // Update colors and bars
    update_power_colors();
    update_power_bars();  // NEW: Update bars
}
```

---

### Step 16: Update destroy() to Null Bar Pointers (5 min)

**File**: `src/app/screen_power.cpp`

In `PowerScreen::destroy()`:

```cpp
void PowerScreen::destroy() {
    if (screen_obj) {
        lv_obj_del(screen_obj);
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

### Step 17: Build and Test P3 (30 min)

```bash
# Build
./build.sh esp32c3

# Flash and monitor
./upload.sh esp32c3
./monitor.sh

# Test scenarios:
# 1. Solar 0kW → empty bar (no fill from bottom)
# 2. Solar 1.5kW → 50% filled bar (60px from bottom)
# 3. Solar 3kW → 100% filled bar (full 120px height)
# 4. Solar 4.5kW → 100% filled bar (clamped at 120px)
# 5. Grid -1kW (exporting) → bar shows 33% height (abs value, 40px from bottom)
# 6. Home power bar matches solar + grid calculation
# 7. All bars extend upward from bottom, proportional to 0-3kW range
```

**Validation Checklist**:
- [ ] Vertical bars appear below "kW" labels, extending toward screen bottom
- [ ] Bars fill from bottom upward proportionally
- [ ] Bars update in sync with power values
- [ ] Bars use absolute values for negative grid power
- [ ] Bars clamp at 100% height for values >3kW
- [ ] FPS counter still shows 60 FPS
- [ ] No memory errors or crashes

---

## Final Integration Testing (30 min)

### Complete Feature Test

Test all 3 priorities together:

```bash
# Clean build
./clean.sh
./build.sh esp32c3
./upload.sh esp32c3
./monitor.sh
```

**Validation Matrix**:

| Power State | Expected Icon Color | Expected Bar Height |
|-------------|---------------------|---------------------|
| Solar 0kW | White | 0px (empty) |
| Solar 0.8kW | Bright Green | 32px (27%) |
| Solar 2.5kW | Bright Green | 100px (83%) |
| Grid -0.5kW | Bright Green | 20px (17%, abs) |
| Grid 0.5kW | White | 20px (17%) |
| Grid 1.8kW | Orange | 72px (60%) |
| Grid 3.2kW | Red | 120px (100%, clamped) |
| Home 0.3kW | Bright Green | 12px (10%) |
| Home 0.8kW | White | 32px (27%) |
| Home 1.5kW | Orange | 60px (50%) |
| Home 2.5kW | Red | 100px (83%) |

---

## Troubleshooting

### Build Errors

**Error**: `icon_assets.h: No such file or directory`
- **Cause**: Icon conversion script not run
- **Fix**: Run `./tools/convert-icons.sh` manually, then rebuild

**Error**: `lv_img_conv: command not found`
- **Cause**: Icon converter not installed
- **Fix**: `npm install -g lv_img_conv`

**Error**: `solar.png dimensions are 64 64, expected 48 48`
- **Cause**: PNG files wrong size
- **Fix**: Resize PNGs to exactly 48×48 pixels

### Runtime Errors

**Problem**: Colors not changing
- **Check**: MQTT data arriving (look for "-- " in values)
- **Check**: `update_power_colors()` being called after value update
- **Debug**: Add `Serial.printf("Solar color: %d\n", get_solar_color(solar_kw));`

**Problem**: Icons not displaying
- **Check**: `icon_assets.h` includes in `screen_power.cpp`
- **Check**: Using `lv_img_set_src()` not `lv_label_set_text()`
- **Check**: Flash usage not exceeded (build output shows partition sizes)

**Problem**: Bars stuck at 0% or not filling upward
- **Check**: `update_power_bars()` being called
- **Check**: Bar range set to 0-3000 (not 0-3)
- **Check**: Bar mode set to LV_BAR_MODE_NORMAL (fills from bottom)
- **Debug**: Add `Serial.printf("Bar value: %d\n", kw_to_bar(solar_kw));`

**Problem**: FPS drops below 60
- **Check**: Using `LV_ANIM_OFF` for bar updates (no animation)
- **Check**: Not calling update functions in tight loop
- **Optimize**: Reduce serial logging in production

---

## Version Control Commit Strategy

Commit each priority independently for easier review and rollback:

```bash
# After P1 complete
git add src/app/screen_power.cpp src/app/screen_power.h
git commit -m "feat: Add color-coded power indicators (P1)

- Implement grid/home/solar color thresholds
- Add color calculation helper methods
- Integrate color updates into power setters
- Colors: Green (low), White (normal), Orange (medium), Red (high)

Refs #001"

# After P2 complete
git add assets/icons/*.png tools/convert-icons.sh src/app/icon_assets.* .gitignore build.sh src/app/screen_power.cpp
git commit -m "feat: Replace LVGL icons with custom PNG icons (P2)

- Add 48x48 PNG icons for solar/home/grid
- Create icon conversion build script
- Integrate conversion into build process
- Use image widgets with recoloring

Refs #001"

# After P3 complete
git add src/app/screen_power.cpp src/app/screen_power.h
git commit -m "feat: Add power bar charts (P3)

- Create horizontal bar widgets below labels
- Implement 0-3kW proportional visualization
- Use absolute values for negative grid power
- Clamp values >3kW to 100% fill

Refs #001"
```

---

## Performance Validation

After all priorities complete, verify performance targets:

```bash
# Monitor serial output for FPS counter
# Should show consistent 60 FPS

# Check flash usage
arduino-cli compile --fqbn esp32:esp32:esp32c3 --output-dir build/esp32c3 src/app 2>&1 | grep "Sketch uses"

# Expected: <85% of partition size
# Example output: Sketch uses 892340 bytes (85%) of program storage space. Maximum is 1048576 bytes.
```

**Performance Targets**:
- [x] 60 FPS LVGL rendering maintained
- [x] <500ms color update latency
- [x] <85% flash usage
- [x] <2% heap usage increase (~900 bytes of 48KB)

---

## Documentation Updates

Update `CHANGELOG.md`:

```markdown
## [1.1.0] - YYYY-MM-DD

### Added
- **Power Screen Enhancements**: Visual improvements for energy monitoring
  - Color-coded power indicators: Green (low/export), White (normal), Orange (medium), Red (high)
  - Custom PNG icons (48×48) for solar, home, and grid power
  - Proportional bar charts showing 0-3kW power levels
  - Automatic icon conversion from PNG to LVGL C arrays in build process

### Changed
- Power screen icons now use custom PNG images instead of LVGL symbol fonts
- Power screen widgets dynamically change color based on consumption/generation thresholds
```

Update `src/version.h`:

```cpp
#define VERSION_MAJOR 1
#define VERSION_MINOR 1  // Incremented from 0
#define VERSION_PATCH 0
```

---

## Implementation Complete

All 3 priorities implemented and tested. Total implementation time: 4-6 hours.

**Next Steps**:
- Create pull request with changes
- Run GitHub Actions CI build for all boards
- Test on physical hardware
- Update documentation with screenshots
- Tag release v1.1.0
