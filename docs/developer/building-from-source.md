# Building from Source

Complete guide to building the ESP32 Energy Monitor firmware from source code.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Initial Setup](#initial-setup)
- [Build System Overview](#build-system-overview)
- [Development Scripts](#development-scripts)
- [Development Workflow](#development-workflow)
- [Build Configuration](#build-configuration)
- [Asset Generation](#asset-generation)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Software

- **Linux or WSL2** - Build scripts require bash
- **Git** - For cloning the repository
- **Python 3** - For icon generation (auto-installed by setup.sh)
- **USB Connection** - For uploading firmware to ESP32

### Supported Boards

- ESP32 DevKit V1 (dual-core 240 MHz)
- ESP32-C3 Super Mini (single-core 160 MHz, USB-C)

## Initial Setup

### 1. Clone Repository

```bash
git clone https://github.com/jantielens/esp32-energymon-169lcd.git
cd esp32-energymon-169lcd
```

### 2. Run Setup Script

The setup script installs all required tools and dependencies:

```bash
./setup.sh
```

**What it does:**
- Downloads and installs `arduino-cli` to `./bin/` (local, no sudo required)
- Configures ESP32 board support
- Installs ESP32 core platform
- Installs Arduino libraries from `arduino-libraries.txt`
- Installs Python Pillow library for PNG icon conversion

**Output:**
```
Installing arduino-cli...
Configuring arduino-cli...
Installing ESP32 board support...
Installing libraries...
Installing Python dependencies...
Setup complete!
```

**When to run:**
- First time setup
- After clean checkout
- When `arduino-libraries.txt` is updated

### 3. Verify Setup

```bash
./bin/arduino-cli version
./bin/arduino-cli board listall esp32
```

## Build System Overview

### Architecture

The build system is designed for multi-board compilation with automated asset generation:

```
┌─────────────────┐
│   config.sh     │ ← Configuration & helper functions
└────────┬────────┘
         │
    ┌────▼──────────────────────────────┐
    │  build.sh                          │
    ├────────────────────────────────────┤
    │ 1. Generate PNG icons → C arrays   │
    │ 2. Minify web assets → gzip        │
    │ 3. Compile firmware (all boards)   │
    │ 4. Create .bin files per board     │
    └────────────────────────────────────┘
         │
    ┌────▼──────────────────┐
    │  build/                │
    │  ├── esp32/            │
    │  │   └── *.bin         │
    │  └── esp32c3/          │
    │      └── *.bin         │
    └────────────────────────┘
```

### Build Outputs

Each board gets its own directory in `build/`:

```
build/
├── esp32/
│   ├── app.ino.bin              # Main firmware binary
│   ├── app.ino.bootloader.bin   # Bootloader
│   ├── app.ino.merged.bin       # Combined (all-in-one)
│   └── app.ino.partitions.bin   # Partition table
└── esp32c3/
    ├── app.ino.bin
    ├── app.ino.bootloader.bin
    ├── app.ino.merged.bin
    └── app.ino.partitions.bin
```

**Files:**
- **app.ino.bin**: Main firmware (use for OTA updates)
- **app.ino.merged.bin**: Combined binary (all partitions)
- **app.ino.bootloader.bin**: ESP32 bootloader
- **app.ino.partitions.bin**: Partition table

## Development Scripts

### config.sh

**Purpose:** Common configuration and helper functions.

**Key Variables:**
- `PROJECT_NAME`: Slug for filenames (e.g., "esp32-energymon-169lcd")
- `PROJECT_DISPLAY_NAME`: Human-readable name (e.g., "ESP32 Energy Monitor")
- `FQBN_TARGETS`: Associative array of board FQBNs and names

**Helper Functions:**
- `find_arduino_cli()`: Locates arduino-cli (local or system-wide)
- `find_serial_port()`: Auto-detects `/dev/ttyUSB0` or `/dev/ttyACM0`
- `get_board_name()`: Extracts board name from FQBN
- `list_boards()`: Lists all configured boards
- `get_fqbn_for_board()`: Gets FQBN for a board name

**Multi-Board Configuration:**
```bash
declare -A FQBN_TARGETS=(
    ["esp32:esp32:esp32"]="esp32"                                        
    ["esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc"]="esp32c3"
)
```

### setup.sh

**Install and configure development environment.**

```bash
./setup.sh
```

Runs once during initial setup or after clean checkout.

### build.sh

**Compile firmware for one or all board variants.**

```bash
./build.sh              # Build all configured boards
./build.sh esp32        # Build only ESP32 DevKit V1
./build.sh esp32c3      # Build only ESP32-C3 Super Mini
```

**What it does:**
1. Generates PNG icons → LVGL C arrays (once for all builds)
2. Minifies and compresses web assets (once for all builds)
3. Compiles firmware for specified board(s)
4. Creates board-specific output directories
5. If `src/boards/<board>/` exists:
   - Adds it to include path
   - Defines `BOARD_<BOARDNAME>` and `BOARD_HAS_OVERRIDE`

**Build Duration:**
- First build: ~60-90 seconds (includes asset generation)
- Incremental builds: ~30-45 seconds
- Asset-only changes: ~20-30 seconds

### upload.sh

**Upload compiled firmware to ESP32 device.**

**Single Board:**
```bash
./upload.sh              # Auto-detect port
./upload.sh /dev/ttyUSB0 # Specify port
```

**Multiple Boards:**
```bash
./upload.sh esp32              # Upload ESP32 build, auto-detect port
./upload.sh esp32c3            # Upload ESP32-C3 build, auto-detect port
./upload.sh esp32 /dev/ttyUSB0 # Upload ESP32 to specific port
```

**Requirements:**
- Must run `./build.sh [board]` first
- ESP32 connected via USB
- User in `dialout` group (Linux)

### monitor.sh

**Display serial output from ESP32.**

```bash
./monitor.sh                  # Auto-detect port, 115200 baud
./monitor.sh /dev/ttyUSB0     # Custom port
./monitor.sh /dev/ttyUSB0 9600 # Custom port and baud
```

**Usage:**
- View boot messages
- Debug MQTT connections
- Monitor errors
- Press `Ctrl+C` to exit

### clean.sh

**Remove build artifacts and temporary files.**

```bash
./clean.sh
```

**What it removes:**
- `./build/` directory (all board builds)
- Temporary files (*.tmp, *.bak, *~)

**When to use:**
- Force complete rebuild
- Clean up disk space
- Troubleshoot build issues

### upload-erase.sh

**Completely erase ESP32 flash memory.**

**Single Board:**
```bash
./upload-erase.sh              # Auto-detect port
./upload-erase.sh /dev/ttyUSB0 # Specify port
```

**Multiple Boards:**
```bash
./upload-erase.sh esp32              # Erase ESP32
./upload-erase.sh esp32c3            # Erase ESP32-C3
./upload-erase.sh esp32 /dev/ttyUSB0 # Erase ESP32 on specific port
```

**⚠️ Warning:** Erases **ALL** data including firmware, WiFi credentials, and configuration.

**When to use:**
- Clean slate before fresh firmware
- Fix corrupted NVS storage
- Remove old settings completely

### library.sh

**Manage Arduino library dependencies.**

```bash
./library.sh search <keyword>    # Search for libraries
./library.sh add <library>       # Add and install library
./library.sh remove <library>    # Remove from config
./library.sh list                # Show configured libraries
./library.sh install             # Install all from config
./library.sh installed           # Show currently installed
```

**Examples:**
```bash
# Search for MQTT libraries
./library.sh search mqtt

# Add PubSubClient
./library.sh add PubSubClient

# List configured libraries
./library.sh list
```

See [library-management.md](library-management.md) for detailed guide.

### Convenience Scripts

#### bum.sh - Build + Upload + Monitor

**Full development cycle in one command:**

```bash
./bum.sh              # Single board setup
./bum.sh esp32        # Multi-board: specify board
./bum.sh esp32c3 /dev/ttyUSB0 # With custom port
```

**Equivalent to:**
```bash
./build.sh esp32 && ./upload.sh esp32 && ./monitor.sh
```

#### um.sh - Upload + Monitor

**Skip build when firmware is already compiled:**

```bash
./um.sh              # Single board setup
./um.sh esp32        # Multi-board: specify board
./um.sh esp32c3 /dev/ttyUSB0 # With custom port
```

**Equivalent to:**
```bash
./upload.sh esp32 && ./monitor.sh
```

## Development Workflow

### Single Board Project

```bash
# Initial setup (one time)
./setup.sh

# Add libraries as needed
./library.sh search mqtt
./library.sh add PubSubClient

# Edit code in src/app/
# ... make changes ...

# Full development cycle
./bum.sh              # Build + Upload + Monitor

# Or step-by-step
./build.sh            # Compile
./upload.sh           # Upload
./monitor.sh          # View serial output

# Clean build when needed
./clean.sh
./build.sh
```

### Multi-Board Project

```bash
# Initial setup (one time)
./setup.sh

# Build all boards
./build.sh

# Or build specific board
./build.sh esp32
./build.sh esp32c3

# Upload to specific board
./upload.sh esp32       # Auto-detect port
./upload.sh esp32c3     # Auto-detect port

# Full cycle for specific board
./bum.sh esp32          # Build + Upload + Monitor
./um.sh esp32c3         # Upload + Monitor

# Clean all board builds
./clean.sh
```

### Typical Edit-Test Cycle

```bash
# 1. Edit code
vim src/app/screen_power.cpp

# 2. Build and upload
./bum.sh esp32c3

# 3. Monitor serial output
# (already running from bum.sh)
# Press Ctrl+C when done

# 4. Repeat from step 1
```

### Testing Configuration Changes

```bash
# 1. Build with current config
./build.sh esp32c3

# 2. Upload and verify
./upload.sh esp32c3
./monitor.sh

# 3. Test via web portal
# Configure WiFi/MQTT, etc.

# 4. If issues, erase and retry
./upload-erase.sh esp32c3
./upload.sh esp32c3
```

## Build Configuration

### Board Configuration

**Adding New Boards:**

Edit `config.sh` and add to `FQBN_TARGETS`:

```bash
declare -A FQBN_TARGETS=(
    ["esp32:esp32:esp32"]="esp32"
    ["esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc"]="esp32c3"
    ["esp32:esp32:esp32s3"]="esp32s3"  # Add new board
)
```

**Finding FQBN:**
```bash
./bin/arduino-cli board listall | grep -i esp32
```

### Board-Specific Overrides

**Purpose:** Customize GPIO pins, hardware features, or driver settings per board.

**Create Override File:**

```bash
mkdir -p src/boards/esp32c3
cat > src/boards/esp32c3/board_overrides.h << 'EOF'
#ifndef BOARD_OVERRIDES_ESP32C3_H
#define BOARD_OVERRIDES_ESP32C3_H

// LCD pins for ESP32-C3 Super Mini
#define LCD_CS_PIN    7
#define LCD_DC_PIN    3
#define LCD_RST_PIN   20
#define LCD_BL_PIN    1
#define LCD_MOSI_PIN  6
#define LCD_SCK_PIN   4

#endif
EOF
```

**Build Automatically Detects:**
- If `src/boards/<board>/` exists
- Adds to include path: `-I src/boards/<board>`
- Defines: `BOARD_ESP32C3` and `BOARD_HAS_OVERRIDE=1`

**Application Usage:**

```cpp
#include "board_config.h"

void setup() {
  #if HAS_LCD
  lcd_init(LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN);
  #endif
}
```

See [multi-board-support.md](multi-board-support.md) for detailed guide.

### Build Profiles (Advanced)

**Optional:** Define custom build properties per board.

**Example** (in `config.sh`):

```bash
get_build_props_for_board() {
    local board="$1"
    local profile="$2"
    
    case "$board:$profile" in
        esp32:psram)
            echo "--build-property"
            echo "build.extra_flags=-DBOARD_HAS_PSRAM"
            ;;
        esp32c3:16m)
            echo "--build-property"
            echo "build.flash_size=16MB"
            ;;
    esac
}
```

**Usage:**
```bash
BOARD_PROFILE=psram ./build.sh esp32
PROFILE=16m ./build.sh esp32c3
```

## Asset Generation

The build system automatically generates C arrays from web assets and icons.

### Icon Generation

**Source:** `assets/icons/*.png`  
**Output:** `src/app/icons.c`, `src/app/icons.h`  
**Tool:** `tools/png2icons.py`

**Process:**
1. Scans `assets/icons/` for PNG files
2. Converts to LVGL TRUE_COLOR_ALPHA format (RGB565 + Alpha)
3. Generates C arrays and image descriptors
4. Creates header file with extern declarations

**Adding New Icons:**
1. Add 48×48 PNG file to `assets/icons/`
2. Run `./build.sh`
3. Use in code: `lv_img_set_src(img, &icon_name);`

See [icon-system.md](icon-system.md) for detailed guide.

### Web Assets

**Source:** `src/app/web/*.html`, `portal.css`, `portal.js`  
**Output:** `src/app/web_assets.h`  
**Tool:** `tools/minify-web-assets.sh`

**Process:**
1. Template substitution (`{{PROJECT_DISPLAY_NAME}}`)
2. HTML minification (remove comments, whitespace)
3. CSS minification (csscompressor)
4. JavaScript minification (rjsmin)
5. Gzip compression (level 9)
6. Generate C byte arrays

**Compression Stats:**
```
HTML home.html:     5234 → 3891 → 1256 bytes (-76%)
HTML network.html:  8912 → 6543 → 1987 bytes (-78%)
CSS  portal.css:   14348 → 10539 → 2864 bytes (-81%)
JS   portal.js:    32032 → 19700 → 4931 bytes (-85%)
```

**Modifying Web Portal:**

1. Edit files in `src/app/web/`
2. Rebuild to regenerate assets: `./build.sh`
3. Upload and test: `./upload.sh && ./monitor.sh`

## Troubleshooting

### Permission Denied on /dev/ttyUSB0

**Symptom:**
```
Error: can't open device "/dev/ttyUSB0": Permission denied
```

**Solution (Linux):**
```bash
sudo usermod -a -G dialout $USER
# Logout and login, or restart
```

**Solution (WSL):**
```bash
sudo usermod -a -G dialout $USER
# Restart WSL: wsl --terminate Ubuntu (in PowerShell)
```

See [../setup/wsl-development.md](../setup/wsl-development.md) for complete WSL guide.

### arduino-cli Not Found

**Symptom:**
```
./build.sh: line 10: arduino-cli: command not found
```

**Solution:**
```bash
./setup.sh  # Reinstall arduino-cli
```

Scripts use local `./bin/arduino-cli`, not system PATH.

### Build Directory Not Found

**Symptom:**
```
Error: build/esp32 directory not found
```

**Solution:**
```bash
./build.sh esp32  # Build before uploading
```

Must build before upload/monitor.

### Board Name Required

**Symptom:**
```
Error: Board name required when multiple boards configured
```

**Solution:**
```bash
./upload.sh esp32    # Specify board name
./bum.sh esp32c3     # For all scripts
```

Check `config.sh` for configured board names.

### Icon Generation Fails

**Symptom:**
```
ModuleNotFoundError: No module named 'PIL'
```

**Solution:**
```bash
python3 -m pip install --user Pillow
# Or re-run setup
./setup.sh
```

### Web Assets Not Updating

**Symptom:** HTML/CSS/JS changes not reflected in web portal.

**Solution:**
```bash
# Force rebuild
./clean.sh
./build.sh

# Or just regenerate assets
rm src/app/web_assets.h
./build.sh
```

### Out of Memory During Build

**Symptom:**
```
Error: region `iram0_0_seg' overflowed
```

**Solution:**
- Reduce LVGL buffer size in `lv_conf.h`
- Use different partition scheme
- Optimize code size (remove unused features)

### ESP32 Won't Boot After Upload

**Symptom:** Device stuck in boot loop or won't start.

**Solution:**
```bash
# Erase flash and reflash
./upload-erase.sh esp32c3
./upload.sh esp32c3

# Check serial monitor for errors
./monitor.sh
```

### Build Takes Too Long

**Optimization:**

1. **Incremental Builds:** Only changed files recompile
2. **Single Board:** Build specific board, not all
3. **Skip Assets:** Assets only regenerate when source changes

```bash
# Fast: Only rebuild if code changed
./build.sh esp32c3

# Slower: Rebuild all boards
./build.sh
```

## Related Documentation

- [Library Management](library-management.md) - Managing Arduino dependencies
- [Icon System](icon-system.md) - PNG to LVGL conversion
- [Multi-Board Support](multi-board-support.md) - Board variants and overrides
- [Web Portal API](web-portal-api.md) - REST API reference
- [WSL Development](../setup/wsl-development.md) - Windows/WSL setup
- [Release Process](release-process.md) - Creating releases

---

**Ready to contribute?** See [adding-features.md](adding-features.md) for code structure and contribution guidelines.
