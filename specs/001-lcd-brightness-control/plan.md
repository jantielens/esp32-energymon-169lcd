# Implementation Plan: LCD Backlight Brightness Control

**Branch**: `001-lcd-brightness-control` | **Date**: December 13, 2025 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-lcd-brightness-control/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

Implement user-configurable LCD backlight brightness control (0-100%) accessible via web portal home page. Users adjust brightness in real-time via slider with immediate hardware feedback, then explicitly save settings via existing shared save button. Brightness persists across reboots via NVS storage. Existing `lcd_set_backlight()` function provides hardware control layer.

## Technical Context

**Language/Version**: C++ (Arduino framework for ESP32)  
**Primary Dependencies**: ESP Async WebServer 3.9.0, ArduinoJson 7.2.1, LVGL (display library)  
**Storage**: NVS (Non-Volatile Storage) for brightness persistence  
**Testing**: Build verification via `./build.sh` (all boards must compile), manual hardware testing on ESP32-C3 with 1.69" LCD  
**Target Platform**: ESP32/ESP32-C3/ESP32-C6 microcontrollers (Arduino framework)  
**Project Type**: Embedded firmware (Arduino sketch structure in `src/app/`)  
**Performance Goals**: <100ms brightness update latency, 60 FPS LVGL display maintained, <2KB additional NVS storage  
**Constraints**: Brightness control via PWM (analogWrite), no blocking in web server callbacks, existing save button pattern integration  
**Scale/Scope**: Single brightness setting (global, not per-user), 1 new REST API endpoint (`/api/brightness`), 1 web portal UI section ("Display Settings")

**Existing Hardware Support**: `lcd_set_backlight(uint8_t brightness)` already implemented in `lcd_driver.cpp` using PWM on `LCD_BL_PIN` (0-100% range validated)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**Hardware-Driven Design Compliance:**
- [x] Pin assignments documented for all supported boards (ESP32/ESP32-C3/ESP32-C6) - Uses existing `LCD_BL_PIN` from `board_config.h`, no new GPIO required
- [x] Memory budget validated (heap, flash, PSRAM if applicable) - ~1 byte NVS storage, ~500 bytes HTML/JS code (gzipped), negligible heap impact
- [x] Hardware constraints identified (SPI speed, GPIO limitations, sensor accuracy) - PWM brightness control validated 0-100%, no SPI/display timing impact

**Configuration-First UX Compliance:**
- [x] Feature accessible via web portal (no serial console required) - Full control via home page "Display Settings" section
- [x] Settings persisted to NVS with backward compatibility - New field added to `DeviceConfig` struct, defaults to 100% for upgrades
- [x] REST API endpoint(s) defined if programmatic access needed - `/api/brightness` (GET/POST) for real-time control
- [x] Mobile-responsive UI design (<768px breakpoint) - Slider control in existing responsive grid layout

**Build Automation Compliance:**
- [x] Changes compile on all board targets in `config.sh` `FQBN_TARGETS` - No board-specific code, uses existing `analogWrite()` API
- [x] New library dependencies added to `arduino-libraries.txt` with version pin - No new libraries required (uses existing dependencies)
- [x] Board-specific code isolated to `src/boards/[board]/board_overrides.h` if needed - Not applicable, feature uses existing GPIO abstraction

**Embedded Best Practices Compliance:**
- [x] No dynamic allocation in interrupt handlers or LVGL callbacks - Brightness value stored as uint8_t (stack/static)
- [x] No blocking operations in display flush or network callbacks - Async web server handlers, `lcd_set_backlight()` is non-blocking PWM
- [x] WiFi reconnection uses non-blocking retry (5s+ backoff) - Not applicable to brightness control
- [x] Watchdog handling documented for long-running operations - No long operations, all <1ms execution time

**Semantic Versioning Compliance:**
- [x] Version bump type determined (MAJOR for breaking changes, MINOR for features, PATCH for fixes) - **MINOR bump** (new feature, no breaking changes)
- [x] `CHANGELOG.md` entry prepared with Keep a Changelog format - Entry: "Added user-configurable LCD brightness control via web portal"
- [x] Breaking changes include migration guide if applicable - No breaking changes, backward compatible (defaults to 100% on upgrade)

**✅ All Constitution Checks PASS** - No violations requiring justification

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

```text
# LCD Brightness Control - Modified Files
src/
├── app/
│   ├── config_manager.h           # ADD: lcd_brightness field to DeviceConfig struct
│   ├── config_manager.cpp         # MODIFY: load/save/print brightness setting
│   ├── lcd_driver.h               # EXISTS: lcd_set_backlight() already defined
│   ├── lcd_driver.cpp             # EXISTS: PWM implementation already present
│   ├── display_manager.cpp        # MODIFY: Initialize brightness from config on boot
│   ├── web_portal.cpp             # MODIFY: Add /api/brightness handlers (GET/POST)
│   ├── web_portal.h               # ADD: Forward declarations for brightness handlers
│   ├── web_assets.h               # AUTO-GENERATED: Rebuilt by minify script
│   └── web/
│       ├── home.html              # MODIFY: Add "Display Settings" section with slider
│       └── portal.js              # MODIFY: Add brightness slider event handlers
├── version.h                      # MODIFY: Bump MINOR version
└── (no changes to boards/)        # Feature uses existing GPIO abstraction

# Build artifacts (auto-generated)
build/                             # Clean rebuild required after changes
├── esp32/esp32.bin
├── esp32c3/esp32c3.bin
└── esp32c6/esp32c6.bin

# Documentation updates
CHANGELOG.md                       # ADD: New feature entry
docs/                              # (Optional) Add brightness configuration guide
```

**Structure Decision**: Feature integrates into existing configuration management pattern. Brightness is a new field in `DeviceConfig` struct (config_manager.h), persisted to NVS alongside WiFi/MQTT settings. Web UI adds one section to home.html, one API endpoint to web_portal.cpp. Hardware layer (`lcd_set_backlight`) already exists, no new drivers needed.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

**✅ NO VIOLATIONS** - All Constitution principles satisfied without exceptions.

---

## Phase Summary

### Phase 0: Research (COMPLETE)

**Outputs**:
- [research.md](research.md) - 6 research tasks completed, all technical unknowns resolved

**Key Findings**:
- Hardware layer already exists (`lcd_set_backlight()` validated 0-100% PWM control)
- NVS storage pattern identified (add uint8_t field to DeviceConfig, default 100%)
- REST API design following existing `/api/*` patterns
- Web UI integration via range slider with `input` event for real-time updates
- Configuration manager integration via `portal.js` fields array

**Blockers**: ✅ None

---

### Phase 1: Design & Contracts (COMPLETE)

**Outputs**:
- [data-model.md](data-model.md) - Brightness entity, storage schema, data flows, validation rules
- [contracts/api-brightness.md](contracts/api-brightness.md) - REST API specification with examples
- [quickstart.md](quickstart.md) - Step-by-step implementation guide with code snippets
- `.github/agents/copilot-instructions.md` - Updated with new technology context

**Design Decisions**:
1. **Separation of Concerns**: `/api/brightness` handles real-time updates, `/api/config` handles persistence
2. **Backward Compatibility**: Default 100% brightness for devices upgrading from older firmware
3. **Single Source of Truth**: `current_brightness` global variable tracks runtime state (may differ from NVS)
4. **No New Dependencies**: Uses existing libraries (ESP Async WebServer, ArduinoJson, LVGL)

**Constitution Re-check**: ✅ All gates pass, no violations introduced by design

**Ready for**: Phase 2 (Task Breakdown) via `/speckit.tasks` command

---

## Notes

- Implementation estimated at **~2.5 hours** total (7 steps in quickstart)
- No breaking changes (MINOR version bump, backward compatible)
- Feature adds minimal overhead: ~1 byte NVS, ~500 bytes flash (gzipped HTML/JS), <1KB heap
- Manual hardware testing required (ESP32-C3 with 1.69" LCD recommended)
- CI will validate compilation on all board targets (ESP32, ESP32-C3, ESP32-C6)
