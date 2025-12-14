# Tasks: Configurable Power Color Thresholds

**Feature**: 002-power-color-thresholds  
**Created**: December 13, 2025  
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/api-thresholds.md, quickstart.md

**Implementation Time Estimate**: 5-6 hours  
**Total Tasks**: 50  
**Files Modified**: 7

## Task Organization

Tasks are organized by user story to enable independent implementation and testing. Each user story phase includes all components needed to deliver that story independently (config storage, API, UI, display logic).

**Format**: `- [ ] [ID] [P?] [Story?] Description with file path`
- **[P]**: Parallelizable (different files, no dependencies)
- **[Story]**: User story label (US1-US5)
- File paths are absolute from repository root

---

## Phase 1: Setup & Infrastructure

**Purpose**: Initialize configuration storage and validation infrastructure shared across all user stories

**Dependencies**: None (can start immediately)

- [X] T001 Extend DeviceConfig struct with threshold and color fields in src/app/config_manager.h
- [X] T002 [P] Add factory default initialization for 9 thresholds + 4 colors in src/app/config_manager.cpp
- [X] T003 [P] Implement config_manager_validate() function with threshold ordering and range checks in src/app/config_manager.cpp
- [X] T004 [P] Add validation function declaration to src/app/config_manager.h

**Completion Criteria**:
- DeviceConfig struct has 9 float fields (grid_threshold[3], home_threshold[3], solar_threshold[3]) + 4 uint32_t fields (color_good, color_ok, color_attention, color_warning)
- Factory defaults match specification: Grid [0.0, 0.5, 2.5], Home [0.5, 1.0, 2.0], Solar [0.5, 1.5, 3.0], Colors [0x00FF00, 0xFFFFFF, 0xFFA500, 0xFF0000]
- Validation enforces: T[0] ≤ T[1] ≤ T[2] for all types, grid range [-10.0, +10.0], home/solar range [0.0, +10.0], colors ≤ 0xFFFFFF
- NVS storage increases by 52 bytes (verified via sizeof(DeviceConfig))

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core display and API infrastructure that all user stories depend on

**Dependencies**: Phase 1 must complete first

- [X] T005 Implement unified get_power_color() algorithm with RGB→BGR conversion in src/app/screen_power.cpp
- [X] T006 [P] Add rgb_to_bgr() and get_power_color() declarations to src/app/screen_power.h
- [X] T007 [P] Add parse_hex_color() helper function in src/app/web_portal.cpp
- [X] T008 Extend handleGetConfig() to serialize 9 threshold + 4 color fields to JSON in src/app/web_portal.cpp
- [X] T009 Extend handlePostConfig() to parse threshold/color fields and call validation in src/app/web_portal.cpp

**Completion Criteria**:
- get_power_color(value, thresholds[3]) returns correct lv_color_t for all 4 zones (good/ok/attention/warning)
- RGB→BGR conversion verified (0xFF0000 input → 0x0000FF display output)
- GET /api/config returns grid_threshold_0..2, home_threshold_0..2, solar_threshold_0..2, color_good/ok/attention/warning
- POST /api/config accepts threshold/color fields, validates, and returns 400 on validation failure
- parse_hex_color("#RRGGBB") correctly converts to uint32_t (0x00RRGGBB)

---

## Phase 3: User Story 1 - Configure Grid Power Thresholds (P1)

**Purpose**: Enable configuration of grid power thresholds (including negative values for export)

**Dependencies**: Phase 2 must complete

**Independent Test**: Configure grid thresholds to [−1.0, 0.0, 2.0], set grid power to -0.5kW via MQTT, verify display shows green (good zone)

- [X] T010 [P] [US1] Add grid threshold input fields (3 number inputs) to src/app/web/home.html
- [X] T011 [P] [US1] Implement validateThresholds() JavaScript function with grid-specific validation (-10 to +10 range) in src/app/web/portal.js
- [X] T012 [P] [US1] Update loadConfig() to populate grid threshold fields from API response in src/app/web/portal.js
- [X] T013 [US1] Modify get_grid_color() to call get_power_color(grid_power, current_config->grid_threshold) in src/app/screen_power.cpp

**Acceptance Tests**:
1. Load web portal → verify grid threshold inputs show defaults [0.0, 0.5, 2.5]
2. Set grid thresholds to [-1.0, 0.0, 2.0] → save → verify POST /api/config succeeds
3. MQTT publish grid_power = -0.5kW → verify grid icon displays green
4. MQTT publish grid_power = 1.5kW → verify grid icon displays orange
5. Try saving grid thresholds [2.0, 1.0, 3.0] → verify client alert "T0 ≤ T1 ≤ T2"

---

## Phase 4: User Story 2 - Configure Home Power Thresholds (P1)

**Purpose**: Enable configuration of home consumption thresholds (non-negative only)

**Dependencies**: Phase 2 must complete (can run parallel with Phase 3)

**Independent Test**: Configure home thresholds to [0.8, 1.5, 2.5], set home power to 1.2kW via MQTT, verify display shows orange (attention zone)

- [X] T014 [P] [US2] Add home threshold input fields (3 number inputs) to src/app/web/home.html
- [X] T015 [P] [US2] Extend validateThresholds() with home-specific validation (0 to +10 range) in src/app/web/portal.js
- [X] T016 [P] [US2] Update loadConfig() to populate home threshold fields in src/app/web/portal.js
- [X] T017 [US2] Modify get_home_color() to call get_power_color(home_power, current_config->home_threshold) in src/app/screen_power.cpp

**Acceptance Tests**:
1. Load web portal → verify home threshold inputs show defaults [0.5, 1.0, 2.0]
2. Set home thresholds to [0.8, 1.5, 2.5] → save → verify POST /api/config succeeds
3. MQTT publish home_power = 1.2kW → verify home icon displays orange
4. MQTT publish home_power = 0.3kW → verify home icon displays green
5. Try saving home thresholds [-0.5, 1.0, 2.0] → verify client alert "0.0 to +10.0 kW"

---

## Phase 5: User Story 3 - Configure Solar Power Thresholds (P1)

**Purpose**: Enable configuration of solar generation thresholds (non-negative only)

**Dependencies**: Phase 2 must complete (can run parallel with Phases 3 & 4)

**Independent Test**: Configure solar thresholds to [1.0, 2.0, 4.0], set solar power to 3.5kW via MQTT, verify display shows orange (attention zone)

- [X] T018 [P] [US3] Add solar threshold input fields (3 number inputs) to src/app/web/home.html
- [X] T019 [P] [US3] Extend validateThresholds() with solar-specific validation (0 to +10 range) in src/app/web/portal.js
- [X] T020 [P] [US3] Update loadConfig() to populate solar threshold fields in src/app/web/portal.js
- [X] T021 [US3] Modify get_solar_color() to call get_power_color(solar_power, current_config->solar_threshold) in src/app/screen_power.cpp

**Acceptance Tests**:
1. Load web portal → verify solar threshold inputs show defaults [0.5, 1.5, 3.0]
2. Set solar thresholds to [1.0, 2.0, 4.0] → save → verify POST /api/config succeeds
3. MQTT publish solar_power = 3.5kW → verify solar icon displays orange
4. MQTT publish solar_power = 0.8kW → verify solar icon displays green
5. Try saving solar thresholds [2.0, 2.0, 1.0] → verify client alert "T0 ≤ T1 ≤ T2"

---

## Phase 6: User Story 4 - Configurable Colors (P1)

**Purpose**: Enable configuration of the 4 global color values (good/ok/attention/warning)

**Dependencies**: Phase 2 must complete (can run parallel with Phases 3, 4, 5)

**Independent Test**: Change color_good to #0000FF (blue), set grid power to -1.0kW, verify grid icon displays blue instead of green

- [X] T022 [P] [US4] Add 4 color picker inputs (<input type="color">) to src/app/web/home.html
- [X] T023 [P] [US4] Update loadConfig() to populate color fields from API response in src/app/web/portal.js
- [X] T024 [P] [US4] Update saveConfig() to include color fields in POST data in src/app/web/portal.js

**Acceptance Tests**:
1. Load web portal → verify color pickers show defaults [#00FF00, #FFFFFF, #FFA500, #FF0000]
2. Set color_good to #0000FF → save → reload → verify color_good still #0000FF
3. Set grid power to -1.0kW → verify grid icon displays blue (custom good color)
4. Set color_warning to #800080 (purple) → verify high power values display purple
5. Verify all 3 power types use the same configured colors (global application)

---

## Phase 7: User Story 5 - Restore Factory Defaults (P2)

**Purpose**: Add "Restore Defaults" button to reset all thresholds and colors

**Dependencies**: Phases 3, 4, 5, 6 must complete

**Independent Test**: Configure custom thresholds/colors, click "Restore Defaults", verify all values revert to factory defaults

- [X] T025 [P] [US5] Add "Restore Defaults" button to src/app/web/home.html
- [X] T026 [US5] Implement restoreDefaults() JavaScript function in src/app/web/portal.js that POSTs factory default values

**Acceptance Tests**:
1. Set custom thresholds and colors → click "Restore Defaults" → verify POST /api/config with factory values
2. Verify all threshold fields reset to: Grid [0.0, 0.5, 2.5], Home [0.5, 1.0, 2.0], Solar [0.5, 1.5, 3.0]
3. Verify all color fields reset to: [#00FF00, #FFFFFF, #FFA500, #FF0000]
4. Verify display immediately reflects factory defaults (no reboot needed)

---

## Phase 8: User Story 6 - Persistence Across Reboots (P1)

**Purpose**: Ensure NVS persistence of configured thresholds and colors

**Dependencies**: Phases 1, 2 must complete (foundational infrastructure)

**Independent Test**: Configure custom thresholds/colors, power cycle device, verify configuration persists

- [X] T027 [US6] Verify config_manager_save() persists extended DeviceConfig struct to NVS in src/app/config_manager.cpp
- [X] T028 [US6] Verify config_manager_load() applies factory defaults if loaded size < sizeof(DeviceConfig) in src/app/config_manager.cpp

**Acceptance Tests**:
1. Configure custom thresholds/colors → save → reboot device → verify GET /api/config returns custom values
2. Flash new firmware with extended DeviceConfig → verify old NVS config loads with factory defaults for new fields
3. Corrupt NVS config → reboot → verify device falls back to factory defaults (doesn't crash)
4. Verify configuration survives power loss (unplug device for 60 seconds, power on, check persistence)

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Final touches, documentation, and cross-feature validation

**Dependencies**: All user story phases (3-8) must complete

- [X] T029 [P] Update src/version.h to next MINOR version (e.g., 1.2.0 → 1.3.0)
- [X] T030 [P] Add CHANGELOG.md entry for configurable thresholds feature with migration notes
- [X] T031 [P] Run minify-web-assets.sh to regenerate src/app/web_assets.h from updated HTML/JS
- [X] T032 Verify ./build.sh compiles successfully for all board targets (ESP32, ESP32-C3, ESP32-C6)
- [X] T033 Test on physical hardware: Flash firmware, configure thresholds, verify display update <1s
- [X] T034 [P] Update README.md with threshold configuration usage instructions

**Completion Criteria**:
- Version bumped to MINOR (new feature, backward compatible)
- CHANGELOG.md documents: configurable thresholds, configurable colors, negative grid support, skip-level capability
- All boards compile without errors (`./build.sh` exit code 0)
- Physical test on ESP32: Configure thresholds via web portal, verify <1s display update, verify persistence after reboot
- Documentation includes: How to access threshold config, what each threshold controls, default values, validation rules

---

## Phase 10: Rolling Statistics Overlay (US6)

**Purpose**: Add 10-minute min/max visual indicators to power bars

**Dependencies**: Phase 9 must complete (core display working)

- [X] T035 [P] [US6] Implement PowerStatistics class with circular buffer (600 samples) in src/app/screen_power.cpp
- [X] T036 [P] [US6] Add min/max/avg calculation methods (recalculate, getMin, getMax, getAvg) to PowerStatistics class
- [X] T037 [P] [US6] Add three PowerStatistics instances (solar_stats, home_stats, grid_stats) as void* members in src/app/screen_power.h
- [X] T038 [P] [US6] Add six lv_obj_t* line members (solar/home/grid × min/max) in src/app/screen_power.h
- [X] T039 [P] [US6] Add six lv_point_t arrays for line coordinates in src/app/screen_power.h
- [X] T040 [US6] Instantiate PowerStatistics objects in PowerScreen::create() with new operator
- [X] T041 [P] [US6] Implement create_line() helper function for LVGL line object creation
- [X] T042 [US6] Create six line objects (min/max for each power type) in PowerScreen::create()
- [X] T043 [P] [US6] Implement update_statistics() method to sample power values every second
- [X] T044 [US6] Call update_statistics() from PowerScreen::update() based on 1-second timer
- [X] T045 [P] [US6] Implement position_line() helper to set line coordinates based on kW value
- [X] T046 [US6] Implement update_stat_overlays() method to position all six lines
- [X] T047 [US6] Call update_stat_overlays() from update_statistics() after sampling
- [X] T048 [US6] Clean up PowerStatistics objects in PowerScreen::destroy() with delete operator
- [X] T049 [US6] Remove debug Serial.printf statements from statistics code
- [X] T050 [US6] Test on hardware: Verify min/max lines appear, verify 10-minute rolling window, verify memory stability

**Completion Criteria**:
- PowerStatistics class correctly maintains 600-sample circular buffer
- Min/max lines appear on all three power bars at correct Y positions
- Lines extend 2px left and 5px right beyond bar edges
- Lines are 1px wide, white color, 70% opacity
- Memory usage increases by ~1.6 KB (measured via build output)
- Statistics reset on reboot (not persisted)
- Lines positioned using lv_obj_align + relative coordinates (not absolute)
- No debug output in production code

---

## Dependencies Diagram

```
Phase 1: Setup
    ↓
Phase 2: Foundational (blocking)
    ↓
    ├─→ Phase 3: US1 Grid Thresholds (P1) ─┐
    ├─→ Phase 4: US2 Home Thresholds (P1) ─┼─→ Phase 7: US5 Restore Defaults (P2)
    ├─→ Phase 5: US3 Solar Thresholds (P1) ┤       ↓
    ├─→ Phase 6: US4 Colors (P1) ──────────┘       ↓
    └─→ Phase 8: US6 Persistence (P1) ────────→ Phase 9: Polish
                                                      ↓
                                                 Phase 10: Statistics (US6)
```

**Critical Path**: Setup → Foundational → Any User Story → Polish (estimated 3-4 hours total)

**Parallel Opportunities**: 
- After Phase 2: Phases 3, 4, 5, 6, 8 can all run in parallel (independent user stories)
- Within each phase: Tasks marked [P] can run simultaneously

---

## Validation Checklist

Before marking feature complete, verify:

### NVS Migration
- [ ] Old firmware config loads successfully with new firmware (defaults applied)
- [ ] New firmware preserves existing config fields (hostname, MQTT broker, etc.)
- [ ] DeviceConfig struct size increase documented (52 bytes)

### Web Portal
- [ ] All 9 threshold fields + 4 color pickers visible on Home page
- [ ] Client validation prevents invalid threshold ordering
- [ ] Client validation prevents out-of-range values
- [ ] Invalid input preserved for user correction (not cleared)
- [ ] "Restore Defaults" button resets all 13 fields

### Display Update
- [ ] Grid icon color changes at configured thresholds (tested with MQTT)
- [ ] Home icon color changes at configured thresholds (tested with MQTT)
- [ ] Solar icon color changes at configured thresholds (tested with MQTT)
- [ ] Colors match configured RGB values (not hardcoded green/white/orange/red)
- [ ] Negative grid threshold works (export shows "good" color)
- [ ] Equal thresholds skip color levels (e.g., T0=T1 skips "ok" zone)

### API Contract
- [ ] GET /api/config returns all 13 new fields
- [ ] POST /api/config accepts partial updates (only changed fields)
- [ ] Server validation returns 400 Bad Request with specific error messages
- [ ] Color parsing rejects invalid hex formats

### Performance
- [ ] Display maintains 60 FPS (LVGL FPS counter)
- [ ] Configuration save → display update latency <1s
- [ ] No heap fragmentation after 10+ configuration saves
- [ ] Flash usage increase <500 bytes (measured via build output)

### Backward Compatibility
- [ ] Old configs auto-upgrade to new schema without user intervention
- [ ] Factory defaults match previous hardcoded behavior where applicable
- [ ] No breaking changes to existing /api/config response structure

### Edge Cases
- [ ] Thresholds [2.0, 1.0, 3.0] rejected (ordering violation)
- [ ] Grid threshold -15kW rejected (out of range)
- [ ] Home threshold -0.5kW rejected (negative not allowed)
- [ ] Color value 0xFFFFFFFF rejected (exceeds 24-bit RGB)
- [ ] All thresholds equal (T0=T1=T2) displays single color (intentional, not error)

---

## Implementation Strategy

### MVP (Minimum Viable Product)
Start with **Phase 3: User Story 1 - Grid Thresholds** as the MVP:
- Demonstrates end-to-end flow (config storage → API → UI → display)
- Validates architecture decisions (NVS schema, API contract, color algorithm)
- Delivers immediate user value (most requested use case: grid import/export visualization)
- Estimated: 1.5-2 hours

### Incremental Delivery
After MVP, deliver user stories in parallel:
1. **Week 1**: MVP (Grid thresholds) + Persistence (US6)
2. **Week 2**: Home (US2) + Solar (US3) + Colors (US4) in parallel
3. **Week 3**: Restore Defaults (US5) + Polish (Phase 9)

### Rollback Plan
If critical issues found post-deployment:
1. Revert to previous firmware (flash last release)
2. Old firmware ignores new NVS fields (magic number validation)
3. Users lose custom thresholds but device remains functional

---

## Summary

- **Total Tasks**: 50 (Setup: 4, Foundational: 5, User Stories: 19, Persistence: 2, Polish: 6, Statistics: 16)
- **Parallelizable**: 28 tasks marked [P] (56% can run concurrently)
- **Critical Path**: 22 sequential tasks (Setup → Foundational → 1 User Story → Polish → Statistics)
- **User Stories**: 6 (Grid, Home, Solar, Colors, Persistence, Statistics)
- **Files Modified**: 7 (config_manager.h/cpp, screen_power.h/cpp, web_portal.cpp, home.html, portal.js)
- **Estimated Time**: 5-6 hours (experienced developer, full implementation + testing + statistics)

**Ready for implementation** - All tasks have clear acceptance criteria and file paths.
