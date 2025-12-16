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
5. [Concurrency & Thread Safety](#concurrency--thread-safety)
6. [Architecture Design](#architecture-design)
7. [Implementation Details](#implementation-details)
8. [Usage Guide](#usage-guide)
9. [Lessons Learned](#lessons-learned)
10. [References](#references)

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
- ✅ Display with configurable timeout (default: 10 seconds, 0=permanent, max: 24 hours)
- ✅ Auto-dismiss or manual dismiss
- ✅ Return to previous screen after timeout
- ✅ Minimize preprocessing complexity
- ✅ Handle color accuracy (RGB↔BGR)
- ✅ Accurate timeout counting (starts when upload completes, not after decode)

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
14-15: Image width (uint16, little-endian)
16-17: Image height (uint16, little-endian)
18-19: Number of frames/strips (uint16)
20-21: Block height in pixels (uint16)
22+:   Per-strip length table (uint16 per strip)
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

## Concurrency & Thread Safety

### The Challenge: LVGL is NOT Thread-Safe

**Critical constraint:** LVGL must only be called from a single task context. Calling LVGL functions from multiple FreeRTOS tasks causes:
- Memory corruption
- Display artifacts (partial images, garbage data)
- Crashes and undefined behavior

**Problem:** ESP32 uses multiple tasks:
- **Main loop task** - Runs LVGL updates (lv_timer_handler)
- **AsyncWebServer task** - Handles HTTP requests in background
- **WiFi/MQTT tasks** - Process network events

### Race Conditions Discovered

#### Issue 1: Widget Updates During Screen Switch
```
Timeline:
T+0ms:   User uploads image
T+10ms:  ImageScreen loads and becomes visible
T+500ms: MQTT receives power update
         → PowerScreen::set_solar_power() called
         → lv_label_set_text() modifies widgets
         ❌ PowerScreen NOT visible but widgets updated!
         → Display corruption (20% of uploads)
```

#### Issue 2: AsyncWebServer Direct LVGL Calls
```
HTTP Upload Handler (AsyncWebServer task):
  ├─ display_hide_image()
  │   └─ lv_scr_load()  ❌ LVGL call from wrong task!
  ├─ display_show_image()
  │   └─ lv_img_set_src()  ❌ LVGL call from wrong task!
  └─ Result: Race condition with main loop's lv_timer_handler()
```

#### Issue 3: Concurrent Upload Handling
```
Upload 1: In progress (state = UPLOAD_IN_PROGRESS)
Upload 2: Arrives → HTTP 409 Conflict rejection
          ❌ User confusion, poor UX for rapid uploads
```

#### Issue 4: VFS Global Variable Corruption
```
Upload 1: vfs_jpeg_data points to buffer A
LVGL:     Reading from buffer A via VFS
Upload 2: vfs_jpeg_data reassigned to buffer B
LVGL:     Read corruption! Mixing buffer A and B data
          → White screen with garbage characters
```

### Solutions Implemented

#### Solution 1: Visibility Checks ✅

**Prevent widget updates when screen not visible:**

```cpp
// screen_power.cpp
void PowerScreen::set_solar_power(float kw) {
    // Update statistics (always track)
    update_statistics(solar_kwh, kw);
    
    // Only update widgets if screen is visible
    if (!visible) return;
    
    lv_label_set_text(solar_label, format_power(kw));
    update_bar_charts();
    // ... other widget updates
}
```

**Applied to:** `set_solar_power`, `set_grid_power`, `update_home_value`, `update_bar_charts`, `update_power_colors`, `update_stat_overlays`

---

#### Solution 2: Deferred Operations Pattern ✅

**Architecture:**
```
┌─────────────────────────────────────────┐
│ AsyncWebServer Task (HTTP Handler)      │
│  ├─ Receive upload data                  │
│  ├─ Validate format                      │
│  ├─ Store in pending_image_op struct     │
│  ├─ Set upload_state = READY_TO_DISPLAY  │
│  └─ Increment pending_op_id              │
└─────────────────────────────────────────┘
                 ↓ (no LVGL calls!)
┌─────────────────────────────────────────┐
│ Main Loop Task (app.ino)                │
│  ├─ web_portal_process_pending()         │
│  │   ├─ Check upload_state               │
│  │   ├─ If READY_TO_DISPLAY:             │
│  │   │   ├─ display_show_image()         │
│  │   │   │   └─ lv_img_set_src() ✅      │
│  │   │   └─ upload_state = IDLE          │
│  │   └─ Track last_processed_id          │
│  ├─ lv_timer_handler()                   │
│  └─ mqtt_manager_loop()                  │
└─────────────────────────────────────────┘
```

**State machine:**
```cpp
enum UploadState {
    UPLOAD_IDLE = 0,              // No upload in progress
    UPLOAD_IN_PROGRESS = 1,       // Receiving data from client
    UPLOAD_READY_TO_DISPLAY = 2   // Ready for main loop to process
};

struct PendingImageOp {
    uint8_t* buffer;
    size_t size;
    bool dismiss;
};

static volatile UploadState upload_state = UPLOAD_IDLE;
static volatile unsigned long pending_op_id = 0;
static volatile PendingImageOp pending_image_op = {nullptr, 0, false};
```

**Upload handler (AsyncWebServer task):**
```cpp
void handleImageUpload(...) {
    if (index == 0) {
        // Allocate buffer, set state
        upload_state = UPLOAD_IN_PROGRESS;
    }
    
    // Receive chunks...
    
    if (final) {
        // Queue for display (no LVGL calls here!)
        pending_image_op.buffer = image_upload_buffer;
        pending_image_op.size = image_upload_size;
        pending_op_id++;
        upload_state = UPLOAD_READY_TO_DISPLAY;
        
        request->send(200, "application/json", "{...}");
    }
}
```

**Main loop processing (main task):**
```cpp
void web_portal_process_pending() {
    static unsigned long last_processed_id = 0;
    
    if (upload_state == UPLOAD_READY_TO_DISPLAY && 
        pending_op_id != last_processed_id) {
        
        last_processed_id = pending_op_id;
        
        // Safe to call LVGL here (main task context)
        display_show_image(pending_image_op.buffer, pending_image_op.size);
        
        free((void*)pending_image_op.buffer);
        pending_image_op.buffer = nullptr;
        upload_state = UPLOAD_IDLE;
    }
}

void loop() {
    web_portal_process_pending();  // Process deferred operations
    lv_timer_handler();             // LVGL updates
    // ... other main loop tasks
}
```

---

#### Solution 3: Concurrent Upload Wait-With-Timeout ✅

**Instead of rejecting concurrent uploads, wait for completion:**

```cpp
void handleImageUpload(...) {
    if (index == 0) {  // First chunk
        if (upload_state == UPLOAD_IN_PROGRESS) {
            Logger.logMessage("Upload", "Another upload in progress, waiting...");
            
            unsigned long wait_start = millis();
            while (upload_state == UPLOAD_IN_PROGRESS && 
                   (millis() - wait_start) < 1000) {
                delay(10);  // Yield to other tasks
            }
            
            if (upload_state == UPLOAD_IN_PROGRESS) {
                // Timeout after 1 second
                request->send(409, "application/json", "{...timeout...}");
                return;
            }
            
            Logger.logMessage("Upload", "Previous upload completed, proceeding");
        }
        // Continue with upload...
    }
}
```

**Benefits:**
- Seamless user experience (uploads typically complete in 100-200ms)
- 1 second timeout prevents infinite blocking
- Second upload automatically replaces first

---

#### Solution 4: VFS Busy Flag Protection ✅

**Protect global VFS pointers during concurrent access:**

```cpp
// screen_image.cpp
static volatile bool vfs_busy = false;
static const uint8_t* vfs_jpeg_data = nullptr;
static size_t vfs_jpeg_size = 0;
static size_t vfs_jpeg_pos = 0;

bool ImageScreen::load_image(const uint8_t* buffer, size_t size) {
    if (vfs_busy) {
        Logger.logMessage("Image", "VFS busy, cannot load");
        return false;
    }
    
    vfs_busy = true;
    vfs_jpeg_data = buffer;
    vfs_jpeg_size = size;
    vfs_jpeg_pos = 0;
    
    lv_img_set_src(img_obj, "M:mem.sjpg");
    
    vfs_busy = false;
    return true;
}
```

---

#### Solution 5: Memory Management During Concurrent Uploads ✅

**Free old buffer before allocating new one:**

```cpp
void handleImageUpload(...) {
    if (index == 0) {
        // Free any pending buffer (not yet displayed)
        if (pending_image_op.buffer) {
            free((void*)pending_image_op.buffer);
            pending_image_op.buffer = nullptr;
        }
        
        // Hide currently displayed image (frees its buffer)
        display_hide_image();
        
        // Now safe to allocate new buffer
        image_upload_buffer = (uint8_t*)malloc(total_size);
        upload_state = UPLOAD_IN_PROGRESS;
    }
}
```

---

#### Solution 6: Screen Switching Synchronization ✅

**Issue:** Split JPEG decoder needs multiple frames to fully render.

**Solution:** Call lv_timer_handler() multiple times during screen switch:

```cpp
// display_manager.cpp
void display_show_image(const uint8_t* buffer, size_t size) {
    image_screen.load_image(buffer, size);
    
    lv_scr_load(image_screen.get_screen());
    
    // Give LVGL time to decode initial SJPG strips
    for (int i = 0; i < 5; i++) {
        lv_timer_handler();
        delay(5);
    }
}
```

---

### Debugging Tips

**Serial logging patterns used:**
```cpp
// Track state transitions
Logger.logMessagef("Upload", "State: %d → %d", old_state, new_state);

// Track operation IDs
Logger.logMessagef("Portal", "Processing id=%lu (last=%lu)", 
    pending_op_id, last_processed_id);

// Detect timing issues
unsigned long start = millis();
// ... operation ...
Logger.logMessagef("Perf", "Operation took %lums", millis() - start);
```

**Common symptoms and causes:**
| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Partial image display | Screen switch during widget update | Add visibility checks |
| White screen with "a" | Empty LVGL image widget shown | Use display_hide_image() not clear_buffer |
| Random crashes | LVGL called from wrong task | Use deferred operations |
| Second image doesn't show | Memory not freed | Hide image before allocating |
| Upload hangs | Infinite wait loop | Add timeout to wait logic |

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
│  │ AsyncWebServer Task (Background)            │        │
│  │  POST /api/display/image                    │        │
│  │   ├─ Wait if upload_state == IN_PROGRESS    │        │
│  │   ├─ display_hide_image() (free memory)     │        │
│  │   ├─ Validate SJPG header (_SJP)           │        │
│  │   ├─ Allocate buffer (malloc)               │        │
│  │   ├─ Copy uploaded data → buffer            │        │
│  │   ├─ pending_image_op.buffer = buffer       │        │
│  │   ├─ upload_state = READY_TO_DISPLAY        │        │
│  │   └─ pending_op_id++                        │        │
│  │   ⚠️  NO LVGL CALLS (not thread-safe!)      │        │
│  └────────────────────────────────────────────┘        │
│                       ↓ (deferred)                       │
│  ┌────────────────────────────────────────────┐        │
│  │ Main Loop Task (app.ino)                    │        │
│  │  web_portal_process_pending():              │        │
│  │   ├─ Check upload_state == READY_TO_DISPLAY │        │
│  │   ├─ Check pending_op_id != last_processed  │        │
│  │   ├─ display_show_image() ✅ LVGL safe      │        │
│  │   ├─ free(buffer)                            │        │
│  │   ├─ upload_state = IDLE                    │        │
│  │   └─ last_processed_id = pending_op_id      │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ ImageScreen::load_image()                   │        │
│  │   ├─ vfs_busy = true (atomic flag)          │        │
│  │   ├─ Point VFS globals → image_buffer      │        │
│  │   └─ vfs_busy = false                       │        │
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
│  │   fs_open_cb()  → check vfs_busy, return   │        │
│  │   fs_read_cb()  → memcpy from image_buffer │        │
│  │   fs_seek_cb()  → adjust vfs_jpeg_pos      │        │
│  │   fs_tell_cb()  → return vfs_jpeg_pos      │        │
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
    
    // Timeout timer set when upload completes (for accuracy)
    // Only set here if not already set by upload handler
    if (timeout_start == 0) {
        timeout_start = millis();
    }
}

void ImageScreen::update() {
    if (visible && millis() - timeout_start >= timeout_duration) {
        display_hide_image();  // Return to previous screen
    }
}
```

**Timeout timing:**
The timeout timer starts when the HTTP upload completes (before image decoding), ensuring the full specified timeout duration is available for viewing. This accounts for the 1-3 seconds of JPEG decode time, so users get the full requested display time.

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

### Display Rotation Handling

**Insight:** LVGL's `sw_rotate` setting automatically rotates rendered content, including images.

**How it works:**
```cpp
// board_config.h
#define LCD_ROTATION 1  // 0=portrait, 1=landscape (90°), 2=180°, 3=270°

// display_manager.cpp
#if LCD_ROTATION == 1
    disp_drv.sw_rotate = 1;
    disp_drv.rotated = LV_DISP_ROT_90;
#endif
```

**For image upload:**
- Images displayed via `lv_img_set_src()` are **automatically rotated** by LVGL
- Upload images in **physical display dimensions** (240×280 for portrait display)
- LVGL rotates to match `LCD_ROTATION` setting (e.g., 280×240 landscape)
- No manual rotation needed in image preparation

**Example:**
```bash
# Display hardware: 240×280 pixels (portrait orientation)
# LCD_ROTATION: 1 (90° landscape mode)
# Logical resolution: 280×240 (landscape)

# ✅ Correct - upload in physical dimensions
convert photo.jpg -resize 240x280 output.jpg
python3 upload_image.py 192.168.1.111 output.jpg --mode single

# LVGL automatically rotates 240×280 → 280×240 landscape ✅
```

**Note:** Strip-based upload (direct LCD write) **does NOT** use LVGL rotation - pixels are written exactly as provided. See [strip-upload-implementation.md](strip-upload-implementation.md) for details.

---

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

### Thread Safety with AsyncWebServer

**Critical discovery:** AsyncWebServer runs HTTP handlers in a separate FreeRTOS task.

**Problem pattern:**
```cpp
// ❌ WRONG - LVGL called from AsyncWebServer task
server.on("/api/display/image", HTTP_POST, [](AsyncWebServerRequest* req) {
    display_show_image(buffer, size);  // Crashes/corruption!
});
```

**Solution pattern:**
```cpp
// ✅ CORRECT - Deferred operation
server.on("/api/display/image", HTTP_POST, [](AsyncWebServerRequest* req) {
    pending_image_op.buffer = buffer;
    pending_op_id++;  // Signal main loop
    upload_state = UPLOAD_READY_TO_DISPLAY;
});

void loop() {
    web_portal_process_pending();  // Main loop processes LVGL calls
    lv_timer_handler();
}
```

**Debugging indicators:**
- Partial/corrupted images (20% failure rate) → race condition
- White screen with garbage → VFS globals reassigned mid-decode
- Crashes after upload → LVGL called from wrong task
- Second upload fails → memory not freed before allocation

**Solution checklist:**
- ✅ All LVGL calls from main loop only
- ✅ State machine tracks upload progress
- ✅ Operation IDs prevent duplicate processing
- ✅ Visibility checks prevent widget updates when screen hidden
- ✅ VFS busy flag protects global pointers
- ✅ Memory freed before new allocation

**Recommendation:** For any embedded LVGL project with async networking (WiFi, Bluetooth), implement deferred operations from day one.

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

## Appendix: Troubleshooting Common Issues

### Issue: Intermittent Partial Display (20% failure rate)

**Symptoms:**
- Most uploads display correctly
- ~20% show only partial image
- ~2% show black screen

**Root Cause:** MQTT updates modifying PowerScreen widgets while ImageScreen is visible.

**Fix:**
```cpp
void PowerScreen::set_solar_power(float kw) {
    update_statistics(solar_kwh, kw);  // Always update stats
    if (!visible) return;  // Skip widget updates when not visible
    lv_label_set_text(solar_label, format_power(kw));
}
```

---

### Issue: Second Upload Doesn't Display

**Symptoms:**
- First upload works
- Subsequent uploads fail silently
- Serial log shows "Memory allocation failed"

**Root Cause:** Memory not freed before allocating new buffer.

**Fix:**
```cpp
if (index == 0) {
    display_hide_image();  // Frees displayed image memory
    if (pending_image_op.buffer) {
        free((void*)pending_image_op.buffer);  // Free pending buffer
    }
    image_upload_buffer = (uint8_t*)malloc(total_size);  // Now succeeds
}
```

---

### Issue: White Screen with "a" Character

**Symptoms:**
- Upload completes successfully
- Screen shows white background
- Single "a" character visible

**Root Cause:** Empty LVGL image widget shown (no source set).

**Fix:** Use `display_hide_image()` instead of `display_clear_image_buffer()` - properly hides screen before clearing.

---

### Issue: Upload Hangs / Very Slow (6+ seconds)

**Symptoms:**
- Uploads take 6+ seconds (should be ~200ms)
- No error messages
- Eventually completes

**Root Cause:** `delay()` calls in upload handler blocking data reception.

**Fix:** Remove debugging delays from production code, especially:
```cpp
// ❌ Remove these from handleImageUpload():
delay(1);   // State verification
delay(10);  // Before function exit
```

---

### Issue: HTTP 409 Conflict on Rapid Uploads

**Symptoms:**
- First upload succeeds
- Second upload within 1 second returns 409 error

**Root Cause:** Concurrent upload detection rejecting second upload.

**Evolution:**
1. **v1:** Immediate rejection → poor UX
2. **v2:** Wait-with-timeout (1 second) → seamless replacement ✅

**Current implementation:**
```cpp
if (upload_state == UPLOAD_IN_PROGRESS) {
    // Wait up to 1 second for previous upload to complete
    unsigned long wait_start = millis();
    while (upload_state == UPLOAD_IN_PROGRESS && (millis() - wait_start) < 1000) {
        delay(10);
    }
    if (upload_state == UPLOAD_IN_PROGRESS) {
        return 409;  // Only timeout after 1 second
    }
}
```

---

**Document version:** 2.0  
**Last updated:** December 15, 2025  
**Maintainer:** ESP32 Energy Monitor Project  

**Changelog:**
- v2.0 (Dec 15, 2025): Added concurrency/thread safety section, deferred operations pattern, debugging guide
- v1.0 (Dec 14, 2025): Initial documentation
