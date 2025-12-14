# LVGL 8.4.0 BGR Display Anti-Aliasing Fix

## Problem Description

When using LVGL 8.4.0 with BGR mode displays (like ST7789V2), anti-aliased text shows color artifacts:
- Red text displays with yellow halos on edges
- Orange text appears with green/blue tints
- Overall "ugly" appearance due to incorrect color blending at text edges

## Root Cause

**LVGL color blending vs. Display hardware mismatch:**

1. LVGL renders and blends colors in **RGB color space**
2. ST7789V2 display hardware is in **BGR mode** (MADCTL bit 3 = 0)
3. During anti-aliasing, LVGL creates intermediate colors by blending:
   - Example: Red text (R=255, G=0, B=0) on black background
   - LVGL creates edge pixel (R=128, G=0, B=0) for smoothing
4. Display interprets this as BGR → reads R channel as B channel
   - Result: (B=128, G=0, R=0) = dark blue component
   - Creates yellow/green artifacts on red text edges

## Solution Overview

**Swap RGB↔BGR in the display flush callback** to align LVGL's RGB blending with the display's BGR interpretation.

This is the same approach used in LVGL 9.x with `lv_draw_sw_rgb565_swap()`, but LVGL 8.4.0 doesn't have this function, so we implement it manually.

## Implementation

### Step 1: Disable LV_COLOR_16_SWAP

In `src/app/lv_conf.h`:

```c
/*Swap the 2 bytes of RGB565 color. Useful if the display has an 8-bit interface (e.g. SPI)*/
#define LV_COLOR_16_SWAP 0  /* Disabled - we do RGB→BGR swap in flush callback instead */
```

**Why:** `LV_COLOR_16_SWAP` only swaps byte order for SPI, not RGB↔BGR channels. We handle both in the flush callback.

### Step 2: Add RGB→BGR Swap Function

In `src/app/display_manager.cpp`:

```c
// RGB565 to BGR565 color swap for anti-aliasing fix
// LVGL blends colors in RGB space, but ST7789V2 display expects BGR
// Swapping here fixes color artifacts on anti-aliased text edges
static inline void rgb565_to_bgr565(uint16_t *pixels, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint16_t color = pixels[i];
        // Extract RGB components from RGB565
        uint16_t r = (color >> 11) & 0x1F;  // 5 bits red
        uint16_t g = (color >> 5) & 0x3F;   // 6 bits green
        uint16_t b = color & 0x1F;          // 5 bits blue
        // Recombine as BGR565
        pixels[i] = (b << 11) | (g << 5) | r;
    }
}
```

### Step 3: Call Swap in Flush Callback

Update `display_flush_cb()` in `src/app/display_manager.cpp`:

```c
static void display_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    const uint32_t pixel_count = w * h;

    // Swap RGB to BGR for display hardware
    rgb565_to_bgr565((uint16_t *)color_p, pixel_count);

    lcd_set_window(area->x1, area->y1, area->x2, area->y2);
    lcd_push_colors((uint16_t *)color_p, pixel_count);

    lv_disp_flush_ready(disp);
}
```

### Step 4: Define Colors in BGR Format

Since we swap RGB→BGR in the flush callback, **define colors as BGR values** so they display correctly:

```c
// Display hardware is BGR mode, and we swap RGB→BGR in flush callback to fix anti-aliasing
// Therefore, we define colors in BGR format here so they display correctly after the swap
// 
// BGR color mapping (what we define → what displays):
//   Define BLUE 0x0000FF   → displays as RED (after RGB→BGR swap)
//   Define GREEN 0x00FF00  → displays as GREEN (G channel unaffected)
//   Define RED 0xFF0000    → displays as BLUE (after RGB→BGR swap)
//   Define 0x00A5FF        → displays as ORANGE (BGR for orange)
//
const lv_color_t COLOR_BRIGHT_GREEN = lv_color_hex(0x00FF00);  // Green (unaffected by swap)
const lv_color_t COLOR_WHITE        = lv_color_hex(0xFFFFFF);  // White (unaffected)
const lv_color_t COLOR_ORANGE       = lv_color_hex(0x00A5FF);  // BGR for orange (displays as RGB 255,165,0)
const lv_color_t COLOR_RED          = lv_color_hex(0x0000FF);  // BGR for red (displays as RGB 255,0,0)
```

**Color Conversion Table:**

| Desired Display Color | RGB Value     | Define as BGR |
|-----------------------|---------------|---------------|
| Red                   | 0xFF0000      | 0x0000FF      |
| Green                 | 0x00FF00      | 0x00FF00      |
| Blue                  | 0x0000FF      | 0xFF0000      |
| Orange (255,165,0)    | 0xFFA500      | 0x00A5FF      |
| Cyan (0,173,181)      | 0x00ADB5      | 0xB5AD00      |
| White                 | 0xFFFFFF      | 0xFFFFFF      |
| Black                 | 0x000000      | 0x000000      |
| Gray                  | 0x333333      | 0x333333      |

**Note:** Green, white, black, and pure grays are unaffected because swapping R↔B doesn't change them.

## Results

✅ **Fixed:**
- Anti-aliased text renders cleanly with no color artifacts
- Red text displays as pure red without yellow halos
- Orange text displays correctly without green tints
- All text remains sharp and crisp

✅ **No performance impact:**
- Swap happens in flush callback (already blocking)
- Inline function optimizes well
- No additional memory allocation

## Hardware Configuration

Display remains in BGR mode:

In `src/app/lcd_driver.cpp`:

```c
// ST7789V2 init sequence (from Waveshare sample)
// BGR mode (manufacturer default) - display_manager.cpp swaps RGB→BGR in flush callback
// This allows LVGL to blend colors correctly in RGB space, fixing anti-aliasing artifacts
lcd_write_command(0x36);
lcd_write_data(0x00);  // BGR mode (bit 3 = 0), portrait orientation
```

## References

- **LVGL Forum:** Color artifacts discussion led to discovery of subpixel rendering BGR flag (applies to subpixel fonts only, not regular anti-aliasing)
- **LVGL 9.x Solution:** `lv_draw_sw_rgb565_swap()` function in flush callback (not available in LVGL 8.4.0)
- **Reddit r/embedded:** ST7796S display color issues - byte swap + bit inversion discussion confirming RGB↔BGR swap approach
- **Manufacturer Sample:** `/samples/GUI_Paint.h` validates RGB565 color values

## Testing

Tested configurations:
- Display: ST7789V2 240×280 (1.69" Waveshare)
- MCU: ESP32-C3 @ 160MHz
- LVGL: 8.4.0
- Fonts: Montserrat 14, 20, 32, 48 (4-bit anti-aliased)
- Colors: Red, orange, green, white on black background
- Result: Clean, crisp anti-aliased text with accurate colors

## Alternative Approaches Considered

❌ **RGB mode (MADCTL bit 3 = 1):** Makes colors correct but breaks byte order for SPI
❌ **1-bit fonts (no anti-aliasing):** Eliminates artifacts but text looks jagged
❌ **LV_FONT_SUBPX_BGR flag:** Only applies to subpixel rendering, not regular anti-aliasing
❌ **LVGL 9.x upgrade:** Breaking changes, significant refactoring required
✅ **RGB→BGR swap in flush:** Clean solution, no breaking changes

## Conclusion

The RGB→BGR swap in the flush callback is the **correct solution** for LVGL 8.4.0 with BGR displays. It allows LVGL to perform color blending correctly while ensuring the display receives colors in the format it expects.

This approach will be standard in LVGL 9.x via `lv_draw_sw_rgb565_swap()`. For LVGL 8.4.0, manual implementation is required.
