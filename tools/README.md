# Development Tools

This directory contains build-time and runtime tools for the ESP32 Energy Monitor project.

## Build Tools (automated via build scripts)

### png2icons.py
**Used by:** `build.sh`  
**Purpose:** Converts PNG icons from `assets/icons/` to C arrays for LVGL  
**Usage:** Automatically invoked during build  

### minify-web-assets.sh
**Used by:** `build.sh`  
**Purpose:** Minifies HTML/CSS/JS files and generates `web_assets.h`  
**Usage:** Automatically invoked during build  

### extract-changelog.sh
**Used by:** Release process  
**Purpose:** Extracts version-specific changelog sections from CHANGELOG.md  
**Usage:** `./tools/extract-changelog.sh <version>`  
**Documentation:** See [docs/developer/release-process.md](../docs/developer/release-process.md)

---

## Runtime Tools (manual usage)

### upload_image.py ⭐ **Main Image Upload Tool**
**Purpose:** Upload images to ESP32 device (baseline JPEG) using either single upload or strip upload (multiple requests)  
**Features:**
- Configurable strip height (8, 16, 32, 64 pixels)
- Debug mode (upload specific strips)
- Progress display

**Usage:**
```bash
# Strip-based upload with 32px strips (memory efficient for large images)
python3 upload_image.py 192.168.1.111 photo.jpg --mode strip --strip-height 32

# Single-file upload (faster for small images)
python3 upload_image.py 192.168.1.111 photo.jpg --mode single

# Custom display timeout (0 = permanent, default = 10s)
python3 upload_image.py 192.168.1.111 photo.jpg --timeout 30

# Debug: upload only strip 2
python3 upload_image.py 192.168.1.111 photo.jpg --mode strip --start 2 --end 2
```

**Requirements:** `pip3 install Pillow requests`

**Documentation:** See [docs/developer/strip-upload-implementation.md](../docs/developer/strip-upload-implementation.md)

---

### camera_to_esp32.py
**Purpose:** Home Assistant AppDaemon app for sending camera snapshots to ESP32  
**Usage:** Deploy to AppDaemon, trigger via automation  
**Documentation:** See [docs/user/home-assistant-integration.md](../docs/user/home-assistant-integration.md)

---

## Test Files

### test240x280.jpg
Example image for testing (240×280 pixels, matches display resolution)
