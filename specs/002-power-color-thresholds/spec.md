# Feature Specification: Configurable Power Color Thresholds

**Feature Branch**: `002-power-color-thresholds`  
**Created**: December 13, 2025  
**Status**: Draft  
**Input**: User description: "for the power screen, we currently have hard-coded the values for the color coding (like <2kw is red etc). I want to make these values for the 3 icons configurable via the web portal"

## Clarifications

### Session 2025-12-13

- Q: Where in the web portal should the threshold configuration interface be located? → A: Add threshold fields to the existing Home page that shows current power values
- Q: How should threshold changes be saved in the web portal UI? → A: Use the existing save button in the footer, no additional save action needed
- Q: What precision/increment should be supported for threshold input fields (step size)? → A: 0.1 kW (100 watts)
- Q: When validation fails, should the system clear invalid values or preserve them for correction? → A: Show error message but preserve user's invalid input for correction
- Q: What UI pattern should be used for threshold input fields? → A: Standard form layout with labels
- Q: Should all three icons (grid, home, solar) use the same threshold structure? → A: Yes, all icons use 4 consistent color levels (good/ok/attention/warning) with 3 thresholds each defining 4 zones
- Q: Should the colors themselves be configurable, or only the thresholds? → A: Both - colors are configurable with defaults (green/white/orange/red)
- Q: How do users "skip" a color level if they don't want it? → A: Set adjacent thresholds equal (e.g., threshold_1 = threshold_2 skips the "ok" level)
- Q: Can threshold values be negative? → A: Yes, especially for grid power when exporting to grid (negative values indicate export)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Configure Grid Power Thresholds (Priority: P1)

Users need to customize the color-coded thresholds for grid power display to match their specific energy consumption patterns and tariff structures. For example, a user with high-consumption appliances may want red warnings at 3kW instead of 2.5kW, while a user optimizing for low consumption might prefer warnings at 1.5kW.

**Why this priority**: Core functionality that enables personalization of the primary visual feedback mechanism. Different households have vastly different energy patterns, making one-size-fits-all thresholds ineffective.

**Independent Test**: Can be fully tested by accessing the web portal, configuring grid power thresholds, and verifying that the power screen displays colors according to the new thresholds, delivering immediate personalized visual feedback.

**Acceptance Scenarios**:

1. **Given** user accesses the web portal configuration page, **When** the page loads, **Then** current grid power thresholds are displayed with default or saved values (export threshold, low import threshold, medium import threshold)
2. **Given** user enters new grid power thresholds (e.g., 0.0kW export, 0.3kW low, 2.0kW medium), **When** user saves the configuration, **Then** the system validates and stores the new thresholds
3. **Given** new grid power thresholds are saved, **When** the power screen displays with grid power at 1.8kW, **Then** the grid icon displays in orange (below the 2.0kW medium threshold)
4. **Given** grid power is -0.5kW (exporting), **When** the power screen displays, **Then** the grid icon displays in bright green (below the 0.0kW export threshold)
5. **Given** user attempts to save invalid thresholds (e.g., medium < low), **When** validation occurs, **Then** the system rejects the configuration and displays an error message

---

### User Story 2 - Configure Home Power Thresholds (Priority: P1)

Users need to customize the color-coded thresholds for home consumption display to reflect their household's normal operating ranges and identify abnormal consumption patterns quickly.

**Why this priority**: Equal importance to grid thresholds (P1) - both are primary visual indicators. Home consumption thresholds help users identify when appliances are using unexpected amounts of power.

**Independent Test**: Can be fully tested by accessing the web portal, configuring home power thresholds, and verifying that home consumption displays colors according to the new thresholds, independent of grid configuration.

**Acceptance Scenarios**:

1. **Given** user accesses the web portal configuration page, **When** the page loads, **Then** current home power thresholds are displayed with default or saved values (very low threshold, low threshold, medium threshold)
2. **Given** user enters new home power thresholds (e.g., 0.4kW very low, 0.8kW low, 1.5kW medium), **When** user saves the configuration, **Then** the system validates and stores the new thresholds
3. **Given** new home power thresholds are saved, **When** the power screen displays with home consumption at 1.2kW, **Then** the home icon displays in orange (between 0.8kW and 1.5kW)
4. **Given** home power is 0.3kW, **When** the power screen displays, **Then** the home icon displays in bright green (below the 0.4kW very low threshold)

---

### User Story 3 - Configure Solar Power Thresholds (Priority: P1)

Users need to customize the color-coded threshold for solar power generation to distinguish between meaningful generation and negligible/no generation, accounting for their specific solar installation capacity.

**Why this priority**: Equal importance to other thresholds (P1) - completes the set of configurable visual indicators. Solar generation patterns vary widely based on installation size and location.

**Independent Test**: Can be fully tested by accessing the web portal, configuring solar power threshold, and verifying that solar generation displays colors according to the new threshold, independent of other configurations.

**Acceptance Scenarios**:

1. **Given** user accesses the web portal configuration page, **When** the page loads, **Then** current solar power threshold is displayed with default or saved value (generation threshold)
2. **Given** user enters a new solar power threshold (e.g., 0.3kW for meaningful generation), **When** user saves the configuration, **Then** the system validates and stores the new threshold
3. **Given** new solar power threshold is saved, **When** the power screen displays with solar generation at 0.8kW, **Then** the solar icon displays in bright green (above the 0.3kW threshold)
4. **Given** solar power is 0.1kW, **When** the power screen displays, **Then** the solar icon displays in white (below the 0.3kW threshold)

---

### User Story 4 - Restore Default Thresholds (Priority: P2)

Users who have experimented with custom thresholds need a simple way to restore the original factory default thresholds without manually entering each value.

**Why this priority**: Important for usability but not critical for core functionality. Users can manually re-enter default values if needed, making this a convenience feature.

**Independent Test**: Can be fully tested by clicking a "Restore Defaults" button on the web portal and verifying that all thresholds revert to original factory values, delivering quick recovery from configuration mistakes.

**Acceptance Scenarios**:

1. **Given** user has customized power thresholds, **When** user clicks "Restore Defaults" button on the web portal, **Then** all power thresholds are reset to factory default values
2. **Given** default thresholds are restored, **When** the configuration page reloads, **Then** the displayed threshold values match the factory defaults (Grid: 0.0kW/0.5kW/2.5kW, Home: 0.5kW/1.0kW/2.0kW, Solar: 0.5kW)
3. **Given** default thresholds are restored, **When** the power screen displays, **Then** colors are applied using the factory default thresholds

---

### User Story 5 - Persist Configuration Across Reboots (Priority: P1)

Users expect their customized power thresholds to persist across device reboots and power cycles, so they don't need to reconfigure after every restart.

**Why this priority**: Critical for user experience - configuration that doesn't persist is effectively useless. This is a prerequisite for the feature to be considered complete.

**Independent Test**: Can be fully tested by configuring thresholds, rebooting the device, and verifying that the thresholds are still applied, delivering reliable persistence independent of other features.

**Acceptance Scenarios**:

1. **Given** user has configured custom power thresholds, **When** the device configuration is saved, **Then** the thresholds are stored in persistent storage
2. **Given** custom thresholds are saved in persistent storage, **When** the device reboots, **Then** the custom thresholds are automatically loaded and applied to the power screen
3. **Given** device has no saved threshold configuration, **When** the device boots for the first time, **Then** factory default thresholds are used

---

### User Story 6 - Rolling Min/Max Statistics Overlay (Priority: P2)

Users viewing the power screen benefit from visual indicators showing the recent min/max power levels for each category (solar, home, grid) over the past 10 minutes. This provides instant context about recent consumption patterns and variability without cluttering the display with numeric data.

**Why this priority**: Enhances situational awareness but not critical for basic monitoring. Provides valuable insight into consumption patterns and helps identify spikes or drops in power usage.

**Independent Test**: Can be fully tested by running the display for 10 minutes with varying power levels and verifying that min/max horizontal lines appear on each power bar at the correct positions, delivering visual feedback about recent power ranges.

**Acceptance Scenarios**:

1. **Given** the system has been running for less than 10 minutes, **When** the power screen displays, **Then** min/max lines appear based on available sample data
2. **Given** solar power has varied between 0.5kW and 2.5kW over the past 10 minutes, **When** the power screen displays, **Then** white horizontal lines appear on the solar bar at positions corresponding to 0.5kW (min) and 2.5kW (max)
3. **Given** grid power has been stable at 1.0kW, **When** the power screen displays, **Then** both min and max lines appear at the same position (1.0kW)
4. **Given** power values are updated every second, **When** the 10-minute window fills, **Then** the oldest samples are discarded and statistics update with rolling window
5. **Given** power value is NaN (no data), **When** statistics are calculated, **Then** NaN values are excluded from min/max calculations

---

### Edge Cases

- What happens when user enters negative values for thresholds that should be positive? System should validate and reject negative values for home and solar thresholds, but allow negative grid export threshold.
- What happens when thresholds are entered in wrong order (e.g., medium < low)? System should validate that thresholds are in ascending order and reject invalid configurations with a clear error message.
- What happens when user enters extremely high threshold values (e.g., 999kW)? System should accept valid numeric values but may impose reasonable upper limits (e.g., 10kW maximum) to prevent display issues.
- What happens when user enters decimal precision beyond display capability? System should accept decimal values and round for display as needed, storing full precision internally.
- What happens when threshold values are modified while the power screen is actively displaying? The power screen should update colors immediately upon receiving new threshold configuration.
- What happens when user accesses the web portal while device is in AP mode vs normal mode? Configuration should be accessible in both modes, with thresholds persisting regardless of network mode.
- What happens when stored threshold configuration becomes corrupted? System should detect invalid configuration and fall back to factory defaults.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow users to configure three threshold values for each power type (grid, home, solar), defining four color zones: good (green), ok (white), attention (orange), warning (red)
- **FR-002**: System MUST allow users to configure four color values (good, ok, attention, warning) as RGB hex codes, applied consistently across all three power displays
- **FR-003**: System MUST allow negative threshold values for grid power to support export scenarios (negative power = exporting to grid)
- **FR-004**: System MUST validate that threshold values are in correct ascending order before saving (threshold_0 <= threshold_1 <= threshold_2), allowing equal values to "skip" a color level
- **FR-005**: System MUST validate that threshold values are numeric and within acceptable ranges (-10kW to +10kW with 0.1kW step precision)
- **FR-006**: System MUST store configured thresholds and colors in persistent storage alongside other device configuration
- **FR-007**: System MUST load stored thresholds and colors on device boot and apply them to power screen color calculations
- **FR-008**: System MUST provide factory default thresholds and colors when no custom configuration exists: Thresholds Grid (0.0/0.5/2.5 kW), Home (0.5/1.0/2.0 kW), Solar (0.5/1.5/3.0 kW); Colors Good=#00FF00, OK=#FFFFFF, Attention=#FFA500, Warning=#FF0000
- **FR-009**: System MUST provide a web portal interface for viewing and editing all power threshold values and color values on the Home page
- **FR-010**: System MUST update power screen colors immediately when new thresholds or colors are applied (without requiring reboot)
- **FR-011**: Web portal Home page MUST display current threshold values and color values when the page loads
- **FR-012**: Web portal Home page MUST provide clear labeling for each threshold field indicating which color zone it controls using standard form layout
- **FR-013**: System MUST provide a mechanism to restore all thresholds and colors to factory defaults
- **FR-014**: System MUST display validation errors to users when invalid threshold or color values are submitted while preserving the invalid input values for correction
- **FR-015**: System MUST preserve threshold and color configuration across device reboots and power cycles
- **FR-016**: System MUST track rolling min/max statistics for each power type (solar, home, grid) over a 10-minute window with 1-second sampling
- **FR-017**: System MUST display horizontal overlay lines on each power bar indicating the min and max values from the 10-minute rolling window
- **FR-018**: Min/max overlay lines MUST be visually distinct (1px width, 70% opacity, white color) without obscuring the power bar or value text
- **FR-019**: Statistics buffer MUST use a circular buffer implementation to maintain constant memory usage (600 samples per power type)
- **FR-020**: System MUST exclude NaN values from min/max calculations to handle missing or invalid data gracefully
- **FR-021**: Min/max lines MUST extend slightly beyond the bar edges (2px left, 5px right) for better visibility

### Key Entities

- **Power Threshold Configuration**: Represents the complete set of configurable threshold and color values
  - Grid power thresholds: 3 values (threshold_0, threshold_1, threshold_2) defining 4 zones, can be negative for export
  - Home power thresholds: 3 values (threshold_0, threshold_1, threshold_2) defining 4 zones, must be non-negative
  - Solar power thresholds: 3 values (threshold_0, threshold_1, threshold_2) defining 4 zones, must be non-negative
  - Color definitions: 4 RGB hex values (good_color, ok_color, attention_color, warning_color) applied globally
  - Relationship: Stored as part of DeviceConfig structure, persisted in NVS storage

- **Color Coding Logic**: Represents the mapping between power values and display colors
  - Uses 3 threshold values to determine which of 4 color zones applies: value < threshold_0 → good, < threshold_1 → ok, < threshold_2 → attention, else → warning
  - Applies configured color values to icons, text, and labels on power screen
  - Supports "skipping" color levels by setting adjacent thresholds equal (e.g., threshold_0 = threshold_1 skips "ok" level)
  - Relationship: Consumes Power Threshold Configuration to make color decisions

- **Power Statistics**: Represents rolling statistics for a single power type over 10-minute window
  - Circular buffer: 600 samples (1-second sampling interval × 600 seconds)
  - Tracked values: minimum, maximum, average
  - Memory footprint: ~480 bytes per power type (600 × 4-byte float)
  - Total for 3 types: ~1.44 KB
  - Relationship: Consumed by PowerScreen for rendering min/max overlay lines

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can access and modify all nine threshold values (three per power type) and four color values through the web portal in under 2 minutes
- **SC-002**: Modified threshold values are reflected on the power screen display within 1 second of saving configuration
- **SC-003**: Configured threshold values persist across device reboots with 100% reliability
- **SC-004**: Users can restore factory default thresholds with a single action (one button click)
- **SC-005**: Invalid threshold configurations (wrong order, invalid values) are rejected with clear error messages 100% of the time
- **SC-006**: The system continues to operate normally if threshold configuration is missing or corrupted, falling back to factory defaults
- **SC-007**: Min/max statistics overlay lines are visually identifiable on the power bars without obscuring current power values
- **SC-008**: Statistics memory usage remains constant at ~1.6 KB regardless of runtime duration (circular buffer prevents unbounded growth)
- **SC-009**: Min/max calculations complete within 20 microseconds per update cycle, maintaining 60 FPS display performance

## Assumptions

- The existing DeviceConfig structure in config_manager.h can be extended to include threshold values without breaking existing configuration storage
- The web portal already has a framework for displaying and updating configuration values that can be reused for threshold configuration
- The power screen color calculation functions (get_grid_color, get_home_color, get_solar_color) can be modified to use configurable thresholds instead of hard-coded values
- Users understand the color coding scheme (green = good/exporting, white = low, orange = medium, red = high) as established in the existing implementation
- Threshold values will be stored as floating-point numbers to match the existing power value data type
- The maximum reasonable power threshold is 10kW for residential energy monitoring applications
- Users accessing the web portal have basic understanding of their household's typical power consumption patterns
- Configuration changes can be applied at runtime without requiring device reboot (though reboot will preserve changes)
- ESP32 has sufficient free RAM (~200+ KB) to accommodate 1.6 KB statistics buffers without impacting stability
- 1-second sampling interval provides sufficient granularity for 10-minute trend analysis without excessive memory usage
- Min/max statistics do not need to persist across reboots (acceptable to rebuild statistics after boot)
- LVGL line objects can be positioned with pixel-perfect accuracy using relative coordinate system (lv_obj_align + local points)
