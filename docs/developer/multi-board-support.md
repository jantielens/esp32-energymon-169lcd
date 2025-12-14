# Multi-Board Support

Complete guide to adding and managing multiple ESP32 board variants in the same project.

## Table of Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [Board Configuration](#board-configuration)
- [Board-Specific Overrides](#board-specific-overrides)
- [Build System Integration](#build-system-integration)
- [Pin Mapping](#pin-mapping)
- [Adding New Boards](#adding-new-boards)
- [Best Practices](#best-practices)

## Overview

The ESP32 Energy Monitor supports multiple board variants through a flexible multi-board build system. Add new ESP32 boards with a single configuration line, customize hardware-specific code when needed, and compile all variants in parallel.

### Architecture

```
┌─────────────────────────────────────┐
│         config.sh                   │
│  FQBN_TARGETS = {                   │
│    "esp32:esp32:esp32" → "esp32"    │
│    "esp32:...c3..." → "esp32c3"     │
│  }                                  │
└──────────────┬──────────────────────┘
               │
    ┌──────────▼──────────────────────┐
    │     build.sh [board]            │
    │  For each board:                │
    │  1. Find src/boards/<board>/    │
    │  2. Add to include path         │
    │  3. Define BOARD_<NAME>         │
    │  4. Compile with arduino-cli    │
    └──────────┬──────────────────────┘
               │
    ┌──────────▼──────────────────────┐
    │  src/app/board_config.h         │
    │  Phase 1: Include override      │
    │    #ifdef BOARD_HAS_OVERRIDE    │
    │      #include "board_overrides.h"
    │    #endif                        │
    │  Phase 2: Define defaults       │
    │    #ifndef LCD_CS_PIN            │
    │      #define LCD_CS_PIN 5        │
    │    #endif                        │
    └─────────────────────────────────┘
```

### Supported Boards

| Board | FQBN | Board Name | Override Dir |
|-------|------|------------|--------------|
| **ESP32 DevKit V1** | `esp32:esp32:esp32` | `esp32` | _(uses defaults)_ |
| **ESP32-C3 Super Mini** | `esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc` | `esp32c3` | `src/boards/esp32c3/` |

## Quick Start

### Build All Boards

```bash
./build.sh
```

Output:
```
Building firmware for all boards...
Building esp32 (esp32:esp32:esp32)...
Sketch uses 823456 bytes (62%) of program storage space.
Build completed: build/esp32/

Building esp32c3 (esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc)...
Sketch uses 745632 bytes (57%) of program storage space.
Build completed: build/esp32c3/
```

### Build Specific Board

```bash
./build.sh esp32c3
```

### Upload to Board

```bash
./upload.sh esp32c3              # Auto-detect USB port
./upload.sh esp32c3 /dev/ttyUSB0 # Specify port
```

## Board Configuration

### config.sh - Board Registry

All boards are defined in [`config.sh`](../../config.sh):

```bash
# Board configuration (FQBN - Fully Qualified Board Name)
# Define target boards as an associative array: [FQBN]="board-name"
#
# FQBN format: <package>:<architecture>:<boardID>[:<options>]
# Example: esp32:esp32:esp32
#          esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc
#
# Board name: Short identifier used in scripts and build directories
# - Omit board name (or leave empty) to auto-extract from FQBN (3rd segment)
# - Custom board names useful for clarity or branding
#
# Board options:
# - Add options after FQBN with colons: <boardID>:<option1>=<value1>
# - Example: CDCOnBoot=cdc enables USB CDC for ESP32-C3
# - To check if a board needs CDC: `arduino-cli board details <FQBN> | grep CDCOnBoot`
#
# Finding FQBNs:
# - List all ESP32 boards: `./bin/arduino-cli board listall | grep -i esp32`
# - Get board details: `./bin/arduino-cli board details <FQBN>`

declare -A FQBN_TARGETS=(
    ["esp32:esp32:esp32"]="esp32"                                        
    ["esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc"]="esp32c3"
)
```

**Key Concepts:**

- **FQBN** (Fully Qualified Board Name): Arduino's board identifier format
  - Format: `<package>:<architecture>:<boardID>[:<options>]`
  - Example: `esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc`
    - Package: `esp32`
    - Architecture: `esp32`
    - Board ID: `nologo_esp32c3_super_mini`
    - Options: `CDCOnBoot=cdc` (enable USB CDC for serial)

- **Board Name**: Short identifier for scripts and directories
  - Used in: `build.sh esp32c3`, `build/esp32c3/`, `src/boards/esp32c3/`
  - Auto-extracted from FQBN if not specified (3rd segment)
  - Custom names allowed for clarity

### Helper Functions

#### get_board_name()

Extract board name from FQBN:

```bash
get_board_name() {
    local fqbn="$1"
    local board_name="${FQBN_TARGETS[$fqbn]}"
    
    if [[ -n "$board_name" ]]; then
        echo "$board_name"  # Use custom name
    else
        # Auto-extract from FQBN (3rd segment)
        echo "$fqbn" | cut -d':' -f3
    fi
}
```

**Example:**
```bash
get_board_name "esp32:esp32:esp32"
# Output: esp32

get_board_name "esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc"
# Output: esp32c3 (custom name from FQBN_TARGETS)
```

#### list_boards()

Display all configured boards:

```bash
./build.sh help  # Or call list_boards() directly
```

Output:
```
Available boards:
  esp32 → esp32:esp32:esp32
  esp32c3 → esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc
```

#### get_fqbn_for_board()

Lookup FQBN by board name:

```bash
get_fqbn_for_board "esp32c3"
# Output: esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc
```

## Board-Specific Overrides

### Two-Phase Include Pattern

The project uses a **two-phase include pattern** to support board-specific customization:

**Phase 1: Load Board Overrides** (if they exist)
```cpp
#ifdef BOARD_HAS_OVERRIDE
#include "board_overrides.h"
#endif
```

**Phase 2: Define Defaults** (only if not already defined)
```cpp
#ifndef LCD_CS_PIN
#define LCD_CS_PIN 5  // Default for ESP32 DevKit V1
#endif
```

### Creating Board Overrides

**Directory Structure:**
```
src/boards/<board-name>/board_overrides.h
```

**Example: ESP32-C3 Super Mini**

File: [`src/boards/esp32c3/board_overrides.h`](../../src/boards/esp32c3/board_overrides.h)

```cpp
#ifndef BOARD_OVERRIDES_ESP32C3_H
#define BOARD_OVERRIDES_ESP32C3_H

// Built-in LED on ESP32-C3 Super Mini is GPIO8 (not GPIO2)
#define HAS_BUILTIN_LED true
#define LED_PIN 8
#define LED_ACTIVE_HIGH true

// Display (Waveshare 1.69" ST7789V2) - ESP32-C3 Pin Mapping
// ESP32-C3 has different default SPI pins than classic ESP32

// SPI pins
#define LCD_SCK_PIN 4    // Clock (not GPIO18 like ESP32)
#define LCD_MOSI_PIN 6   // Data (not GPIO23 like ESP32)

// Control pins (avoid strapping pins GPIO2/GPIO8/GPIO9)
#define LCD_CS_PIN 7     // Chip Select
#define LCD_DC_PIN 3     // Data/Command
#define LCD_RST_PIN 20   // Reset
#define LCD_BL_PIN 1     // Backlight PWM

#endif
```

**What Gets Overridden:**

| Pin | ESP32 DevKit V1<br>(default) | ESP32-C3 Super Mini<br>(override) |
|-----|------------------------------|-----------------------------------|
| **LED_PIN** | GPIO 2 | GPIO 8 |
| **LCD_SCK_PIN** | GPIO 18 (VSPI) | GPIO 4 |
| **LCD_MOSI_PIN** | GPIO 23 (VSPI) | GPIO 6 |
| **LCD_CS_PIN** | GPIO 5 | GPIO 7 |
| **LCD_DC_PIN** | GPIO 16 | GPIO 3 |
| **LCD_RST_PIN** | GPIO 17 | GPIO 20 |
| **LCD_BL_PIN** | GPIO 4 | GPIO 1 |

### Default Configuration

File: [`src/app/board_config.h`](../../src/app/board_config.h)

```cpp
// ============================================================================
// Board Configuration - Two-Phase Include Pattern
// ============================================================================

// Phase 1: Load Board-Specific Overrides
#ifdef BOARD_HAS_OVERRIDE
#include "board_overrides.h"
#endif

// ============================================================================
// Phase 2: Default Hardware Capabilities
// ============================================================================

// Built-in LED
#ifndef HAS_BUILTIN_LED
#define HAS_BUILTIN_LED false
#endif

#ifndef LED_PIN
#define LED_PIN 2  // Common GPIO for ESP32 boards
#endif

// Display: 1.69" LCD (ST7789V2)
#ifndef HAS_DISPLAY
#define HAS_DISPLAY true
#endif

#ifndef LCD_CS_PIN
#define LCD_CS_PIN 5      // Chip Select (VSPI SS)
#endif

#ifndef LCD_DC_PIN
#define LCD_DC_PIN 16     // Data/Command
#endif

#ifndef LCD_RST_PIN
#define LCD_RST_PIN 17    // Reset
#endif

#ifndef LCD_BL_PIN
#define LCD_BL_PIN 4      // Backlight PWM
#endif

#ifndef LCD_MOSI_PIN
#define LCD_MOSI_PIN 23   // SPI MOSI (VSPI)
#endif

#ifndef LCD_SCK_PIN
#define LCD_SCK_PIN 18    // SPI Clock (VSPI)
#endif
```

### Using Board Configuration in Code

**Application Code:**

```cpp
#include "board_config.h"

void setup() {
  // Use board-agnostic pin constants
  pinMode(LCD_CS_PIN, OUTPUT);
  pinMode(LCD_DC_PIN, OUTPUT);
  pinMode(LCD_RST_PIN, OUTPUT);
  
  // Conditional compilation for optional features
  #if HAS_BUILTIN_LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? HIGH : LOW);
  #endif
}
```

**Conditional Features:**

```cpp
// Board-specific behavior
#ifdef BOARD_ESP32C3
  // ESP32-C3 specific code
  Serial.println("Running on ESP32-C3");
#elif defined(BOARD_ESP32)
  // ESP32 specific code
  Serial.println("Running on ESP32 DevKit V1");
#endif

// Feature detection
#if HAS_BUILTIN_LED
  blinkLED();  // Only compiled if board has LED
#endif
```

## Build System Integration

### How Overrides Are Detected

**In `build.sh`:**

```bash
# Check if board-specific overrides exist
board_override_dir="src/boards/$board_name"
build_props=()

if [[ -d "$board_override_dir" ]]; then
    echo "Using board overrides: $board_override_dir"
    
    # Add board override directory to include path
    build_props+=(
        "--build-property"
        "compiler.cpp.extra_flags=-I$board_override_dir -DBOARD_HAS_OVERRIDE=1 -DBOARD_${board_name^^}=1"
    )
fi
```

**What Happens:**

1. **Directory Check**: `src/boards/esp32c3/` exists?
2. **Include Path**: Add `-I src/boards/esp32c3` to compiler flags
3. **Define Macros**:
   - `BOARD_HAS_OVERRIDE=1` - Triggers override include
   - `BOARD_ESP32C3=1` - Board-specific conditional compilation

**Compiler Flags:**
```bash
-I src/boards/esp32c3          # Add override dir to include path
-DBOARD_HAS_OVERRIDE=1         # Enable override include
-DBOARD_ESP32C3=1              # Board identifier macro
```

### Build Output

```
Building esp32c3 (esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc)...
Using board overrides: src/boards/esp32c3
Compiling sketch...
Sketch uses 745632 bytes (57%) of program storage space.
Global variables use 42816 bytes (13%) of dynamic memory.
Build completed: build/esp32c3/
```

## Pin Mapping

### Finding Pin Information

**For ESP32 DevKit V1:**

See [HARDWARE.md](../../HARDWARE.md#esp32-devkit-v1-pinout) for complete pinout.

**For ESP32-C3 Super Mini:**

See [HARDWARE.md](../../HARDWARE.md#esp32-c3-super-mini-pinout) for complete pinout.

**Visual Diagrams:**

- [docs/pinout.md](../pinout.md) - Detailed wiring diagrams
- Includes ASCII art diagrams and SVG images
- Shows physical pin locations and connections

### Pin Selection Guidelines

**ESP32 DevKit V1 Safe Pins:**

✅ **Safe for LCD:**
- GPIO 23 (VSPI MOSI)
- GPIO 18 (VSPI SCK)
- GPIO 5 (VSPI SS, requires HIGH during boot)
- GPIO 16, 17 (RX2/TX2, no boot conflicts)
- GPIO 4 (general purpose)

❌ **Avoid:**
- GPIO 0 (boot mode select)
- GPIO 1/3 (USB serial)
- GPIO 2 (onboard LED, boot strapping)
- GPIO 6-11 (internal flash)
- GPIO 12 (boot strapping, LOW required)
- GPIO 15 (boot strapping, HIGH required)

**ESP32-C3 Super Mini Strapping Pins:**

⚠️ **Boot-Sensitive (avoid for LCD control):**
- GPIO 2 (strapping)
- GPIO 8 (onboard LED + strapping)
- GPIO 9 (BOOT button)

✅ **Safe for LCD:**
- GPIO 1, 3, 4, 6, 7, 20

### Pin Mapping Strategy

1. **SPI Pins**: Use hardware SPI (VSPI on ESP32, default SPI on ESP32-C3)
2. **Control Pins**: Choose GPIOs without boot strapping requirements
3. **PWM Pins**: Any GPIO can do PWM on ESP32 (use for backlight)
4. **Document Changes**: Update override file with pin rationale

## Adding New Boards

### Step-by-Step Guide

#### 1. Find Board FQBN

```bash
# List all ESP32 boards
./bin/arduino-cli board listall | grep -i esp32

# Or search by keyword
./bin/arduino-cli board listall | grep -i "s3"

# Get board details
./bin/arduino-cli board details esp32:esp32:esp32s3
```

**Example Output:**
```
Board Name: ESP32S3 Dev Module
FQBN: esp32:esp32:esp32s3
```

#### 2. Add to config.sh

Edit [`config.sh`](../../config.sh):

```bash
declare -A FQBN_TARGETS=(
    ["esp32:esp32:esp32"]="esp32"
    ["esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc"]="esp32c3"
    ["esp32:esp32:esp32s3"]="esp32s3"  # New board
)
```

#### 3. Test Default Build

```bash
./build.sh esp32s3
```

If successful, the board uses default pins from `board_config.h`.

#### 4. Create Board Overrides (Optional)

If GPIO pins differ:

```bash
# Create override directory
mkdir -p src/boards/esp32s3

# Create override header
cat > src/boards/esp32s3/board_overrides.h << 'EOF'
#ifndef BOARD_OVERRIDES_ESP32S3_H
#define BOARD_OVERRIDES_ESP32S3_H

// ESP32-S3 specific pin mappings
#define LCD_SCK_PIN 12
#define LCD_MOSI_PIN 11
#define LCD_CS_PIN 10
#define LCD_DC_PIN 9
#define LCD_RST_PIN 8
#define LCD_BL_PIN 7

#endif
EOF
```

#### 5. Rebuild with Overrides

```bash
./build.sh esp32s3
```

Output:
```
Building esp32s3 (esp32:esp32:esp32s3)...
Using board overrides: src/boards/esp32s3
Build completed: build/esp32s3/
```

#### 6. Test and Document

1. **Upload and Test:**
   ```bash
   ./upload.sh esp32s3
   ./monitor.sh
   ```

2. **Document Pinout:**
   - Update [HARDWARE.md](../../HARDWARE.md) with new board section
   - Add pin mapping table
   - Include wiring diagram

3. **Update CHANGELOG.md:**
   ```markdown
   ### Added
   - ESP32-S3 Dev Module board support
   ```

### Board Options

Some boards require configuration options in the FQBN.

**Common Options:**

- **CDCOnBoot**: Enable USB CDC for serial (ESP32-C3/S3)
  ```bash
  "esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc"
  ```

- **FlashSize**: Select flash size
  ```bash
  "esp32:esp32:esp32:FlashSize=4M"
  ```

- **PSRAM**: Enable PSRAM support
  ```bash
  "esp32:esp32:esp32:PSRAM=enabled"
  ```

**Finding Available Options:**

```bash
./bin/arduino-cli board details esp32:esp32:esp32s3
```

Look for "Config options" section.

## Best Practices

### Design Patterns

#### ✅ DO: Use Capability Flags

```cpp
// board_overrides.h
#define HAS_BATTERY_MONITOR true
#define BATTERY_ADC_PIN 4

// app.ino
#if HAS_BATTERY_MONITOR
void readBattery() {
  int raw = analogRead(BATTERY_ADC_PIN);
  // ...
}
#endif
```

#### ✅ DO: Document Pin Choices

```cpp
// ESP32-C3 Pin Selection Rationale:
// - GPIO3: Data/Command (no boot strapping)
// - GPIO8: AVOID (onboard LED + boot strapping)
// - GPIO20: Reset (safe GPIO)
#define LCD_DC_PIN 3
#define LCD_RST_PIN 20
```

#### ✅ DO: Use Defaults When Possible

Only override pins that **must** differ from defaults.

```cpp
// ✅ Good: Only override what's different
#define LCD_SCK_PIN 4   // ESP32-C3 SPI clock

// ❌ Bad: Redefining identical values
#define HAS_DISPLAY true  // Already default
#define LCD_WIDTH 240     // Already default
```

#### ❌ DON'T: Hardcode Pins in Application

```cpp
// ❌ Bad: Hardcoded GPIO
pinMode(5, OUTPUT);

// ✅ Good: Use config constant
pinMode(LCD_CS_PIN, OUTPUT);
```

#### ❌ DON'T: Duplicate Logic

```cpp
// ❌ Bad: Board-specific code duplication
#ifdef BOARD_ESP32
  SPI.begin(18, -1, 23, 5);
#endif
#ifdef BOARD_ESP32C3
  SPI.begin(4, -1, 6, 7);
#endif

// ✅ Good: Use config constants
SPI.begin(LCD_SCK_PIN, -1, LCD_MOSI_PIN, LCD_CS_PIN);
```

### File Organization

```
src/
├── app/
│   ├── board_config.h         # Default configuration
│   ├── app.ino                # Board-agnostic code
│   └── ...
└── boards/
    ├── esp32c3/
    │   └── board_overrides.h  # ESP32-C3 specific
    ├── esp32s3/
    │   └── board_overrides.h  # ESP32-S3 specific (future)
    └── README.md              # Board variant guide
```

### Testing Checklist

When adding a new board:

- [ ] Board builds successfully: `./build.sh <board>`
- [ ] Firmware uploads without errors: `./upload.sh <board>`
- [ ] Serial output shows correct boot messages
- [ ] Display initializes and shows content
- [ ] WiFi connects successfully
- [ ] Web portal accessible
- [ ] OTA updates work
- [ ] All features functional (MQTT, config, etc.)
- [ ] Documentation updated (HARDWARE.md, CHANGELOG.md)
- [ ] Pin mappings verified against datasheet

## Related Documentation

- [HARDWARE.md](../../HARDWARE.md) - Bill of materials and wiring diagrams
- [Building from Source](building-from-source.md) - Build system details
- [docs/pinout.md](../pinout.md) - Detailed pin mapping reference
- [Release Process](release-process.md) - Multi-board releases

---

**Need help?** Check the [pinout documentation](../pinout.md) or open an issue on GitHub.
