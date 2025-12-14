# Image Display Implementation Guide

## Overview

This document details the implementation of dynamic image display on an ESP32-C3 with a 1.69" ST7789V2 LCD (240×280 pixels). It covers the challenges faced, solutions evaluated, and the final architecture that balances memory constraints, performance, and ease of use.

**Target audience:** Developers implementing image display on resource-constrained embedded systems with LVGL.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Constraints & Requirements](#constraints--requirements)
3. [Format Evaluation](#format-evaluation)
4. [Color Space Challenge](#color-space-challenge)
5. [Architecture Design](#architecture-design)
6. [Implementation Details](#implementation-details)
7. [Usage Guide](#usage-guide)
8. [Lessons Learned](#lessons-learned)
9. [References](#references)

---

## Problem Statement

**Goal:** Display user-uploaded images (e.g., notifications, alerts, QR codes) on an ESP32-controlled LCD display.

**Initial considerations:**
- How to transfer images to the device?
- What image format to use?
- How to handle memory limitations?
- How to decode and render efficiently?

---

## Constraints & Requirements

### Hardware Constraints

| Component | Specification | Limitation |
|-----------|--------------|------------|
| **MCU** | ESP32-C3 Super Mini | 320KB total RAM |
| **Free RAM** | ~80KB available | After firmware, LVGL, WiFi stack |
| **Display** | ST7789V2 240×280 | **BGR565** pixel format (not RGB!) |
| **Storage** | 4MB Flash | No SD card, limited file storage |

### Functional Requirements

- ✅ Upload images via HTTP API
- ✅ Display for 10 seconds (configurable timeout)
- ✅ Auto-dismiss or manual dismiss
- ✅ Return to previous screen after timeout
- ✅ Minimize preprocessing complexity
- ✅ Handle color accuracy (RGB↔BGR)

---

## Format Evaluation

### Option 1: Standard JPEG

```
Pros:
+ Widespread format, easy to generate
+ Good compression ratio
+ Native camera/phone output

Cons:
- Decoder needs full image decode (~134KB for 240×280×RGB565)
- No strip-based decoding in standard libraries
- Memory usage exceeds ESP32-C3 available RAM
```

**Verdict:** ❌ Too memory-intensive

---

### Option 2: PNG

```
File sizes (for 280×240 test image):
- JPG:  24,836 bytes
- PNG:  45,746 bytes (2x larger!)

Memory requirements:
- File buffer: 46KB
- DEFLATE window: 32KB (required by compression)
- Output buffer: 134KB (full decode) OR 560 bytes (per-line)
Total: 78KB+ (vs 35KB for SJPG)

Pros:
+ Lossless quality
+ No preprocessing needed
+ LVGL has built-in decoder (lodepng)

Cons:
- 2x larger files than JPEG
- No streaming support in lodepng ("All data must be available")
- DEFLATE compression requires sequential decode
- Scanline filtering dependencies (line N needs N-1)
- Higher memory usage overall
```

**Verdict:** ❌ Larger files, more RAM needed, no streaming

---

### Option 3: SJPG (Split JPEG) ✅ SELECTED

**Format specification:**
```
SJPG Header Structure (offset: content):
0-7:   "_SJPG__" magic bytes + version
14-15: Image width (uint16, big-endian)
16-17: Image height (uint16, big-endian)
18-19: Number of frames/strips (uint16)
20-21: Block height in pixels (uint16)
22+:   Frame offset array
Data:  Multiple JPEG fragments (one per horizontal strip)
```

**How it works:**
1. Original image split into horizontal strips (typically 16 pixels high)
2. Each strip encoded as independent JPEG fragment
3. Decoder loads one strip at a time into memory
4. Strips rendered sequentially to display

**Memory efficiency example (280×240 image):**
```
Total file: 22,404 bytes
Strips: 15 frames × 16 pixels high
Per-strip decode: 280 × 16 × 3 bytes = ~13KB

vs Full JPEG decode: 280 × 240 × 2 bytes = 134KB

Memory savings: 10x reduction! ✅
```

**Pros:**
- ✅ 10x less decode memory (13KB vs 134KB)
- ✅ Similar file size to JPEG (~22KB)
- ✅ Built into LVGL (TJpgDec library)
- ✅ Random access to strips (efficient for partial updates)
- ✅ Good compression ratio

**Cons:**
- ⚠️ Requires preprocessing (JPG → SJPG conversion)
- ⚠️ Slightly larger than original JPEG (~10% overhead)

**Verdict:** ✅ Best balance of memory, file size, and LVGL integration

---

## Color Space Challenge

### The Problem: RGB vs BGR

**Discovery:** After implementing SJPG display, red and blue colors were swapped.

**Root cause analysis:**

1. **JPEG format stores RGB data** (industry standard)
2. **LVGL SJPG decoder outputs RGB888** → converts to **RGB565**
3. **ST7789V2 display expects BGR565** (red and blue swapped)

**Color format breakdown:**
```
RGB565 format: RRRR RGGG GGGB BBBB
               ▲ 5 bits red (high)
                    ▲ 6 bits green (middle)
                          ▲ 5 bits blue (low)

BGR565 format: BBBB BGGG GGGR RRRR  
               ▲ 5 bits blue (high)
                    ▲ 6 bits green (middle)
                          ▲ 5 bits red (low)
```

**Why displays differ:**
- Many Chinese LCD manufacturers (ST7789V2, ILI9341, etc.) use BGR pixel order
- RGB order common in Western displays
- **No universal standard** - must check datasheet!

---

### Solution Approaches Evaluated

#### Option A: Modify LVGL Library ❌
```cpp
// In ~/Arduino/libraries/lvgl/src/extra/libs/sjpg/lv_sjpg.c
// Swap R and B during RGB565 packing:

// Original (RGB):
uint16_t col = (*cache++ & 0xf8) << 8;  // R → bits 15-11
col |= (*cache++ & 0xFC) << 3;           // G → bits 10-5
col |= (*cache++ >> 3);                  // B → bits 4-0

// Modified (BGR):
uint8_t b = *cache++, g = *cache++, r = *cache++;
uint16_t col = (b & 0xf8) << 8;  // B → bits 15-11
col |= (g & 0xFC) << 3;           // G → bits 10-5
col |= (r >> 3);                  // R → bits 4-0
```

**Cons:**
- Modifies third-party library
- Changes lost on LVGL updates
- Affects ALL projects using LVGL on system

**Verdict:** ❌ Not acceptable

---

#### Option B: Runtime Color Swap ❌
```cpp
// Swap in VFS callbacks or after decode
for (int i = 0; i < pixels; i++) {
    uint8_t temp = buf[i*3];
    buf[i*3] = buf[i*3 + 2];
    buf[i*3 + 2] = temp;
}
```

**Cons:**
- Complex hooking into LVGL internals
- Performance overhead
- Difficult to maintain

**Verdict:** ❌ Too complex

---

#### Option C: Preprocessing Color Swap ✅ SELECTED

**Convert RGB → BGR during JPG → SJPG conversion on server/PC:**

```bash
# In jpg2sjpg.sh conversion script:
1. Load JPG with PIL (Pillow)
2. Split into R, G, B channels
3. Merge as B, G, R (swapping red and blue)
4. Save temporary BGR JPG
5. Convert to SJPG format
6. Upload to ESP32
```

```python
from PIL import Image

img = Image.open('input.jpg')
r, g, b = img.split()
img_bgr = Image.merge('RGB', (b, g, r))  # Swap R and B
img_bgr.save('temp_bgr.jpg')
# Convert temp_bgr.jpg → SJPG
```

**Pros:**
- ✅ No library modifications
- ✅ No runtime overhead
- ✅ Each image optimized for display
- ✅ Works with any LVGL version
- ✅ Easy to maintain and understand

**Verdict:** ✅ Clean, efficient, maintainable solution

---

## Architecture Design

### System Overview

```
┌─────────────────────────────────────────────────────────┐
│ User's Computer / Server                                │
│                                                          │
│  photo.jpg (RGB)                                        │
│      ↓                                                  │
│  [jpg2sjpg.sh]                                         │
│      ├─ PIL: R,G,B → B,G,R (color swap)               │
│      ├─ Save temp BGR JPG                              │
│      └─ LVGL jpg_to_sjpg.py → photo.sjpg (BGR data)  │
│      ↓                                                  │
│  [test-image-api.sh]                                   │
│      └─ HTTP POST /api/display/image                   │
└─────────────────────────────────────────────────────────┘
                       ↓ WiFi
┌─────────────────────────────────────────────────────────┐
│ ESP32-C3 Device                                         │
│                                                          │
│  ┌────────────────────────────────────────────┐        │
│  │ AsyncWebServer                              │        │
│  │  POST /api/display/image                    │        │
│  │   ├─ Validate SJPG header (_SJP)           │        │
│  │   ├─ Allocate heap buffer (malloc)          │        │
│  │   └─ Copy uploaded data → image_buffer     │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ ImageScreen::load_image()                   │        │
│  │   └─ Point VFS globals → image_buffer      │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ LVGL Image Widget                           │        │
│  │   lv_img_set_src(img_obj, "M:mem.sjpg")   │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ LVGL Filesystem Layer                       │        │
│  │   Sees drive "M:" → calls our VFS driver   │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ Custom VFS Driver (letter 'M')              │        │
│  │   fs_open_cb()  → return success            │        │
│  │   fs_read_cb()  → memcpy from image_buffer │        │
│  │   fs_seek_cb()  → adjust position           │        │
│  │   fs_tell_cb()  → return position           │        │
│  │   fs_close_cb() → no-op                     │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ LVGL SJPG Decoder (TJpgDec)                 │        │
│  │   ├─ Parse header (dimensions, frames)      │        │
│  │   └─ Decode strips on-demand                │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ ST7789V2 LCD Display                        │        │
│  │   Renders BGR565 pixels                     │        │
│  └────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────┘
```

---

## Implementation Details

### 1. Virtual Filesystem Driver

**Why needed:** LVGL's SJPG decoder expects to read from a "file" via filesystem API, but we have data in RAM.

**Solution:** Implement LVGL filesystem driver that reads from memory buffer.

**Registration:**
```cpp
// screen_image.cpp - Static initialization
static lv_fs_drv_t fs_drv;

ImageScreen::ImageScreen() {
    if (!fs_driver_registered) {
        lv_fs_drv_init(&fs_drv);
        fs_drv.letter = 'M';  // Drive letter 'M'
        fs_drv.open_cb = fs_open_cb;
        fs_drv.read_cb = fs_read_cb;
        fs_drv.close_cb = fs_close_cb;
        fs_drv.seek_cb = fs_seek_cb;
        fs_drv.tell_cb = fs_tell_cb;
        lv_fs_drv_register(&fs_drv);
        fs_driver_registered = true;
    }
}
```

**Key callbacks:**
```cpp
// Global buffer pointers
static const uint8_t* vfs_jpeg_data = nullptr;
static size_t vfs_jpeg_size = 0;
static size_t vfs_jpeg_pos = 0;

static void* fs_open_cb(lv_fs_drv_t* drv, const char* path, lv_fs_mode_t mode) {
    if (vfs_jpeg_data && (strcmp(path, "mem.sjpg") == 0)) {
        vfs_jpeg_pos = 0;
        return (void*)1;  // Non-null = success
    }
    return nullptr;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t* drv, void* file_p, 
                               void* buf, uint32_t btr, uint32_t* br) {
    uint32_t remaining = vfs_jpeg_size - vfs_jpeg_pos;
    *br = (btr < remaining) ? btr : remaining;
    
    if (*br > 0) {
        memcpy(buf, vfs_jpeg_data + vfs_jpeg_pos, *br);
        vfs_jpeg_pos += *br;
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t* drv, void* file_p, 
                               uint32_t pos, lv_fs_whence_t whence) {
    if (whence == LV_FS_SEEK_SET) vfs_jpeg_pos = pos;
    else if (whence == LV_FS_SEEK_CUR) vfs_jpeg_pos += pos;
    else if (whence == LV_FS_SEEK_END) vfs_jpeg_pos = vfs_jpeg_size + pos;
    
    if (vfs_jpeg_pos > vfs_jpeg_size) vfs_jpeg_pos = vfs_jpeg_size;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t* drv, void* file_p, uint32_t* pos_p) {
    *pos_p = vfs_jpeg_pos;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t* drv, void* file_p) {
    return LV_FS_RES_OK;  // No cleanup needed
}
```

**Usage:**
```cpp
// Set VFS to point at our buffer
vfs_jpeg_data = image_buffer;
vfs_jpeg_size = buffer_size;
vfs_jpeg_pos = 0;

// LVGL can now "open" and "read" from "M:mem.sjpg"
lv_img_set_src(img_obj, "M:mem.sjpg");
```

---

### 2. SJPG Decoder Initialization

**Enable in lv_conf.h:**
```cpp
#define LV_USE_SJPG 1  // Enable split-JPEG decoder
```

**Initialize in display manager:**
```cpp
// display_manager.cpp
void display_init() {
    lcd_init();
    lv_init();
    
    // Initialize SJPG decoder
    lv_split_jpeg_init();
    
    // ... rest of display setup
}
```

---

### 3. Image Upload Handler

**HTTP endpoint:**
```cpp
// web_portal.cpp
server.on("/api/display/image", HTTP_POST, 
    [](AsyncWebServerRequest* request) {},
    handleImageUpload
);

void handleImageUpload(AsyncWebServerRequest* request, 
                       String filename, size_t index, 
                       uint8_t* data, size_t len, bool final) {
    
    static uint8_t* upload_buffer = nullptr;
    static size_t upload_size = 0;
    static size_t upload_index = 0;
    
    if (index == 0) {
        // First chunk - allocate buffer
        upload_size = request->contentLength();
        upload_buffer = (uint8_t*)malloc(upload_size);
        upload_index = 0;
    }
    
    // Append chunk
    if (upload_buffer && upload_index + len <= upload_size) {
        memcpy(upload_buffer + upload_index, data, len);
        upload_index += len;
    }
    
    if (final) {
        // Validate SJPG header
        if (upload_index >= 4) {
            uint32_t magic = (upload_buffer[0] << 24) | 
                            (upload_buffer[1] << 16) |
                            (upload_buffer[2] << 8) | 
                             upload_buffer[3];
            
            if (magic == 0x5F534A50) {  // "_SJP"
                // Valid SJPG - display it
                display_show_image(upload_buffer, upload_index);
                request->send(200, "application/json", 
                    "{\"success\":true,\"message\":\"Image displayed (10s timeout)\"}");
            } else {
                free(upload_buffer);
                request->send(400, "application/json",
                    "{\"success\":false,\"error\":\"Invalid format (SJPG required)\"}");
            }
        }
    }
}
```

---

### 4. BGR Color Preprocessing

**Conversion script (jpg2sjpg.sh):**
```bash
#!/bin/bash
# Swap R↔B channels before SJPG conversion

INPUT="$1"
OUTPUT="$2"
TEMP_JPG=$(mktemp /tmp/jpg2sjpg-XXXXXX.jpg)

# Swap channels using PIL
python3 << EOF
from PIL import Image

img = Image.open('$INPUT')
if img.mode != 'RGB':
    img = img.convert('RGB')

# Split and swap R↔B
r, g, b = img.split()
img_bgr = Image.merge('RGB', (b, g, r))

# Save temporary BGR JPG
img_bgr.save('$TEMP_JPG', 'JPEG', quality=95)
EOF

# Convert BGR JPG to SJPG
python3 ~/Arduino/libraries/lvgl/scripts/jpg_to_sjpg.py "$TEMP_JPG"

# Move output to desired location
mv "${TEMP_JPG%.jpg}.sjpg" "$OUTPUT"
rm "$TEMP_JPG"
```

---

### 5. Screen Management

**Disable scrolling (clip oversized images):**
```cpp
void ImageScreen::create() {
    screen_obj = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x000000), 0);
    
    // Disable scrollbars - expect correct-sized images
    lv_obj_clear_flag(screen_obj, LV_OBJ_FLAG_SCROLLABLE);
    
    img_obj = lv_img_create(screen_obj);
    lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, 0);
}
```

**Auto-dismiss timer:**
```cpp
void ImageScreen::show() {
    lv_scr_load(screen_obj);
    visible = true;
    
    // 10 second timeout
    timeout_start = millis();
    timeout_duration = 10000;
}

void ImageScreen::update() {
    if (visible && millis() - timeout_start >= timeout_duration) {
        display_hide_image();  // Return to previous screen
    }
}
```

---

## Usage Guide

### For End Users

**1. Prepare image:**
```bash
# Recommended size: 240×280 pixels (or smaller)
# Use ImageMagick, GIMP, or any image editor to resize

convert input.jpg -resize 240x280 output.jpg
```

**2. Upload to device:**
```bash
# Automatic conversion and upload
cd tools
./test-image-api.sh 192.168.1.120 output.jpg
```

**3. Manual dismiss (optional):**
```bash
curl -X DELETE http://192.168.1.120/api/display/image
```

---

### For Developers

**Required tools:**
- Python 3 with PIL/Pillow: `pip3 install Pillow`
- LVGL library in Arduino (for jpg_to_sjpg.py script)
- curl (for API testing)

**Manual conversion:**
```bash
# Convert JPG to SJPG with BGR swap
./tools/jpg2sjpg.sh input.jpg output.sjpg

# Upload SJPG directly
curl -X POST -F "image=@output.sjpg" http://192.168.1.120/api/display/image
```

**Integration example (Node.js):**
```javascript
const fs = require('fs');
const FormData = require('form-data');
const { exec } = require('child_process');

// Convert JPG to SJPG
exec(`./tools/jpg2sjpg.sh photo.jpg photo.sjpg`, (err) => {
    if (err) throw err;
    
    // Upload to ESP32
    const form = new FormData();
    form.append('image', fs.createReadStream('photo.sjpg'));
    
    fetch('http://192.168.1.120/api/display/image', {
        method: 'POST',
        body: form
    })
    .then(res => res.json())
    .then(data => console.log(data));
});
```

---

## Lessons Learned

### Memory Management

**Insight:** On ESP32-C3 with ~80KB free RAM, choose formats that minimize decode overhead.

**Recommendation:**
- ✅ Use strip-based/tile-based image formats (SJPG, tiled PNG)
- ❌ Avoid full-image decode formats for high-res images
- ✅ Profile actual RAM usage with realistic images

**Our results:**
```
280×240 image:
- SJPG file: 22KB
- Decode buffer: 13KB (one strip)
- Total RAM: 35KB ✅

vs Full JPEG decode: 134KB ❌ (exceeds available RAM)
```

---

### Color Space Verification

**Insight:** Always verify display's native color format (RGB vs BGR).

**How to check:**
1. Display solid red (RGB: 0xFF0000, RGB565: 0xF800)
2. If appears blue → BGR display
3. If appears red → RGB display

**Solution patterns:**
- **Option A:** Swap in flush callback (works for LVGL-drawn elements)
- **Option B:** Swap during image preprocessing (works for images) ✅
- **Option C:** Modify decoder library (not recommended)

---

### Streaming vs Buffering

**Finding:** True streaming (fetch on-demand from network) impractical for image decoders.

**Why:**
- JPEG/SJPG decoders need random access (seek operations)
- HTTP range requests add 50-200ms latency per seek
- Total latency for 15 strips: 750ms - 3 seconds
- User experience: unacceptable delay

**Recommendation:**
- ✅ Buffer entire image in RAM
- ✅ Use compressed format to minimize buffer size
- ❌ Don't attempt network-based streaming for random-access decoders

---

### Preprocessing Trade-offs

**Initial concern:** "Preprocessing on server is inconvenient"

**Reality:** Preprocessing enables:
- Optimized file size (conversion to SJPG saves space)
- Color correction (BGR swap)
- Resolution validation (resize to fit display)
- Format validation (reject unsupported formats)

**Recommendation:**
- ✅ Accept preprocessing as part of workflow
- ✅ Automate it (scripts, API wrappers)
- ✅ Validate early (fail fast on invalid inputs)

---

### LVGL Integration

**Insight:** Use LVGL's existing decoders when possible.

**Pros:**
- Well-tested, optimized code
- Maintained by LVGL team
- Integrated with LVGL rendering pipeline

**Cons:**
- Less control over specifics
- May need workarounds for edge cases

**Our approach:**
- ✅ Use LVGL SJPG decoder (TJpgDec)
- ✅ Work around BGR issue via preprocessing
- ✅ Extend via virtual filesystem (clean integration point)

---

## Performance Metrics

### Memory Usage

| Stage | Heap Used | Notes |
|-------|-----------|-------|
| Idle | 0 bytes | No image loaded |
| Upload buffer | ~22KB | Temporary during upload |
| Image buffer | 22KB | Persistent until dismiss |
| SJPG decode | +13KB | Per-strip cache (LVGL internal) |
| **Total** | **~35KB** | Peak during display |

### Timing

| Operation | Duration | Notes |
|-----------|----------|-------|
| RGB→BGR swap | ~80ms | PIL processing on PC |
| JPG→SJPG convert | ~70ms | LVGL script on PC |
| Upload (WiFi) | ~200ms | For 22KB over local network |
| Display load | 3-16ms | SJPG header parse + first strip |
| **Total** | **~350ms** | Upload to display |

### File Sizes

| Format | Size | Compression Ratio |
|--------|------|-------------------|
| Original JPG | 24.8KB | Baseline |
| PNG | 45.7KB | 184% of JPG |
| SJPG | 22.4KB | 90% of JPG ✅ |

---

## References

### Technical Documentation

- **LVGL Documentation:** https://docs.lvgl.io/8/
- **SJPG Format:** https://docs.lvgl.io/8/libs/sjpg.html
- **TJpgDec Library:** http://elm-chan.org/fsw/tjpgd/00index.html
- **ST7789V2 Datasheet:** [Display manufacturer specs]

### Related Code

- `src/app/screen_image.h/cpp` - Image screen implementation
- `src/app/display_manager.cpp` - SJPG decoder initialization
- `src/app/web_portal.cpp` - Image upload API
- `tools/jpg2sjpg.sh` - BGR conversion script
- `tools/test-image-api.sh` - End-to-end test script

### Similar Projects

- **LVGL Examples:** https://github.com/lvgl/lv_examples
- **ESP32 Image Display:** Community forums, GitHub repos
- **TFT_eSPI Library:** Alternative display driver (no LVGL)

---

## Conclusion

Implementing image display on resource-constrained devices requires careful consideration of:

1. **Format selection** - SJPG provides best balance for ESP32-C3
2. **Color space handling** - Preprocessing solves RGB/BGR mismatch cleanly
3. **Memory architecture** - VFS enables RAM-based file access
4. **User workflow** - Automated preprocessing minimizes friction

**Key takeaway:** Don't fight the constraints - design within them. The SJPG format was specifically created for embedded systems like ours, and preprocessing is a small price for correct, efficient operation.

---

**Document version:** 1.0  
**Last updated:** December 14, 2025  
**Maintainer:** ESP32 Energy Monitor Project
