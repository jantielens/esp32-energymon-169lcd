# Implementation Plan: Power Screen Enhancements

**Branch**: `001-power-screen-enhancements` | **Date**: 2025-12-13 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/001-power-screen-enhancements/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

This feature enhances the existing power monitoring screen with three priority-ordered improvements: (P1) Color-coded visual indicators for grid/home/solar power using hex color values based on power thresholds, (P2) Custom 48x48 PNG icons replacing LVGL symbols with build-time validation and conversion, and (P3) Vertical bar charts showing proportional power visualization on a 0-3kW scale. The feature maintains the current 3-column layout (Solar → Home ← Grid) and respects the 1Hz MQTT update rate.

## Technical Context

**Language/Version**: C++ (Arduino framework for ESP32, ESP-IDF v2.0.x via arduino-cli)  
**Primary Dependencies**: LVGL@8.4.0 (display widgets), lv_font_montserrat_48/32/14 (text rendering), existing PowerScreen class  
**Storage**: No NVS required (no user configuration - color/icon settings are compile-time constants)  
**Testing**: Build verification via `./build.sh` (all boards must compile), manual hardware testing on 240x280 ST7789V2 display  
**Target Platform**: ESP32/ESP32-C3/ESP32-C6 microcontrollers (Arduino framework), 240x280 LCD (rotated to 280x240)  
**Project Type**: Embedded firmware (Arduino sketch structure in `src/app/`)  
**Performance Goals**: Maintain 60 FPS LVGL rendering, <500ms color update latency, <85% flash usage after PNG-to-C array conversion  
**Constraints**: 48KB heap for LVGL, SPI 60MHz for ST7789V2, non-blocking callbacks, 1Hz MQTT update rate, icons must be 48x48 pixels  
**Scale/Scope**: Single PowerScreen class modifications, 3 PNG icon files, 3 color threshold ranges per power type, 3 bar chart widgets

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Initial Check (Before Phase 0)

**Hardware-Driven Design Compliance:**
- [x] Pin assignments documented for all supported boards (ESP32/ESP32-C3/ESP32-C6) - N/A: No new GPIO usage, display already configured
- [x] Memory budget validated (heap, flash, PSRAM if applicable) - 48KB heap sufficient for LVGL, PNG assets stored in PROGMEM flash
- [x] Hardware constraints identified (SPI speed, GPIO limitations, sensor accuracy) - SPI 60MHz ST7789V2, 240x280 display, 1Hz MQTT updates

**Configuration-First UX Compliance:**
- [x] Feature accessible via web portal (no serial console required) - N/A: Visual-only feature, no user configuration needed
- [x] Settings persisted to NVS with backward compatibility - N/A: No runtime configuration (compile-time color/icon settings)
- [x] REST API endpoint(s) defined if programmatic access needed - N/A: No API access required for display enhancements
- [x] Mobile-responsive UI design (<768px breakpoint) - N/A: No web UI changes

**Build Automation Compliance:**
- [x] Changes compile on all board targets in `config.sh` `FQBN_TARGETS` - Screen code board-agnostic, uses LVGL abstraction
- [x] New library dependencies added to `arduino-libraries.txt` with version pin - No new libraries (uses existing LVGL@8.4.0)
- [x] Board-specific code isolated to `src/boards/[board]/board_overrides.h` if needed - N/A: No board-specific display code

**Embedded Best Practices Compliance:**
- [x] No dynamic allocation in interrupt handlers or LVGL callbacks - Color changes via lv_obj_set_style_text_color (static LVGL calls)
- [x] No blocking operations in display flush or network callbacks - All updates via existing PowerScreen::set_*_power() pattern
- [x] WiFi reconnection uses non-blocking retry (5s+ backoff) - N/A: No WiFi/network code changes
- [x] Watchdog handling documented for long-running operations - N/A: No long-running operations added

**Semantic Versioning Compliance:**
- [x] Version bump type determined (MAJOR for breaking changes, MINOR for features, PATCH for fixes) - MINOR: New visual features, no breaking changes
- [x] `CHANGELOG.md` entry prepared with Keep a Changelog format - Added section for 1.1.0 with color coding, icons, bar charts
- [x] Breaking changes include migration guide if applicable - N/A: No breaking changes

**Gate Status (Initial)**: ✅ PASSED - All constitution requirements satisfied, no violations to justify

### Re-Check (After Phase 1 Design)

**Hardware-Driven Design Compliance:**
- [x] Memory budget still valid after design - ~14KB flash for 3 PNG icons (48x48 RGB565), 900 bytes heap for 3 bar widgets, within budgets
- [x] Performance targets achievable with design - Color updates O(1) if-else, bar updates via lv_bar_set_value <1ms, 60 FPS maintained
- [x] Hardware constraints respected - All rendering on LVGL thread, no SPI conflicts, 1Hz update rate preserves responsiveness

**Configuration-First UX Compliance:**
- [x] Design maintains zero-config approach - All thresholds hardcoded in get_*_color() functions, icons baked into firmware
- [x] No runtime state to persist - Color/bar state derived from power values, no user preferences

**Build Automation Compliance:**
- [x] Icon conversion integrated into build.sh - tools/convert-icons.sh runs before compilation, validates PNG dimensions
- [x] Build failures detectable - PNG missing/wrong size → script exits 1, arduino-cli won't proceed
- [x] Cross-platform compatible - lv_img_conv CLI (npm) works on Linux/macOS/Windows, ImageMagick optional

**Embedded Best Practices Compliance:**
- [x] Zero dynamic allocation in update paths - All widgets allocated in create(), update functions only set properties
- [x] Update functions remain non-blocking - update_power_colors() and update_power_bars() are O(1) LVGL API calls
- [x] Icon assets in PROGMEM - lv_img_conv generates const lv_img_dsc_t with LVGL_ATTRIBUTE_CONST for flash storage

**Semantic Versioning Compliance:**
- [x] MINOR bump justified - New visual features (colors, icons, bars), no API/config breaking changes
- [x] Forward compatibility maintained - Existing MQTT handlers unchanged, new code isolated to PowerScreen class

**Gate Status (Post-Design)**: ✅ PASSED - Design complies with all constitution principles, no new violations introduced

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)
<!--
  ACTION REQUIRED: Replace the placeholder tree below with the concrete layout
  for this feature. The structure below reflects the ESP32 Arduino sketch layout.
  Expand with specific files for the feature being implemented.
-->

```text
# ESP32 Arduino Firmware Structure
src/
├── app/
│   ├── app.ino                    # Main Arduino sketch
│   ├── board_config.h             # Default pin/config defines
│   ├── [feature]_manager.cpp/h   # Feature implementation (e.g., display, MQTT, sensor)
│   ├── config_manager.cpp/h       # NVS configuration persistence
│   ├── web_portal.cpp/h           # Web server & REST API
│   ├── web_assets.cpp/h           # Embedded HTML/CSS/JS (generated)
│   └── web/
│       ├── _header.html           # Shared header template
│       ├── _nav.html              # Shared navigation template
│       ├── _footer.html           # Shared footer template
│       ├── [page].html            # Feature-specific web pages
│       ├── portal.css             # Styles
│       └── portal.js              # Client-side logic
├── boards/
│   ├── esp32/
│   │   └── board_overrides.h      # ESP32-specific pin overrides (optional)
│   ├── esp32c3/
│   │   └── board_overrides.h      # ESP32-C3-specific pin overrides (optional)
│   └── esp32c6/
│       └── board_overrides.h      # ESP32-C6-specific pin overrides (optional)
└── version.h                      # Firmware version (MAJOR.MINOR.PATCH)

build/                             # Compiled firmware (gitignored)
├── esp32/
│   └── esp32.bin
├── esp32c3/
│   └── esp32c3.bin
└── esp32c6/
    └── esp32c6.bin

docs/
└── [feature].md                   # Feature-specific documentation (pinouts, wiring, usage)

tools/
└── minify-web-assets.sh           # Build-time script to generate web_assets.h

# Root-level build scripts
./build.sh                         # Compile firmware for all/specific boards
./upload.sh                        # Flash firmware to device
./monitor.sh                       # Serial console monitor
./bum.sh                           # Build + Upload + Monitor convenience script
```

**Structure Decision**: ESP32 Arduino firmware follows sketch-based architecture. Feature implementations go in `src/app/[feature]_manager.cpp/h`, web UI changes in `src/app/web/`, board-specific GPIO mappings in `src/boards/[board]/board_overrides.h`. No traditional test directory - verification via build compilation and manual hardware testing.
**Structure Decision**: ESP32 Arduino firmware follows sketch-based architecture. Feature implementations go in `src/app/[feature]_manager.cpp/h`, web UI changes in `src/app/web/`, board-specific GPIO mappings in `src/boards/[board]/board_overrides.h`. No traditional test directory - verification via build compilation and manual hardware testing.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., Blocking I/O in callback] | [specific use case] | [why non-blocking insufficient] |
| [e.g., Dynamic allocation] | [performance need] | [why static buffer insufficient] |
