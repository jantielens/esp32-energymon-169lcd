# Implementation Plan: [FEATURE]

**Branch**: `[###-feature-name]` | **Date**: [DATE] | **Spec**: [link]
**Input**: Feature specification from `/specs/[###-feature-name]/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

[Extract from feature spec: primary requirement + technical approach from research]

## Technical Context

<!--
  ACTION REQUIRED: Replace the content in this section with the technical details
  for the project. The structure here is presented in advisory capacity to guide
  the iteration process.
-->

**Language/Version**: C++ (Arduino framework for ESP32)  
**Primary Dependencies**: [e.g., LVGL, PubSubClient, ArduinoJson, ESP Async WebServer or NEEDS CLARIFICATION]  
**Storage**: NVS (Non-Volatile Storage) for configuration persistence  
**Testing**: Build verification via `./build.sh` (all boards must compile), manual hardware testing  
**Target Platform**: ESP32/ESP32-C3/ESP32-C6 microcontrollers (Arduino framework)  
**Project Type**: Embedded firmware (Arduino sketch structure in `src/app/`)  
**Performance Goals**: [e.g., 60 FPS display, <2s MQTT latency, <85% flash usage or NEEDS CLARIFICATION]  
**Constraints**: [e.g., 48KB heap, SPI timing, non-blocking callbacks, WiFi reconnection or NEEDS CLARIFICATION]  
**Scale/Scope**: [e.g., single device deployment, 3 web portal pages, 5 MQTT topics or NEEDS CLARIFICATION]

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**Hardware-Driven Design Compliance:**
- [ ] Pin assignments documented for all supported boards (ESP32/ESP32-C3/ESP32-C6)
- [ ] Memory budget validated (heap, flash, PSRAM if applicable)
- [ ] Hardware constraints identified (SPI speed, GPIO limitations, sensor accuracy)

**Configuration-First UX Compliance:**
- [ ] Feature accessible via web portal (no serial console required)
- [ ] Settings persisted to NVS with backward compatibility
- [ ] REST API endpoint(s) defined if programmatic access needed
- [ ] Mobile-responsive UI design (<768px breakpoint)

**Build Automation Compliance:**
- [ ] Changes compile on all board targets in `config.sh` `FQBN_TARGETS`
- [ ] New library dependencies added to `arduino-libraries.txt` with version pin
- [ ] Board-specific code isolated to `src/boards/[board]/board_overrides.h` if needed

**Embedded Best Practices Compliance:**
- [ ] No dynamic allocation in interrupt handlers or LVGL callbacks
- [ ] No blocking operations in display flush or network callbacks
- [ ] WiFi reconnection uses non-blocking retry (5s+ backoff)
- [ ] Watchdog handling documented for long-running operations

**Semantic Versioning Compliance:**
- [ ] Version bump type determined (MAJOR for breaking changes, MINOR for features, PATCH for fixes)
- [ ] `CHANGELOG.md` entry prepared with Keep a Changelog format
- [ ] Breaking changes include migration guide if applicable

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
