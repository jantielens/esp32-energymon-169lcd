# Tasks: LCD Backlight Brightness Control

**Feature**: 001-lcd-brightness-control  
**Input**: Design documents from `/specs/001-lcd-brightness-control/`  
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/api-brightness.md, quickstart.md

**Tests**: Not requested in specification - implementation only

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `- [ ] [ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and workspace preparation

- [ ] T001 Verify on correct feature branch `001-lcd-brightness-control`
- [ ] T002 [P] Clean build artifacts with `./clean.sh`
- [ ] T003 [P] Verify existing hardware layer `lcd_set_backlight()` in src/app/lcd_driver.cpp

**Checkpoint**: Development environment ready

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core configuration infrastructure that MUST be complete before ANY user story implementation

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T004 Add `uint8_t lcd_brightness;` field to DeviceConfig struct in src/app/config_manager.h
- [ ] T005 Add NVS key constant `#define KEY_LCD_BRIGHTNESS "lcd_bright"` in src/app/config_manager.cpp
- [ ] T006 Implement NVS load for brightness in `config_manager_load()` function in src/app/config_manager.cpp
- [ ] T007 Implement NVS save for brightness in `config_manager_save()` function in src/app/config_manager.cpp
- [ ] T008 Add brightness to debug output in `config_manager_print()` function in src/app/config_manager.cpp
- [ ] T009 Verify build compiles: `./build.sh esp32c3`

**Checkpoint**: Configuration management foundation ready - user story implementation can now begin

---

## Phase 3: User Story 1 - Adjust LCD Brightness in Real-Time (Priority: P1) 🎯 MVP

**Goal**: Users can adjust LCD brightness via web portal slider with immediate hardware feedback (0-100%, real-time updates within 100ms)

**Independent Test**: Access home page, move brightness slider, observe LCD backlight intensity change in real-time

### Backend API Implementation for User Story 1

- [ ] T010 [P] [US1] Add forward declaration for `handleGetBrightness()` in src/app/web_portal.h
- [ ] T011 [P] [US1] Add forward declaration for `handlePostBrightness()` in src/app/web_portal.h
- [ ] T012 [US1] Add global variable `static uint8_t current_brightness = 100;` in src/app/web_portal.cpp
- [ ] T013 [US1] Implement `handleGetBrightness()` function in src/app/web_portal.cpp
- [ ] T014 [US1] Implement `handlePostBrightness()` function with JSON parsing and validation in src/app/web_portal.cpp
- [ ] T015 [US1] Register GET `/api/brightness` endpoint in `web_portal_init()` function in src/app/web_portal.cpp
- [ ] T016 [US1] Register POST `/api/brightness` endpoint in `web_portal_init()` function in src/app/web_portal.cpp
- [ ] T017 [US1] Verify build compiles: `./build.sh esp32c3`

### Frontend UI Implementation for User Story 1

- [ ] T018 [P] [US1] Add "Display Settings" section with brightness slider to src/app/web/home.html
- [ ] T019 [P] [US1] Add brightness value display span `<span id="lcd_brightness_value">` to src/app/web/home.html
- [ ] T020 [US1] Add `lcd_brightness` field to fields array in src/app/web/portal.js
- [ ] T021 [US1] Add real-time brightness slider `input` event listener in src/app/web/portal.js
- [ ] T022 [US1] Implement fetch POST to `/api/brightness` in slider event handler in src/app/web/portal.js
- [ ] T023 [US1] Add page load brightness fetch from `/api/brightness` in src/app/web/portal.js
- [ ] T024 [US1] Rebuild web assets: `./tools/minify-web-assets.sh`
- [ ] T025 [US1] Verify build compiles: `./build.sh esp32c3`

### Deployment & Testing for User Story 1

- [ ] T026 [US1] Upload firmware to ESP32-C3: `./upload.sh esp32c3`
- [ ] T027 [US1] Test: Open web portal home page, verify "Display Settings" section appears
- [ ] T028 [US1] Test: Move slider to 50%, verify LCD backlight dims immediately
- [ ] T029 [US1] Test: Move slider to 0%, verify LCD backlight turns off
- [ ] T030 [US1] Test: Move slider to 100%, verify LCD backlight at maximum
- [ ] T031 [US1] Test: Rapidly move slider back and forth, verify no lag or flickering
- [ ] T032 [US1] Test: curl GET `/api/brightness`, verify returns current value
- [ ] T033 [US1] Test: curl POST `/api/brightness` with `{"brightness": 60}`, verify hardware updates

**Checkpoint**: User Story 1 complete and independently testable - real-time brightness control works

---

## Phase 4: User Story 2 - Persist Brightness Settings (Priority: P2)

**Goal**: Brightness settings persist across device reboots via existing save button integration

**Independent Test**: Adjust brightness, click save, reboot device, verify brightness level is restored

### Save Integration for User Story 2

- [ ] T034 [US2] Add brightness field handling in `handlePostConfig()` function in src/app/web_portal.cpp
- [ ] T035 [US2] Call `lcd_set_backlight()` when brightness saved via `/api/config` in src/app/web_portal.cpp
- [ ] T036 [US2] Update `current_brightness` global when saved via `/api/config` in src/app/web_portal.cpp
- [ ] T037 [US2] Verify build compiles: `./build.sh esp32c3`

### Boot Restoration for User Story 2

- [ ] T038 [US2] Initialize brightness from config on boot in src/app/app.ino or src/app/display_manager.cpp
- [ ] T039 [US2] Call `lcd_set_backlight(device_config.lcd_brightness)` after config load
- [ ] T040 [US2] Verify build compiles: `./build.sh esp32c3`

### Testing for User Story 2

- [ ] T041 [US2] Upload firmware: `./upload.sh esp32c3`
- [ ] T042 [US2] Monitor serial output: `./monitor.sh`
- [ ] T043 [US2] Test: Set brightness to 30%, click save button, wait for reboot
- [ ] T044 [US2] Test: After reboot, verify LCD backlight is at 30%
- [ ] T045 [US2] Test: Verify serial output shows "LCD Brightness: 30%"
- [ ] T046 [US2] Test: Refresh web page, verify slider position shows 30%
- [ ] T047 [US2] Test: Set brightness to 75% (don't save), refresh page, verify slider reverts to 30%
- [ ] T048 [US2] Test: Backward compatibility - flash old firmware, save config, upgrade, verify default 100%

**Checkpoint**: User Story 2 complete - persistence works, User Story 1 still functions independently

---

## Phase 5: User Story 3 - View Current Brightness Setting (Priority: P3)

**Goal**: When user opens web portal, slider displays current brightness level immediately

**Independent Test**: Open web portal, verify slider position matches actual LCD brightness

### Get Config Integration for User Story 3

- [ ] T049 [US3] Add `lcd_brightness` field to JSON response in `handleGetConfig()` function in src/app/web_portal.cpp
- [ ] T050 [US3] Add brightness loading to `loadConfig()` function in src/app/web/portal.js
- [ ] T051 [US3] Update brightness value display span when config loads in src/app/web/portal.js
- [ ] T052 [US3] Rebuild web assets: `./tools/minify-web-assets.sh`
- [ ] T053 [US3] Verify build compiles: `./build.sh esp32c3`

### Testing for User Story 3

- [ ] T054 [US3] Upload firmware: `./upload.sh esp32c3`
- [ ] T055 [US3] Test: Set brightness to 80%, save, reboot
- [ ] T056 [US3] Test: Open web portal, verify slider immediately shows 80%
- [ ] T057 [US3] Test: First boot (no saved config), verify slider shows 100% (default)
- [ ] T058 [US3] Test: Set brightness to 45% (don't save), close browser, reopen portal, verify slider shows last saved value
- [ ] T059 [US3] Test: curl GET `/api/config`, verify response includes `"lcd_brightness": 80`

**Checkpoint**: User Story 3 complete - all three user stories now fully functional and independently testable

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Version management, documentation, and final validation

- [ ] T060 [P] Bump MINOR version in src/version.h (e.g., 0.1.0 → 0.2.0)
- [ ] T061 [P] Add CHANGELOG.md entry: "Added user-configurable LCD brightness control via web portal"
- [ ] T062 Build all board targets: `./build.sh` (no arguments = all boards)
- [ ] T063 Verify ESP32 build: `ls build/esp32/esp32.bin`
- [ ] T064 Verify ESP32-C3 build: `ls build/esp32c3/esp32c3.bin`
- [ ] T065 Verify ESP32-C6 build: `ls build/esp32c6/esp32c6.bin`
- [ ] T066 Run final end-to-end test on ESP32-C3 hardware following quickstart.md testing section
- [ ] T067 Commit changes: `git add -A && git commit -m "Add LCD brightness control feature"`
- [ ] T068 Push branch: `git push origin 001-lcd-brightness-control`
- [ ] T069 Create pull request to main branch
- [ ] T070 Wait for GitHub Actions CI to pass on all board targets

**Checkpoint**: Feature complete, tested, and ready for code review

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup (Phase 1) - BLOCKS all user stories
- **User Stories (Phase 3-5)**: All depend on Foundational (Phase 2) completion
  - Can proceed in priority order: US1 (P1) → US2 (P2) → US3 (P3)
  - Or US1 must complete before US2/US3 (since they build on US1's API implementation)
- **Polish (Phase 6)**: Depends on all user stories (Phases 3-5) being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational (Phase 2) - No dependencies on other stories
- **User Story 2 (P2)**: Depends on US1 backend API (T013-T016) - Extends with save/restore functionality
- **User Story 3 (P3)**: Depends on US2 persistence (T034-T040) - Adds config load integration

### Within Each User Story

**User Story 1**:
- Backend API tasks (T010-T017) can proceed independently
- Frontend UI tasks (T018-T025) can proceed in parallel with backend
- Deployment (T026) requires both backend and frontend complete
- Testing (T027-T033) must follow deployment

**User Story 2**:
- Save integration (T034-T037) depends on US1 API handlers existing
- Boot restoration (T038-T040) can run in parallel with save integration
- Testing (T041-T048) requires both save and boot complete

**User Story 3**:
- Get config integration (T049-T053) builds on US2 persistence
- Testing (T054-T059) requires integration complete

### Parallel Opportunities

**Phase 1 (Setup)**:
- T002 and T003 can run in parallel

**Phase 2 (Foundational)**:
- All tasks must run sequentially (each builds on previous)

**Phase 3 (User Story 1)**:
- T010-T011 (forward declarations) can run in parallel
- T018-T019 (HTML changes) can run in parallel
- Backend (T010-T017) and Frontend (T018-T025) can run in parallel by different developers
- Testing tasks (T027-T033) can run in any order after deployment

**Phase 4 (User Story 2)**:
- T034-T037 (save integration) and T038-T040 (boot restoration) can run in parallel
- Testing tasks (T043-T048) can run in any order after deployment

**Phase 5 (User Story 3)**:
- T049-T053 must run sequentially
- Testing tasks (T055-T059) can run in any order after deployment

**Phase 6 (Polish)**:
- T060 and T061 can run in parallel
- T063-T065 (build verification) can run in parallel

---

## Parallel Example: User Story 1

If two developers are working on User Story 1:

```bash
# Developer A: Backend API
T010-T017 (Backend implementation)

# Developer B: Frontend UI (simultaneously)
T018-T025 (Frontend implementation)

# Both developers: Coordinate deployment
T026 (Upload firmware - requires both A and B complete)

# Either developer: Testing
T027-T033 (Can split testing tasks)
```

**Timeline**: ~1.5 hours instead of 2.5 hours (40% time savings)

---

## Implementation Strategy

### MVP-First Approach

**Minimum Viable Product = User Story 1 only**:
- Tasks T001-T033 deliver core value: real-time brightness control
- Can release to users after Phase 3 if persistence is not critical
- Estimated time: ~1.5 hours

**Full Feature = All Three User Stories**:
- Tasks T001-T070 deliver complete feature with persistence and state visibility
- Recommended for production release
- Estimated time: ~2.5 hours

### Incremental Delivery

1. **Sprint 1**: Phases 1-3 (Setup + Foundational + US1) → MVP release
2. **Sprint 2**: Phase 4 (US2) → Add persistence
3. **Sprint 3**: Phases 5-6 (US3 + Polish) → Complete feature

### Risk Mitigation

- Verify build after each user story phase (T017, T037, T053)
- Test on hardware after each user story (T026-T033, T041-T048, T054-T059)
- Run full multi-board build before creating PR (T062-T065)
- Backward compatibility explicitly tested (T048)

---

## Task Summary

**Total Tasks**: 70
- Setup: 3 tasks
- Foundational: 6 tasks (BLOCKING)
- User Story 1 (P1): 24 tasks (~1.5 hours)
- User Story 2 (P2): 15 tasks (~0.7 hours)
- User Story 3 (P3): 11 tasks (~0.3 hours)
- Polish: 11 tasks

**Parallel Tasks**: 11 tasks marked [P]
**Critical Path**: ~2.5 hours (sequential execution)
**With Parallelism**: ~1.5 hours (2 developers)

**Files Modified**: 8 files
- Backend: config_manager.h, config_manager.cpp, web_portal.h, web_portal.cpp, app.ino
- Frontend: home.html, portal.js
- Version: version.h

**New Code**: ~200 lines (including comments)
**Tests**: 0 (not requested in specification)