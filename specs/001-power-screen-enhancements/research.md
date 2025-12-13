# Research: Power Screen Enhancements

**Feature**: Power Screen Enhancements  
**Branch**: `001-power-screen-enhancements`  
**Date**: 2025-12-13

## Research Tasks

### 1. LVGL Color Management for Dynamic Text/Icon Styling

**Question**: How should color changes be applied to existing LVGL label objects (icons, values, labels) without recreating widgets?

**Decision**: Use `lv_obj_set_style_text_color(obj, lv_color_hex(0xRRGGBB), LV_PART_MAIN)` API

**Rationale**:
- LVGL supports dynamic style changes without object recreation
- `lv_color_hex()` converts 24-bit RGB hex values to LVGL color format
- LV_PART_MAIN applies to the main part of the widget (vs borders, backgrounds)
- Already proven pattern in codebase: `screen_power.cpp` line 41 uses `lv_obj_set_style_text_color(solar_icon, TEXT_COLOR, 0)`
- Zero argument defaults to LV_PART_MAIN and LV_STATE_DEFAULT
- Performance: O(1) style property update, no reallocation

**Alternatives considered**:
- Recreate label objects on every update → Rejected: Causes memory fragmentation, breaks LVGL object hierarchy
- Use LVGL themes → Rejected: Too heavyweight for 3 power states, requires theme switching infrastructure

**Implementation note**: Color updates should occur in existing `PowerScreen::set_solar_power()`, `set_grid_power()`, and `update_home_value()` methods after value text update.

---

### 2. LVGL Bar Chart Widget API

**Question**: What LVGL widget and API should be used for bar chart visualization of power values?

**Decision**: Use `lv_bar_create()` widget with vertical orientation and value range mapping

**Rationale**:
- LVGL provides built-in `lv_bar` widget designed for progress/value visualization
- Supports vertical orientation via `lv_obj_set_size(bar, width, height)` (height > width)
- Value range configured via `lv_bar_set_range(bar, min, max)` → `lv_bar_set_range(bar, 0, 3000)` for 0-3kW in milliwatts
- Current value set via `lv_bar_set_value(bar, value, LV_ANIM_OFF)` → no animation for instant updates
- Bar fill color customizable via `lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR)`
- Existing LVGL@8.4.0 dependency, no new library needed
- Proven in `lv_conf.h`: `#define LV_USE_BAR 1` already enabled

**Alternatives considered**:
- Custom rectangle drawing with lv_obj_draw_part → Rejected: Reinvents LVGL bar functionality
- lv_canvas with manual pixel manipulation → Rejected: Too low-level, breaks LVGL styling system
- lv_chart widget → Rejected: Designed for time-series data, overkill for single value bar

**Implementation note**:
- Create 3 bar widgets in `PowerScreen::create()` after label row
- Position below "kW" labels: `lv_obj_align(bar, LV_ALIGN_TOP_MID, x_offset, 140)` (20px below label at Y=115)
- Set bar dimensions: 60px width × 8px height (matches 3-column spacing)
- Update bar values in `set_solar_power()`, `set_grid_power()`, `update_home_value()` using absolute power value

---

### 3. PNG to LVGL C Array Conversion Process

**Question**: What build-time tool and process should convert 48x48 PNG files to LVGL-compatible C arrays?

**Decision**: Use LVGL's official online image converter (https://lvgl.io/tools/imageconverter) or `lv_img_conv` CLI tool during build

**Rationale**:
- LVGL provides official image conversion toolchain
- Output format: C array with `lv_img_dsc_t` structure compatible with `lv_img_create()` or `lv_label_set_recolor()`
- For icons: Use "True color" output format (RGB565 for LV_COLOR_DEPTH 16) with alpha channel
- For 48x48 @ RGB565: ~4.6KB per icon × 3 icons = ~14KB flash (well within <85% target)
- Build integration: Add pre-build step to `build.sh` or create `tools/convert-icons.sh` script
- CLI tool `lv_img_conv` available via npm: `npm install -g lv_img_conv`

**Alternatives considered**:
- ImageMagick + manual C array formatting → Rejected: No LVGL metadata, manual struct creation error-prone
- Python PIL with custom converter → Rejected: Adds Python dependency, reinvents LVGL tooling
- Runtime PNG decoding (LV_USE_PNG) → Rejected: `lv_conf.h` line 312 shows `#define LV_USE_PNG 0`, would require runtime decoder overhead

**Implementation approach**:
1. Store source PNGs in `assets/icons/` (solar.png, home.png, grid.png)
2. Create `tools/convert-icons.sh` script to validate size and convert to C arrays
3. Output to `src/app/icon_assets.h` (generated file, add to .gitignore)
4. Call conversion script from `build.sh` before arduino-cli compile
5. Fail build if PNG validation fails (wrong size, missing files, invalid format)

**PNG validation criteria**:
- Dimensions: Exactly 48×48 pixels (use `identify` from ImageMagick or `file` command)
- Format: PNG (check magic bytes or file extension)
- Color depth: RGB or RGBA (alpha channel recommended for anti-aliasing)

---

### 4. Color Threshold Logic and Hex Value Mapping

**Question**: How should color threshold ranges be implemented and what exact hex values map to each power state?

**Decision**: Use if-else ladder with float comparisons and const lv_color_t definitions

**Rationale**:
- Spec clarification defined exact thresholds and hex values:
  - Grid: <0 → #00FF00, ≥0<1kW → #FFFFFF, ≥1kW<2.5kW → #FFA500, ≥2.5kW → #FF0000
  - Home: <0.5kW → #00FF00, ≥0.5kW<1kW → #FFFFFF, ≥1kW<2kW → #FFA500, ≥2kW → #FF0000
  - Solar: <0.5kW → #FFFFFF, ≥0.5kW → #00FF00
- Implementation as const at file scope for flash storage (PROGMEM):
  ```cpp
  const lv_color_t COLOR_GREEN = lv_color_hex(0x00FF00);
  const lv_color_t COLOR_WHITE = lv_color_hex(0xFFFFFF);
  const lv_color_t COLOR_ORANGE = lv_color_hex(0xFFA500);
  const lv_color_t COLOR_RED = lv_color_hex(0xFF0000);
  ```
- Threshold logic as helper functions to avoid code duplication:
  ```cpp
  lv_color_t get_grid_color(float kw);
  lv_color_t get_home_color(float kw);
  lv_color_t get_solar_color(float kw);
  ```

**Alternatives considered**:
- Lookup table with struct arrays → Rejected: 3 different threshold sets, not reusable
- Macro definitions → Rejected: No type safety, harder to debug
- Single function with enum parameter → Rejected: Each power type has different thresholds

**Edge case handling**:
- NaN values: Return COLOR_WHITE (neutral state, matches existing "--" text behavior)
- Negative home power: Treat as 0kW (shouldn't occur based on calculation, but safe fallback)
- Boundary values: Use >= comparison (e.g., exactly 1.0kW uses >= 1kW < 2.5kW range → orange)

---

### 5. Layout Integration: Bar Chart Positioning

**Question**: What are the exact Y-coordinates and dimensions for bar charts to fit below existing labels without overflow?

**Decision**: Position bars at Y=140, 60px wide × 8px height, aligned with respective columns

**Rationale**:
- Current layout from `screen_power.cpp`:
  - Icons row: Y=15 (48px font height)
  - Values row: Y=80 (32px font height)
  - Labels row: Y=115 (14px font + "kW" text)
- Screen dimensions: 280×240 (rotated from 240×280)
- Available space below labels: 240 - 115 - 14 - margin = ~110px vertical
- Bar placement: Y=140 gives 25px spacing after label baseline
- Bar dimensions: 60px width matches icon spacing (3 columns across 280px width), 8px height sufficient for visibility
- Column X offsets (from screen_power.cpp):
  - Solar: X=-107 (left column)
  - Home: X=0 (center column)
  - Grid: X=107 (right column)

**Visual layout**:
```
Y=15:  [Icon48]      [Icon48]      [Icon48]      (Icons row)
Y=80:  [Value32]     [Value32]     [Value32]     (Values row)
Y=115: [Label14]     [Label14]     [Label14]     (Labels "kW")
Y=140: [Bar 60x8]    [Bar 60x8]    [Bar 60x8]    (New bars)
```

**Alternatives considered**:
- Horizontal bars below each column → Rejected: Vertical bars better utilize available screen space (120px height available)
- Replace labels with bars → Rejected: Users need both numeric precision and visual comparison
- Larger bars (e.g., 20px width) → Rejected: 12px width sufficient for visibility at 2m viewing distance

**Implementation note**: Bars should use same background color (#000000) as screen, with indicator color matching power value color for consistency.

---

### 6. Build Process Integration for Icon Validation

**Question**: Where and how should PNG validation and conversion be integrated into the existing build.sh script?

**Decision**: Add pre-compile hook in `build.sh` before arduino-cli compile loop, with fail-fast on validation errors

**Rationale**:
- Existing `build.sh` structure:
  1. Install arduino-cli to `./bin/`
  2. Install libraries from `arduino-libraries.txt`
  3. For each board in FQBN_TARGETS: compile with arduino-cli
- Icon conversion must occur before compile (C arrays needed by source files)
- Validation must fail build immediately (don't compile if assets invalid)
- Script location: `tools/convert-icons.sh` alongside existing `tools/minify-web-assets.sh`

**Proposed build.sh integration** (insert after line ~100, before compile loop):
```bash
# Convert and validate icon assets
echo "Converting PNG icons to LVGL C arrays..."
./tools/convert-icons.sh || {
    echo "ERROR: Icon conversion failed. Check assets/icons/ for valid 48x48 PNG files."
    exit 1
}
```

**tools/convert-icons.sh responsibilities**:
1. Check for `assets/icons/solar.png`, `home.png`, `grid.png`
2. Validate each PNG is exactly 48×48 pixels (use `identify -format "%w %h" file.png`)
3. Convert using `lv_img_conv` CLI:
   ```bash
   lv_img_conv assets/icons/solar.png -f true_color_alpha -o src/app/icon_assets.c
   ```
4. Generate header file `src/app/icon_assets.h` with extern declarations
5. Exit 0 on success, exit 1 on any validation failure

**Validation checks**:
- File exists: `[ -f "assets/icons/solar.png" ] || exit 1`
- Dimensions: `SIZE=$(identify -format "%w %h" solar.png)` → check `== "48 48"`
- Format: Check file magic bytes or extension
- Conversion success: Check lv_img_conv exit code

**Error messages**:
- Missing file: "ERROR: assets/icons/solar.png not found"
- Wrong size: "ERROR: solar.png is 64x64, expected 48x48"
- Invalid format: "ERROR: solar.png is not a valid PNG file"

**Alternatives considered**:
- Manual conversion workflow → Rejected: Error-prone, doesn't integrate with CI
- Convert at runtime using LV_USE_PNG → Rejected: Disabled in lv_conf.h, wastes runtime resources
- Commit generated C arrays to git → Rejected: Source PNGs are single source of truth, generated files should be gitignored

---

### 7. Memory Impact Analysis

**Question**: What is the total memory footprint (flash + heap) of adding 3 icons and 3 bar widgets?

**Decision**: Estimated total footprint is acceptable within existing 48KB heap and <85% flash constraints

**Calculation**:
- **Flash (PROGMEM)**:
  - 3 PNG icons @ 48×48 RGB565: 48×48×2 bytes = 4,608 bytes each
  - Total icons: 4,608 × 3 = 13,824 bytes (~13.5KB)
  - LVGL image descriptors: ~100 bytes each × 3 = 300 bytes
  - Color threshold code: ~500 bytes (helper functions)
  - Total flash: ~14.6KB
  
- **Heap (runtime)**:
  - 3 bar widgets: ~200 bytes each (LVGL lv_bar object) = 600 bytes
  - Existing 6 labels already allocated (no change)
  - Bar style memory: ~100 bytes per bar × 3 = 300 bytes
  - Total heap: ~900 bytes

**Current baseline** (from CHANGELOG.md and lv_conf.h):
- LVGL heap: 48KB (`LV_MEM_SIZE`)
- Display buffers: 48KB (double buffering, `LCD_WIDTH * 20 * 2`)
- Current power screen: ~1.5KB (6 labels + 2 arrows)
- **Impact**: +900 bytes heap (<2% of 48KB), +14.6KB flash (acceptable for ESP32 4MB flash)

**Alternatives considered**:
- Compress PNG icons → Rejected: LVGL decompression adds runtime CPU overhead
- Use smaller icons (32×32) → Rejected: Violates spec, less visible from 2m distance
- Reuse icon memory → Rejected: All 3 icons shown simultaneously

**Conclusion**: Memory impact is minimal and well within budget. No optimization needed.

---

### 8. LVGL Image Widget vs Label Approach for Icons

**Question**: Should custom PNG icons use `lv_img_create()` widgets or remain as labels with image data?

**Decision**: Use `lv_img_create()` widgets for PNG icons, replacing current `lv_label_create()` with LV_SYMBOL_*

**Rationale**:
- Current implementation uses `lv_label_create()` + `lv_label_set_text(LV_SYMBOL_REFRESH)` for icons
- LV_SYMBOL_* are font glyphs from Montserrat 48pt, not true images
- PNG icons require `lv_img_create()` for proper rendering:
  ```cpp
  solar_icon = lv_img_create(background);
  lv_img_set_src(solar_icon, &icon_solar);  // icon_solar from icon_assets.h
  lv_obj_align(solar_icon, LV_ALIGN_TOP_MID, -107, 15);
  ```
- Image widgets support color re-coloring via `lv_obj_set_style_img_recolor()`
- Alignment and positioning identical to label widgets (no layout changes)

**Migration path**:
1. Change `lv_label_create()` → `lv_img_create()` in `PowerScreen::create()`
2. Change `lv_label_set_text(icon, LV_SYMBOL_*)` → `lv_img_set_src(icon, &icon_data)`
3. Remove `lv_obj_set_style_text_font()` calls (not applicable to images)
4. Keep `lv_obj_set_style_text_color()` for recoloring (change to `lv_obj_set_style_img_recolor()`)

**Color recoloring approach**:
- Use `lv_obj_set_style_img_recolor(icon, color, LV_PART_MAIN)` with `lv_obj_set_style_img_recolor_opa(icon, 255, LV_PART_MAIN)`
- Recolor opacity 255 = full recolor (replaces all colors with target color)
- Preserves alpha channel for anti-aliasing

**Alternatives considered**:
- Keep as labels, encode icons as custom fonts → Rejected: More complex than PNG workflow
- Use lv_canvas to draw icons → Rejected: No automatic color management

---

## Technology Decisions Summary

| Decision | Technology/Approach | Justification |
|----------|---------------------|---------------|
| Color updates | `lv_obj_set_style_text_color()` | Built-in LVGL API, no memory allocation, proven in codebase |
| Bar charts | `lv_bar_create()` widget | LVGL built-in, designed for value visualization, configurable range |
| PNG conversion | `lv_img_conv` CLI tool | Official LVGL toolchain, produces correct lv_img_dsc_t format |
| Color storage | `const lv_color_t` in flash | PROGMEM-friendly, compile-time constants |
| Icon rendering | `lv_img_create()` widgets | Proper PNG image rendering with recoloring support |
| Build integration | Pre-compile hook in build.sh | Fail-fast validation, CI-compatible, follows existing pattern |
| Memory allocation | Static LVGL objects + PROGMEM | Respects heap budget, no dynamic allocation in callbacks |
| Layout positioning | Y=120-240 vertical bars, 12×120px | Vertical bars utilize available space, fill from bottom up, maintains 60 FPS |

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| PNG conversion tool not installed | Build fails | Document `npm install -g lv_img_conv` in README, add check to build.sh |
| Icon colors don't recolor properly | Wrong visual feedback | Test recolor API in Phase 1, fallback to separate icon color variants if needed |
| Bar charts cause FPS drop | <60 FPS target | Use LV_ANIM_OFF for instant updates, profile with existing FPS counter |
| Flash size exceeds 85% | Build fails or OTA breaks | Monitor partition usage in build.sh, optimize icon compression if needed |
| LVGL bar widget not performant | UI lag | Bar updates only on 1Hz MQTT, no animation, minimal CPU impact |

---

## Open Questions for Phase 1

1. Should bar chart background be visible or transparent? → Design decision in quickstart
2. Should bar color match icon/text color or remain fixed? → Spec implies single color (likely white indicator)
3. Should icons have drop shadow for depth? → PNG design choice, not code decision

---

## Research Complete

All technical unknowns resolved. No NEEDS CLARIFICATION markers remain. Ready to proceed to Phase 1 (data model, contracts, quickstart).

**Next Steps**:
- Phase 1A: Define data structures in `data-model.md`
- Phase 1B: Define function contracts in `contracts/`
- Phase 1C: Generate implementation guide in `quickstart.md`
- Phase 1D: Update agent context with new patterns
