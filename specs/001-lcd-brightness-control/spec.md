# Feature Specification: LCD Backlight Brightness Control

**Feature Branch**: `001-lcd-brightness-control`  
**Created**: December 13, 2025  
**Status**: Draft  
**Input**: User description: "our lcd screen has backlight. i want to allow the user to configure the backlight via the web portal. on the home page of the web portal there should be a config section for this. i want to expose the lcd brightness as a slider, from 0 to 100%. when the slider is moved, the brightness should update on the devices immediately. only persist the value when the user uses one of the save buttons."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Adjust LCD Brightness in Real-Time (Priority: P1)

A user accesses the home page of the web portal and wants to adjust the LCD screen brightness to a comfortable viewing level. They interact with a brightness slider, and as they move it, the physical LCD backlight immediately adjusts to match the selected brightness level. The change is instant and provides immediate visual feedback on the device.

**Why this priority**: This is the core functionality of the feature - allowing users to control brightness with immediate visual feedback. Without this, the feature provides no value.

**Independent Test**: Can be fully tested by accessing the home page, moving the brightness slider, and observing the LCD backlight intensity change in real-time. Delivers immediate value by allowing users to set comfortable viewing levels.

**Acceptance Scenarios**:

1. **Given** the user has opened the web portal home page, **When** they move the brightness slider to 50%, **Then** the LCD backlight immediately adjusts to 50% brightness
2. **Given** the LCD brightness is currently at 100%, **When** the user moves the slider to 25%, **Then** the backlight dims to 25% brightness within 100 milliseconds
3. **Given** the user is adjusting the brightness slider, **When** they move it from 0% to 100%, **Then** the brightness transitions smoothly across the full range
4. **Given** the brightness slider is at any position, **When** the user moves it to 0%, **Then** the LCD backlight turns off completely

---

### User Story 2 - Persist Brightness Settings (Priority: P2)

After adjusting the LCD brightness to their preferred level, a user wants to save this setting so it persists across device reboots. They click a save button on the home page, and the current brightness level is stored permanently. When the device restarts, it automatically restores the saved brightness level.

**Why this priority**: Persistence ensures users don't have to reconfigure brightness after every reboot. This provides long-term convenience but the feature is still useful without it (P1 works independently).

**Independent Test**: Can be tested by adjusting brightness, clicking save, rebooting the device, and verifying the brightness level is restored. Delivers value by eliminating repeated configuration.

**Acceptance Scenarios**:

1. **Given** the user has adjusted brightness to 75%, **When** they click the save button, **Then** the brightness setting is permanently stored
2. **Given** a brightness setting of 60% has been saved, **When** the device reboots, **Then** the LCD restores to 60% brightness on startup
3. **Given** the user has moved the slider but not saved, **When** they refresh the web page, **Then** the slider shows the last saved value, not the temporary adjustment
4. **Given** the user has adjusted brightness multiple times without saving, **When** they finally click save, **Then** only the most recent brightness value is persisted

---

### User Story 3 - View Current Brightness Setting (Priority: P3)

When a user opens the web portal home page, they can immediately see the current brightness level displayed on the slider. This helps them understand the current configuration and make informed adjustments.

**Why this priority**: This enhances user experience by showing current state, but the feature is still functional without it. Users can still adjust brightness even if the initial slider position isn't perfectly accurate.

**Independent Test**: Can be tested by opening the web portal and verifying the slider position matches the actual LCD brightness. Delivers value by providing configuration visibility.

**Acceptance Scenarios**:

1. **Given** the LCD brightness is set to 80%, **When** the user opens the home page, **Then** the brightness slider displays at the 80% position
2. **Given** no brightness setting has been saved (first use), **When** the user opens the home page, **Then** the slider displays at the default brightness level
3. **Given** the brightness was changed via real-time adjustment but not saved, **When** the user closes and reopens the web portal, **Then** the slider shows the saved value, not the temporary value

---

### Edge Cases

- What happens when the user rapidly moves the brightness slider back and forth? System should handle rapid updates smoothly without flickering or lag
- How does the system handle concurrent users adjusting brightness from different devices? Last adjustment wins, no conflict resolution needed
- What happens if the save operation fails (e.g., storage full)? User should receive clear error feedback
- What happens when brightness is set to 0%? LCD backlight turns completely off but device remains operational
- What happens if the user navigates away from the page without saving? Temporary brightness adjustments are lost, device reverts to saved setting on page reload or reboot

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a brightness slider control on the web portal home page with a range of 0% to 100% in 1% increments (integer values 0-100)
- **FR-002**: System MUST update the LCD backlight brightness in real-time (within 100ms) when the slider value changes
- **FR-003**: System MUST display the current brightness level on the slider when the home page loads
- **FR-004**: System MUST persist the brightness setting only when the user explicitly saves via the shared save button on the home page
- **FR-005**: System MUST restore the saved brightness level when the device powers on or reboots
- **FR-006**: System MUST support brightness values across the full range from 0% (backlight off) to 100% (maximum brightness)
- **FR-007**: System MUST use the existing shared save button feedback mechanism (no brightness-specific visual feedback required)
- **FR-008**: System MUST handle rapid slider adjustments without performance degradation or visual artifacts
- **FR-009**: The brightness slider MUST be part of a "Display Settings" section on the home page
- **FR-010**: System MUST maintain temporary brightness changes until explicitly saved or until the page is reloaded/device rebooted

### Key Entities

- **Brightness Setting**: Represents the LCD backlight intensity level (0-100%). Has two states: temporary (in-memory) and persisted (non-volatile storage). Default value is assumed to be 100% (maximum brightness) unless previously saved.
- **LCD Backlight**: Physical hardware component that displays content. Controlled by the brightness setting. Responds to real-time adjustments from the web interface.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can adjust LCD brightness from 0% to 100% with the change visible on the physical display within 100 milliseconds of slider movement
- **SC-002**: Brightness settings persist across device reboots with 100% reliability (saved value matches restored value)
- **SC-003**: Users can make and view temporary brightness adjustments without triggering automatic saves
- **SC-004**: The brightness control interface responds to rapid slider movements (10+ adjustments per second) without lag or visual glitches
- **SC-005**: Brightness control is accessible from the web portal home page in under 2 clicks (navigate to home, control is immediately visible)

## Assumptions

1. **Default Brightness**: The default brightness level (when no saved setting exists) is 100% (maximum brightness)
2. **Save Button Integration**: The existing shared save button on the home page will save brightness settings alongside other configuration (brightness is included in the same save operation)
3. **Hardware Capability**: The LCD hardware supports PWM or similar brightness control across the full 0-100% range
4. **Response Time**: A 100ms response time for brightness changes is acceptable and achievable with the current hardware
5. **Single Brightness Setting**: There is one global brightness setting for the LCD, not per-user or per-page settings
6. **Concurrent Access**: If multiple users adjust brightness simultaneously, the most recent adjustment takes precedence (no locking or conflict resolution)
7. **Web API**: The system will expose `/api/brightness` endpoint supporting GET (retrieve current brightness 0-100) and POST (update brightness in real-time) operations
8. **Storage Availability**: Non-volatile storage (NVS/flash) has sufficient space for storing an additional integer or percentage value

## Clarifications

### Session 2025-12-13

- Q: The specification mentions the brightness control should be in a "clearly labeled configuration section" on the home page, but doesn't specify the section name. What should this section be called? → A: Display Settings
- Q: For the REST API endpoint that will handle brightness updates, what path pattern should be used? → A: /api/brightness
- Q: What type of visual feedback should the user receive during the save operation for brightness settings? → A: Use existing shared save button pattern (no custom message needed)
- Q: What should be the granularity/step size for the brightness slider? → A: 1% increments (0-100 integer values)

## Out of Scope

The following are explicitly not included in this feature:

- **Automatic brightness adjustment** based on ambient light sensors
- **Scheduled brightness changes** (e.g., dim at night, bright during day)
- **Per-page brightness settings** (e.g., different brightness for different screens/pages on the LCD)
- **Brightness presets** or favorite levels
- **Animation effects** for brightness transitions (instant changes only)
- **Power consumption display** showing energy usage at different brightness levels
- **Screen timeout** or auto-off features
- **Brightness adjustment via physical buttons** on the device (web portal only)
