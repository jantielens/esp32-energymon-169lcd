# Tasks: Power Screen Enhancements

**Input**: Design documents from `/specs/001-power-screen-enhancements/`  
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/api.md, quickstart.md

**Feature Summary**: Enhance power monitoring screen with color-coded visual indicators (P1), custom PNG icons (P2), and vertical bar charts (P3) for improved energy system status awareness.

**Tests**: Not requested - manual hardware testing only (build verification + visual validation on 240x280 ST7789V2 display).

**Organization**: Tasks grouped by user story (P1→P2→P3) to enable independent implementation and testing.

## Format: `- [ ] [ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: User story this task belongs to (US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and basic structure

- [X] T001 Create feature branch `001-power-screen-enhancements` from main
- [X] T002 [P] Verify LVGL@8.4.0 is installed in arduino-libraries.txt
- [X] T003 [P] Verify build tools work: Run `./build.sh esp32c3` to confirm baseline compiles

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T004 Read existing PowerScreen implementation in src/app/screen_power.cpp (lines 1-180)
- [X] T005 Read existing PowerScreen header in src/app/screen_power.h to understand widget structure
- [X] T006 Identify current TEXT_COLOR usage and widget creation pattern in PowerScreen::create()
- [X] T007 Verify current layout coordinates: icons at Y=15, values at Y=80, labels at Y=115

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel

---

## Phase 3: User Story 1 - Color-Coded Power Indicators (Priority: P1) 🎯 MVP

**Goal**: Visual power flow understanding through color-coded icons/values/labels

**Independent Test**: Display various power values (solar 0.3kW→2.5kW, grid -1kW→3kW, home 0.3kW→2.5kW) and verify colors change according to defined thresholds

### Implementation for User Story 1

- [X] T008 [P] [US1] Add 4 color constants (BRIGHT_GREEN, WHITE, ORANGE, RED) at file scope in src/app/screen_power.cpp
- [X] T009 [P] [US1] Add method declarations for get_grid_color(), get_home_color(), get_solar_color() to src/app/screen_power.h private section
- [X] T010 [P] [US1] Add method declaration for update_power_colors() to src/app/screen_power.h private section
- [X] T011 [US1] Implement get_grid_color(float kw) in src/app/screen_power.cpp with 4 threshold ranges (<0, <1kW, <2.5kW, ≥2.5kW)
- [X] T012 [US1] Implement get_home_color(float kw) in src/app/screen_power.cpp with 4 threshold ranges (<0.5kW, <1kW, <2kW, ≥2kW)
- [X] T013 [US1] Implement get_solar_color(float kw) in src/app/screen_power.cpp with 2 threshold ranges (<0.5kW, ≥0.5kW)
- [X] T014 [US1] Implement update_power_colors() in src/app/screen_power.cpp to apply colors to 9 widgets (3 icons, 3 values, 3 labels)
- [X] T015 [US1] Replace all TEXT_COLOR references with COLOR_WHITE in PowerScreen::create() in src/app/screen_power.cpp
- [X] T016 [US1] Add update_power_colors() call in set_solar_power() after value update in src/app/screen_power.cpp
- [X] T017 [US1] Add update_power_colors() call in set_grid_power() after value update in src/app/screen_power.cpp
- [X] T018 [US1] Add update_power_colors() call in update_home_value() after value calculation in src/app/screen_power.cpp
- [X] T019 [US1] Build for esp32c3: Run `./build.sh esp32c3` and verify no compilation errors
- [X] T020 [US1] Flash and test: Run `./upload.sh esp32c3 && ./monitor.sh` and verify colors change with power thresholds
- [X] T021 [US1] Validate P1 acceptance criteria: Grid <0→green, ≥0<1kW→white, ≥1kW<2.5kW→orange, ≥2.5kW→red
- [X] T022 [US1] Validate P1 acceptance criteria: Home <0.5kW→green, ≥0.5kW<1kW→white, ≥1kW<2kW→orange, ≥2kW→red
- [X] T023 [US1] Validate P1 acceptance criteria: Solar <0.5kW→white, ≥0.5kW→green
- [X] T024 [US1] Verify FPS counter shows 60 FPS maintained after color updates

**Checkpoint**: User Story 1 (P1) complete - color coding fully functional and testable independently

**Commit Point**: 
```bash
git add src/app/screen_power.cpp src/app/screen_power.h
git commit -m "feat: Add color-coded power indicators (P1)

- Implement grid/home/solar color thresholds
- Add color calculation helper methods
- Integrate color updates into power setters
- Colors: Green (low), White (normal), Orange (medium), Red (high)

Refs #001"
```

---

## Phase 4: User Story 2 - Custom PNG Icons (Priority: P2)

**Goal**: Enhanced visual clarity with custom-designed icons replacing LVGL symbols

**Independent Test**: Build with PNG icon files and verify they display correctly on screen, delivering improved aesthetics without requiring color coding

### Implementation for User Story 2

- [X] T025 [P] [US2] Create assets/icons/ directory at repository root
- [X] T026 [P] [US2] Add solar.png icon file (48×48 pixels) to assets/icons/ (sun.png)
- [X] T027 [P] [US2] Add home.png icon file (48×48 pixels) to assets/icons/
- [X] T028 [P] [US2] Add grid.png icon file (48×48 pixels) to assets/icons/
- [X] T029 [US2] Create tools/png2icons.py script with PNG validation and LVGL TRUE_COLOR_ALPHA conversion
- [X] T030 [US2] Make tools/png2icons.py executable: Run `chmod +x tools/png2icons.py`
- [X] T031 [US2] Add icon generation call to build.sh before compilation loop (before web assets)
- [X] T032 [US2] Add icons.h and icons.c to .gitignore (generated files)
- [X] T033 [US2] Test icon generation manually: Run `python3 tools/png2icons.py` and verify src/app/icons.{h,c} generated
- [X] T034 [US2] Add `#include "icons.h"` to src/app/screen_power.cpp after existing includes
- [X] T035 [US2] Replace solar_icon lv_label creation with lv_img widget in PowerScreen::create() in src/app/screen_power.cpp
- [X] T036 [US2] Set solar_icon source to &sun using lv_img_set_src() in src/app/screen_power.cpp
- [X] T037 [US2] Enable solar_icon recolor with lv_obj_set_style_img_recolor_opa(LV_OPA_COVER) in src/app/screen_power.cpp
- [X] T038 [US2] Replace home_icon lv_label creation with lv_img widget in PowerScreen::create() in src/app/screen_power.cpp
- [X] T039 [US2] Set home_icon source to &home using lv_img_set_src() in src/app/screen_power.cpp
- [X] T040 [US2] Enable home_icon recolor with lv_obj_set_style_img_recolor_opa(LV_OPA_COVER) in src/app/screen_power.cpp
- [X] T041 [US2] Replace grid_icon lv_label creation with lv_img widget in PowerScreen::create() in src/app/screen_power.cpp
- [X] T042 [US2] Set grid_icon source to &grid using lv_img_set_src() in src/app/screen_power.cpp
- [X] T043 [US2] Enable grid_icon recolor with lv_obj_set_style_img_recolor_opa(LV_OPA_COVER) in src/app/screen_power.cpp
- [X] T044 [US2] Update update_power_colors() to use lv_obj_set_style_img_recolor() for icons in src/app/screen_power.cpp
- [X] T045 [US2] Build with icon conversion: Run `./build.sh esp32c3` and verify icon conversion runs automatically
- [X] T046 [US2] Verify flash usage <85%: Check build output for partition usage (45% actual)
- [X] T047 [US2] Flash and test: Run `./upload.sh esp32c3 && ./monitor.sh` and verify custom PNG icons display
- [X] T048 [US2] Validate P2 acceptance criteria: PNG files converted automatically during build
- [X] T049 [US2] Validate P2 acceptance criteria: Build validates PNG dimensions (48x48 required)
- [X] T050 [US2] Validate P2 acceptance criteria: Custom icons appear with proper recoloring
- [X] T051 [US2] Verify icons still change colors dynamically (P1 functionality preserved)

**Checkpoint**: User Stories 1 AND 2 complete - both color coding and custom icons work independently

**Commit Point**:
```bash
git add assets/icons/*.png tools/convert-icons.sh src/app/icon_assets.* .gitignore build.sh src/app/screen_power.cpp
git commit -m "feat: Replace LVGL icons with custom PNG icons (P2)

- Add 48x48 PNG icons for solar/home/grid
- Create icon conversion build script
- Integrate conversion into build process
- Use image widgets with recoloring

Refs #001"
```

---

## Phase 5: User Story 3 - Vertical Power Bar Charts (Priority: P3)

**Goal**: Proportional power visualization through vertical bar charts for intuitive comparison

**Independent Test**: Display various power values (0kW, 1.5kW, 3kW, 4.5kW) and verify bars scale correctly from 0-3kW with bottom-to-top fill

### Implementation for User Story 3

- [X] T052 [P] [US3] Add solar_bar widget pointer to src/app/screen_power.h private section
- [X] T053 [P] [US3] Add home_bar widget pointer to src/app/screen_power.h private section
- [X] T054 [P] [US3] Add grid_bar widget pointer to src/app/screen_power.h private section
- [X] T055 [P] [US3] Add update_bar_charts() method declaration to src/app/screen_power.h private section
- [X] T056 [US3] Create solar_bar widget (12×100px) at Y=140, X=-107 in PowerScreen::create() in src/app/screen_power.cpp
- [X] T057 [US3] Configure solar_bar: set range to 0-3000, initial value 0 in src/app/screen_power.cpp
- [X] T058 [US3] Style solar_bar: dark gray background (0x333333), color-coded indicator in PowerScreen::create()
- [X] T059 [US3] Create home_bar widget (12×100px) at Y=140, X=0 in PowerScreen::create() in src/app/screen_power.cpp
- [X] T060 [US3] Configure home_bar: set range to 0-3000, initial value 0 in src/app/screen_power.cpp
- [X] T061 [US3] Style home_bar: dark gray background, color-coded indicator in PowerScreen::create()
- [X] T062 [US3] Create grid_bar widget (12×100px) at Y=140, X=107 in PowerScreen::create() in src/app/screen_power.cpp
- [X] T063 [US3] Configure grid_bar: set range to 0-3000, initial value 0 in src/app/screen_power.cpp
- [X] T064 [US3] Style grid_bar: dark gray background, color-coded indicator in PowerScreen::create()
- [X] T065 [US3] Implement update_bar_charts() with absolute value conversion in src/app/screen_power.cpp
- [X] T066 [US3] Update solar_bar value and color in update_bar_charts() in src/app/screen_power.cpp
- [X] T067 [US3] Update grid_bar value and color in update_bar_charts() in src/app/screen_power.cpp
- [X] T068 [US3] Update home_bar value and color in update_bar_charts() using calculated home power
- [X] T069 [US3] Call update_bar_charts() from update_power_colors() in src/app/screen_power.cpp
- [X] T070 [US3] Null solar_bar/home_bar/grid_bar pointers in PowerScreen::destroy()
- [X] T071 [US3] Add arrows color-coding: arrow1 follows solar, arrow2 follows grid
- [X] T072 [US3] Move arrows down 10px (Y=15 to Y=25)
- [X] T073 [US3] Build for esp32c3: Run `./build.sh esp32c3` and verify no compilation errors
- [X] T074 [US3] Flash and test: Run `./upload.sh esp32c3 && ./monitor.sh` and verify vertical bars appear
- [X] T075 [US3] Validate P3 acceptance criteria: Bar at 1.5kW shows 50% height
- [X] T076 [US3] Validate P3 acceptance criteria: Bar at 0kW shows empty
- [X] T077 [US3] Validate P3 acceptance criteria: Bar at 3kW+ shows 100% height (clamped)
- [X] T078 [US3] Validate P3 acceptance criteria: Negative grid power shows absolute value
- [X] T079 [US3] Validate P3 acceptance criteria: All 3 bars use same 0-3kW scale
- [X] T080 [US3] Verify FPS counter shows 60 FPS maintained with bar updates
- [X] T081 [US3] Verify bars positioned correctly (accounting for 20px hardware offset)

**Checkpoint**: All user stories (P1, P2, P3) complete - independently functional and testable

**Commit Point**:
```bash
git add src/app/screen_power.cpp src/app/screen_power.h
git commit -m "feat: Add vertical power bar charts (P3)

- Create vertical bar widgets below labels
- Implement 0-3kW proportional visualization  
- Use absolute values for negative grid power
- Clamp values >3kW to 100% height
- Bars extend upward from Y=120 (12x120px)

Refs #001"
```

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final integration testing, documentation, and release preparation

- [ ] T082 Clean build all boards: Run `./clean.sh && ./build.sh` to verify all boards compile
- [X] T083 Verify flash usage <85% for all boards (45% actual for esp32c3)
- [X] T084 Test power state scenarios with real MQTT data
- [X] T085 Verify colors update correctly with MQTT messages
- [X] T086 Verify custom icons with recoloring work correctly
- [X] T087 Verify bar charts scale proportionally (0-3kW range)
- [X] T088 Verify arrow color-coding (arrow1=solar, arrow2=grid)
- [X] T089 Update CHANGELOG.md with [Unreleased] entry for all features
- [X] T090 Update documentation (build-and-release-process.md, icon-generation.md)
- [ ] T091 Final hardware test with varied power scenarios
- [ ] T092 Create commit with all power screen enhancements

**Final Checkpoint**: Feature complete, tested, documented, ready to commit

---

## Dependencies Between User Stories

```
Phase 1: Setup
    ↓
Phase 2: Foundational (blocking)
    ↓
    ├─→ Phase 3: US1 (P1 - Color Coding) 🎯 MVP ← START HERE
    │   │
    │   └─→ Phase 4: US2 (P2 - Custom Icons) ← Can start after US1
    │       │
    │       └─→ Phase 5: US3 (P3 - Bar Charts) ← Can start after US1 (US2 optional)
    │
    └─→ Phase 6: Polish (after all user stories)
```

**Critical Path**: Phase 1 → Phase 2 → US1 (P1) → US2 (P2) → US3 (P3) → Polish

**Parallel Opportunities**:
- T008-T010: Color constant/method declaration can be done simultaneously (different sections of files)
- T025-T028: All 3 PNG icon files can be created in parallel
- T052-T054: All 3 bar widget pointers can be added in parallel (same file, different lines)
- T056-T064: All 3 bar widget creation blocks can be implemented in parallel (similar code patterns)

---

## Implementation Strategy

### MVP First (Minimum Viable Product)

**Recommended MVP Scope**: User Story 1 (P1) only
- Delivers immediate value: Color-coded visual feedback
- Independently testable and deployable
- Requires minimal dependencies (no build tools, no PNG assets)
- Estimated time: 2-3 hours

After MVP validation, add:
1. **P2 (Custom Icons)** - Improves aesthetics (1-2 hours)
2. **P3 (Bar Charts)** - Adds proportional visualization (1 hour)

### Incremental Delivery

Each user story can be:
- Developed independently
- Tested independently
- Deployed independently
- Demonstrated to users independently

**Example Deployment Sequence**:
1. Week 1: Ship P1 (color coding) → gather user feedback
2. Week 2: Ship P2 (custom icons) if feedback positive
3. Week 3: Ship P3 (bar charts) for power users who want detailed visualization

---

## Task Execution Checklist

Before starting tasks:
- [ ] Review all design documents (spec.md, plan.md, data-model.md, contracts/api.md)
- [ ] Understand existing PowerScreen implementation in src/app/screen_power.{cpp,h}
- [ ] Install required build tools (lv_img_conv, optional ImageMagick)
- [ ] Create feature branch and verify baseline builds

During implementation:
- [ ] Follow strict checklist format for task completion
- [ ] Test each user story independently before moving to next
- [ ] Commit after each user story completion (P1, P2, P3)
- [ ] Verify 60 FPS maintained after each phase
- [ ] Check flash usage stays <85% after each phase

After completion:
- [ ] All 11 validation matrix scenarios tested
- [ ] Performance targets met (60 FPS, <500ms latency, <85% flash)
- [ ] Documentation updated (CHANGELOG.md, version.h)
- [ ] Screenshots captured
- [ ] Pull request created with comprehensive summary

---

## Summary

**Total Tasks**: 90 tasks across 6 phases  
**User Stories**: 3 (P1: 17 tasks, P2: 27 tasks, P3: 29 tasks)  
**Parallel Opportunities**: ~15 tasks marked [P] can run in parallel  
**Estimated Time**: 4-6 hours total (P1: 2-3h, P2: 1-2h, P3: 1h, Polish: 1h)  
**MVP Scope**: Phase 3 (US1) - 17 tasks - 2-3 hours  
**Files Modified**: src/app/screen_power.{cpp,h}, build.sh, .gitignore, CHANGELOG.md, src/version.h  
**Files Created**: assets/icons/{solar,home,grid}.png, tools/convert-icons.sh, src/app/icon_assets.{h,c} (generated)  
**Build Tools**: lv_img_conv (npm), ImageMagick identify (optional)  
**Test Strategy**: Manual hardware testing on 240x280 ST7789V2 display (no automated tests)
