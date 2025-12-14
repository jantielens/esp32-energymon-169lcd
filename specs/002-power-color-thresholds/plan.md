# Implementation Plan: Configurable Power Color Thresholds

**Branch**: `002-power-color-thresholds` | **Date**: December 13, 2025 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/002-power-color-thresholds/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

Enable users to customize power screen color-coding thresholds for grid, home, and solar power displays via the web portal. Hard-coded threshold values in `screen_power.cpp` (e.g., grid <0.5kW=white, <2.5kW=orange, >=2.5kW=red) will be replaced with configurable values stored in NVS and editable through the Home page web interface. This allows households with different energy consumption patterns to personalize visual feedback thresholds.

## Technical Context

**Language/Version**: C++ (Arduino framework for ESP32)  
**Primary Dependencies**: LVGL 8.3.0, ESPAsyncWebServer 1.2.3, ArduinoJson 6.21.3, NVS (built-in ESP32)  
**Storage**: NVS (Non-Volatile Storage) for configuration persistence via DeviceConfig struct  
**Testing**: Build verification via `./build.sh` (all boards must compile), manual hardware testing on ESP32/ESP32-C3  
**Target Platform**: ESP32/ESP32-C3/ESP32-C6 microcontrollers (Arduino framework)  
**Project Type**: Embedded firmware (Arduino sketch structure in `src/app/`)  
**Performance Goals**: 60 FPS display maintained, <1s latency from web portal save to display update, <100 bytes flash increase  
**Constraints**: 48KB heap budget, no blocking operations in LVGL callbacks, 0.1kW input precision, max 10kW threshold values  
**Scale/Scope**: Single device, 13 new configuration fields (9 thresholds + 4 colors), 1 REST API endpoint, Home page UI additions

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**Hardware-Driven Design Compliance:**
- [x] Pin assignments documented for all supported boards (ESP32/ESP32-C3/ESP32-C6) - N/A, no new GPIO usage
- [x] Memory budget validated (heap, flash, PSRAM if applicable) - ~100 bytes NVS (7 floats), minimal heap impact
- [x] Hardware constraints identified (SPI speed, GPIO limitations, sensor accuracy) - No hardware changes, display refresh unchanged

**Configuration-First UX Compliance:**
- [x] Feature accessible via web portal (no serial console required) - Home page threshold input fields
- [x] Settings persisted to NVS with backward compatibility - DeviceConfig struct extended with default fallbacks
- [x] REST API endpoint(s) defined if programmatic access needed - Existing /api/config handles threshold fields
- [x] Mobile-responsive UI design (<768px breakpoint) - Standard form layout matches existing portal CSS

**Build Automation Compliance:**
- [x] Changes compile on all board targets in `config.sh` `FQBN_TARGETS` - No board-specific code introduced
- [x] New library dependencies added to `arduino-libraries.txt` with version pin - No new library dependencies
- [x] Board-specific code isolated to `src/boards/[board]/board_overrides.h` if needed - Not required

**Embedded Best Practices Compliance:**
- [x] No dynamic allocation in interrupt handlers or LVGL callbacks - Threshold access via global config pointer (static)
- [x] No blocking operations in display flush or network callbacks - Config loaded at boot, LVGL reads cached values
- [x] WiFi reconnection uses non-blocking retry (5s+ backoff) - No WiFi code changes
- [x] Watchdog handling documented for long-running operations - No long operations introduced

**Semantic Versioning Compliance:**
- [x] Version bump type determined (MAJOR for breaking changes, MINOR for features, PATCH for fixes) - MINOR (new feature)
- [x] `CHANGELOG.md` entry prepared with Keep a Changelog format - Entry includes config migration (backward compatible)
- [x] Breaking changes include migration guide if applicable - Not breaking, defaults applied if fields missing

## Project Structure

### Documentation (this feature)

```text
specs/002-power-color-thresholds/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
│   └── api-thresholds.md
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
# ESP32 Arduino Firmware Structure
src/
├── app/
│   ├── app.ino                    # Main Arduino sketch - minimal changes (pass config to screen)
│   ├── config_manager.h           # ADD: 7 new float fields to DeviceConfig struct
│   ├── config_manager.cpp         # ADD: Default threshold initialization, validation
│   ├── screen_power.h             # ADD: Method to update thresholds at runtime
│   ├── screen_power.cpp           # MODIFY: Replace hardcoded thresholds with config lookups
│   ├── web_portal.cpp             # MODIFY: handleGetConfig/handlePostConfig for threshold fields
│   └── web/
│       ├── home.html              # ADD: Threshold input fields (grid/home/solar sections)
│       ├── portal.css             # MODIFY: Styling for threshold input groups (if needed)
│       └── portal.js              # MODIFY: Client-side validation for threshold ordering

# Modified Files
src/app/config_manager.h           # DeviceConfig struct extension
src/app/config_manager.cpp         # NVS load/save logic for thresholds
src/app/screen_power.h             # Threshold update method declaration
src/app/screen_power.cpp           # Color logic uses config thresholds
src/app/web_portal.cpp             # REST API JSON serialization for thresholds
src/app/web/home.html              # UI for threshold configuration
src/app/web/portal.js              # Client validation logic
src/version.h                      # Bump to next MINOR version

# Generated at Build Time
src/app/web_assets.h               # Auto-generated from web/*.html (minify script)
```

**Structure Decision**: Feature integrates into existing configuration system. DeviceConfig struct in config_manager.h holds threshold values, web_portal.cpp REST API exposes them via /api/config endpoint (already handles JSON serialization), home.html adds input fields, screen_power.cpp reads thresholds from global config. No new manager classes needed - extends existing subsystems.

## Complexity Tracking

> **No Constitution Violations** - Feature fully complies with all constitution principles.

This feature introduces no complexity exceptions or deviations from established patterns:
- Uses existing NVS configuration infrastructure
- Follows established web portal REST API patterns
- No blocking operations or dynamic allocations in display code
- Backward compatible NVS migration
- Compiles on all board targets without board-specific code
