# Feature Specification: Power Screen Enhancements

**Feature Branch**: `001-power-screen-enhancements`  
**Created**: December 13, 2025  
**Status**: Draft  
**Input**: User description: "power screen enhancements and fixes: grid power color coding, home power color coding, solar power color coding, custom PNG icons, and power bar charts"

## Clarifications

### Session 2025-12-13

- Q: When implementing color-coded indicators for power states, how should "bright green", "white", "orange", and "red" colors be defined? → A: Use exact hex color values in BGR format to account for display hardware (BGR panel with RGB→BGR swap in flush callback for anti-aliasing)
- Q: What should be the exact acceptable size range for the PNG icon files? → A: 48x48 pixels (matching current lv_font_montserrat_48 icon size)
- Q: Where should the bar charts be positioned relative to the icon/value/label groups? → A: Below the label text (icon → value → label → bar) maintaining current vertical layout, accounting for 20px hardware Y offset
- Q: Where should the PNG icon source files be stored in the repository? → A: In a dedicated assets/icons/ directory at repository root
- Q: When power values fluctuate rapidly (e.g., every second), how should color and bar chart updates be handled to prevent flickering? → A: No delay needed - updates occur max 1 per second
- Q: What tool should be used for PNG to LVGL conversion? → A: Custom Python script (tools/png2icons.py) using Pillow library, generating .c files for Arduino build system compatibility
- Q: Should arrows between icons be color-coded? → A: Yes, left arrow follows solar color, right arrow follows grid color, both positioned at Y=25 (10px below icons)

## User Scenarios & Testing *(mandatory)*

<!--
  IMPORTANT: User stories should be PRIORITIZED as user journeys ordered by importance.
  Each user story/journey must be INDEPENDENTLY TESTABLE - meaning if you implement just ONE of them,
  you should still have a viable MVP (Minimum Viable Product) that delivers value.
  
  Assign priorities (P1, P2, P3, etc.) to each story, where P1 is the most critical.
  Think of each story as a standalone slice of functionality that can be:
  - Developed independently
  - Tested independently
  - Deployed independently
  - Demonstrated to users independently
-->

### User Story 1 - Visual Power Flow Understanding (Priority: P1)

Users monitoring their home energy system need to quickly understand the power flow status at a glance without reading specific numbers. The display shows power values with color-coded visual indicators that communicate the operational state of grid, home, and solar power systems.

**Why this priority**: Core functionality that enables immediate situational awareness of energy system status, which is the primary purpose of the energy monitor display.

**Independent Test**: Can be fully tested by displaying various power values and verifying that colors change according to defined thresholds, delivering immediate visual feedback about energy system state.

**Acceptance Scenarios**:

1. **Given** grid power is negative (exporting to grid), **When** the power screen displays, **Then** the grid icon, power value, and label appear in bright green
2. **Given** grid power is 0.5kW (importing from grid, low consumption), **When** the power screen displays, **Then** the grid icon, power value, and label appear in white
3. **Given** grid power is 1.8kW (importing from grid, medium consumption), **When** the power screen displays, **Then** the grid icon, power value, and label appear in orange
4. **Given** grid power is 2.7kW (importing from grid, high consumption), **When** the power screen displays, **Then** the grid icon, power value, and label appear in red
5. **Given** home power consumption is 0.3kW (low consumption), **When** the power screen displays, **Then** the home icon, power value, and label appear in bright green
6. **Given** home power consumption is 0.8kW (moderate consumption), **When** the power screen displays, **Then** the home icon, power value, and label appear in white
7. **Given** home power consumption is 1.5kW (elevated consumption), **When** the power screen displays, **Then** the home icon, power value, and label appear in orange
8. **Given** home power consumption is 2.2kW (high consumption), **When** the power screen displays, **Then** the home icon, power value, and label appear in red
9. **Given** solar power generation is 0.3kW (low generation), **When** the power screen displays, **Then** the solar icon, power value, and label appear in bright white
10. **Given** solar power generation is 1.2kW (good generation), **When** the power screen displays, **Then** the solar icon, power value, and label appear in bright green

---

### User Story 2 - Enhanced Visual Clarity with Custom Icons (Priority: P2)

Users viewing the power screen experience improved visual clarity through custom-designed icons that are more intuitive and aesthetically pleasing than the standard LVGL icons currently in use.

**Why this priority**: While color coding provides the critical feedback (P1), better icons improve user experience and professional appearance. This can be implemented after color coding is working.

**Independent Test**: Can be fully tested by building the system with PNG icon files and verifying they display correctly on the screen, delivering improved visual aesthetics without requiring color coding functionality.

**Acceptance Scenarios**:

1. **Given** PNG icon files for solar, home, and grid are present in the repository, **When** the build process runs, **Then** the PNG files are automatically converted to LVGL-compatible arrays
2. **Given** PNG icon files exceed maximum allowed dimensions, **When** the build process runs, **Then** the build fails with a clear error message indicating size constraints
3. **Given** PNG icon files are below minimum required dimensions, **When** the build process runs, **Then** the build fails with a clear error message indicating size constraints
4. **Given** the system is running with converted PNG icons, **When** the power screen displays, **Then** the custom solar, home, and grid icons appear instead of default LVGL icons

---

### User Story 3 - Proportional Power Visualization (Priority: P3)

Users can visually compare the relative magnitude of grid, home, and solar power values through bar charts that provide an intuitive graphical representation, making it easier to understand power distribution without mentally processing numerical values.

**Why this priority**: Adds nice-to-have visual enhancement after core color coding (P1) and icons (P2) are working. Provides additional insight but not critical for basic monitoring.

**Independent Test**: Can be fully tested by displaying various power values and verifying bars scale correctly from 0-3kW, delivering proportional visual feedback independent of other features.

**Acceptance Scenarios**:

1. **Given** grid power is 1.5kW, **When** the power screen displays, **Then** a bar chart under the grid icon shows a bar filled to 50% of maximum (1.5kW out of 3kW max)
2. **Given** home power is 0kW, **When** the power screen displays, **Then** a bar chart under the home icon shows an empty bar (0% filled)
3. **Given** solar power is 3kW or more, **When** the power screen displays, **Then** a bar chart under the solar icon shows a completely filled bar (100% filled)
4. **Given** grid power is negative, **When** the power screen displays, **Then** the bar chart under the grid icon shows the absolute value of the power (e.g., -1kW displays as 1kW on the bar)
5. **Given** all three power values are displayed, **When** the power screen updates, **Then** all three bar charts maintain the same 0-3kW scale for consistent comparison

---

### Edge Cases

- What happens when power values update at maximum rate (1 update per second)? Colors and bars should update immediately without delay since update frequency is already limited.
- What happens when grid power is exactly at a threshold boundary (e.g., exactly 1.0kW)? The system should use the color for the lower range (white in this case, since >=0 <1kW).
- What happens when power values are unavailable or invalid (e.g., MQTT disconnected)? Display should show an error state or last known good value with appropriate visual indication.
- What happens when solar power is 0kW at night? Should display white (#FFFFFF) icon/text with empty bar chart.
- What happens when PNG files are missing during build? Build should fail with clear error indicating which icon files are missing.
- What happens when PNG files are in wrong format (e.g., JPEG instead of PNG)? Build should fail with format validation error.

## Requirements *(mandatory)*

### Functional Requirements

#### Grid Power Color Coding
- **FR-001**: System MUST display grid power icon, value, and label in bright green (#00FF00 or equivalent) when grid power is less than 0 (exporting to grid)
- **FR-002**: System MUST display grid power icon, value, and label in white (#FFFFFF or equivalent) when grid power is greater than or equal to 0 and less than 1kW
- **FR-003**: System MUST display grid power icon, value, and label in orange (#FFA500 or equivalent) when grid power is greater than or equal to 1kW and less than 2.5kW
- **FR-004**: System MUST display grid power icon, value, and label in red (#FF0000 or equivalent) when grid power is greater than or equal to 2.5kW

#### Home Power Color Coding
- **FR-005**: System MUST display home power icon, value, and label in bright green (#00FF00 or equivalent) when home power is less than 0.5kW
- **FR-006**: System MUST display home power icon, value, and label in white (#FFFFFF or equivalent) when home power is greater than or equal to 0.5kW and less than 1kW
- **FR-007**: System MUST display home power icon, value, and label in orange (#FFA500 or equivalent) when home power is greater than or equal to 1kW and less than 2kW
- **FR-008**: System MUST display home power icon, value, and label in red (#FF0000 or equivalent) when home power is greater than or equal to 2kW

#### Solar Power Color Coding
- **FR-009**: System MUST display solar power icon, value, and label in bright white (#FFFFFF or equivalent) when solar power is less than 0.5kW
- **FR-010**: System MUST display solar power icon, value, and label in bright green (#00FF00 or equivalent) when solar power is greater than or equal to 0.5kW

#### Custom Icons
- **FR-011**: Repository MUST contain three PNG icon files (48x48 pixels) in the assets/icons/ directory representing solar, home, and grid power sources (sun.png, home.png, grid.png)
- **FR-012**: Build process MUST automatically convert PNG icon files from assets/icons/ to LVGL-compatible C arrays using tools/png2icons.py
- **FR-013**: Build process MUST validate PNG icon dimensions are exactly 48x48 pixels and fail build if dimensions are incorrect
- **FR-014**: System MUST display converted custom PNG icons with recoloring support on the power screen
- **FR-015**: Generated icon files (icons.c, icons.h) MUST be excluded from git via .gitignore

#### Power Bar Charts
- **FR-016**: System MUST display a vertical bar chart positioned below each power label at Y=140 (accounting for 20px hardware offset, ending at Y=240)
- **FR-017**: Each bar chart MUST be 12px wide and 100px tall
- **FR-018**: Each bar chart MUST visualize the absolute value of the corresponding power measurement
- **FR-019**: All bar charts MUST use a consistent scale with minimum value of 0kW and maximum value of 3kW
- **FR-020**: Bar charts MUST update their fill level proportionally to the power value (e.g., 1.5kW shows 50% fill)
- **FR-021**: Bar charts MUST be color-coded to match their corresponding power state indicators

#### Arrow Indicators
- **FR-022**: Left arrow (solar→home) MUST be positioned at Y=25 and color-coded to match solar power state
- **FR-023**: Right arrow (home←grid) MUST be positioned at Y=25 and color-coded to match grid power state

### Key Entities

- **Power Measurement**: Represents real-time power data for grid, home, and solar systems, including numeric value in kilowatts and timestamp
- **Display Element**: Represents a visual component on screen (icon, text label, numeric value, bar chart) with associated color and position
- **Color Threshold**: Defines power value ranges and their corresponding display colors for each power type
- **Icon Asset**: Represents source PNG image files and converted LVGL array data for visual icons

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can identify power flow state (exporting, low import, medium import, high import) within 1 second by glancing at color-coded display
- **SC-002**: Users can compare relative power levels across solar, home, and grid systems without reading numeric values, using only bar chart proportions
- **SC-003**: Display color updates occur within 500ms of receiving new power data from MQTT
- **SC-004uses ST7789V2 controller in BGR mode with RGB→BGR swap in flush callback for correct anti-aliasing
- Display has sufficient color depth to render bright green, white, orange, and red colors distinctly
- LVGL framework version 8.4.0 supports vertical bar chart widgets and custom image recoloring
- PNG icon files will be sized at 48x48 pixels to match current LVGL symbol font size (lv_font_montserrat_48)
- Build process has access to Python 3 with Pillow library for PNG-to-LVGL conversion
- Arduino build system treats .c files differently than .cpp files for proper linkage
- Display refresh rate is sufficient to show color transitions without noticeable delay at 1Hz update rate
- Users are monitoring residential energy systems with typical power ranges (solar up to 5kW, grid up to 10kW, home consumption up to 10kW)
- Negative grid power indicates export to grid (selling excess solar)
- Bar charts use vertical orientation with bottom-to-top fill direction
- PNG icon source files will be stored in assets/icons/ directory at repository root
- Hardware Y offset of 20px must be accounted for in positioning calculationse (lv_font_montserrat_48)
- Build process has access to PNG-to-LVGL conversion tools (e.g., LVGL image converter utility)
- Display refresh rate is sufficient to show color transitions without noticeable delay at 1Hz update rate
- Users are monitoring residential energy systems with typical power ranges (solar up to 5kW, grid up to 10kW, home consumption up to 10kW)
- Negative grid power indicates export to grid (selling excess solar)
- Bar charts use vertical orientation with bottom-to-top fill direction
- PNG icon source files will be stored in assets/icons/ directory at repository root

## Dependencies *(mandatversion 8.4.0 for rendering display elements
- Python 3 with Pillow library for PNG to LVGL conversion during build
- Arduino build system (arduino-cli) with ESP32 board support
- Existing power screen display infrastructure that can be enhanced with new features
- ST7789V2 LCD driver with BGR color mode and RGB→BGR flush callback
- LVGL graphics library for rendering display elements
- Build toolchain capable of processing PNG files and generating C/C++ array data
- Existing power screen display infrastructure that can be enhanced with new features

## Out of Scope

- Historical power data visualization (trends, graphs over time)
- User configuration of color thresholds via settings menu
- Animated transitions between color states
- Audio alerts for high power consumption
- Touchscreen interaction with power display elements
- Power prediction or forecasting features
- Alternative icon styles or themes
- Export of power data to external systems
- Multiple screen layouts or user-customizable views
- Support for battery storage power visualization (only solar, home, grid)
