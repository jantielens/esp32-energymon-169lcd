# Image Display Implementation Guide

## Overview

This document details the implementation of dynamic image display on an ESP32-C3 with a 1.69" ST7789V2 LCD (240×280 pixels). It covers the challenges faced, solutions evaluated, and the final architecture that balances memory constraints, performance, and ease of use.

**Note:** The current HTTP APIs render uploaded images **direct-to-LCD** using TJpgDec:
- `POST /api/display/image` (single baseline JPEG)
- `POST /api/display/image/strips` (sequential JPEG strips)

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
- Some JPEG encodings are unsupported on-device (e.g. progressive JPEG)
- For the single-upload API, the device must buffer the compressed JPEG in RAM (bounded by the upload size limit)
- For best reliability, clients may need to re-encode as baseline JPEG with standard chroma subsampling
```

**Verdict:** ✅ Supported (with constraints)

Why this is feasible now:
- The firmware decodes JPEG **direct-to-LCD** using TJpgDec and a small working buffer (no full RGB framebuffer).
- For constant memory usage independent of image file size, use `/api/display/image/strips`.

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
Total: 78KB+

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

### Option 3: Strip Upload (Baseline JPEG Strips) ✅ SELECTED

**How it works:**
1. Original image split into horizontal strips (typically 16 pixels high)
2. Each strip encoded as independent JPEG fragment
3. Client uploads each strip sequentially to `/api/display/image/strips`
4. Device decodes each fragment and renders it sequentially to the LCD

Metadata is provided via query parameters; each request body is a single baseline JPEG fragment.

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
- ✅ Works with TJpgDec on-device (direct-to-LCD)
- ✅ Random access to strips (efficient for partial updates)
- ✅ Good compression ratio

**Cons:**
- ⚠️ Requires client-side preprocessing (split + encode per strip)
- ⚠️ Higher HTTP overhead (multiple requests)

**Verdict:** ✅ Best balance of memory, file size, and implementation complexity

---

## Color Space Challenge

**Current behavior:** Clients upload normal **RGB JPEG** input. The firmware handles any required RGB/BGR ordering internally during decode/flush.

### The Problem: RGB vs BGR

The ST7789V2 panel expects BGR565 ordering at the LCD interface, while decoded pixel data is naturally RGB.

The firmware resolves this by packing pixels to the correct 16-bit format during the decode/output stage.

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
    
    // Receive request body fragments...
    
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
│ Client                                                  │
│                                                         │
│  photo.jpg (RGB)                                        │
│      ↓                                                  │
│  tools/upload_image.py                                  │
│      ├─ Re-encode as baseline JPEG (4:2:0)              │
│      ├─ POST /api/display/image          (single)        │
│      └─ POST /api/display/image/strips   (strips)        │
└─────────────────────────────────────────────────────────┘
                       ↓ WiFi
┌─────────────────────────────────────────────────────────┐
│ ESP32-C3 Device                                         │
│                                                         │
│  AsyncWebServer task                                    │
│   ├─ Validate upload + best-effort JPEG preflight        │
│   ├─ Queue a pending operation (no direct display calls) │
│   └─ Respond JSON                                        │
│                                                         │
│  Main loop task                                         │
│   ├─ Processes pending operation                         │
│   ├─ Decodes via TJpgDec                                 │
│   └─ Writes pixels directly to LCD (raw panel coords)    │
│                                                         │
│  ST7789V2 LCD                                            │
│   └─ Receives RGB565/BGR565 as required                  │
└─────────────────────────────────────────────────────────┘
```


### Single Upload (`POST /api/display/image`)

- Accepts a single baseline JPEG via multipart upload.
- Performs basic JPEG magic validation and best-effort header preflight.
- Enforces panel dimensions (`LCD_WIDTH`×`LCD_HEIGHT`) for now.
- The main loop performs the decode and writes pixels direct-to-LCD.

### Strip Upload (`POST /api/display/image/strips`)

- Accepts a sequence of baseline JPEG fragments.
- Metadata is carried in query parameters (`strip_index`, `strip_count`, `width`, `height`, `timeout`).
- Each fragment is decoded and rendered to the correct vertical region in raw panel coordinates.

### Rotation

Both endpoints render in raw panel coordinates and do not apply UI rotation.
If you want the image to appear rotated, rotate it on the client before upload.

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
python3 tools/upload_image.py 192.168.1.120 output.jpg --mode single
```

**3. Manual dismiss (optional):**
```bash
curl -X DELETE http://192.168.1.120/api/display/image
```

---

### For Developers

**Required tools:**
- Python 3 with PIL/Pillow and requests: `pip3 install Pillow requests`
- curl (optional, for API testing)

**Manual upload (curl):**
```bash
# Upload a baseline JPEG
curl -X POST -F "image=@output.jpg" http://192.168.1.120/api/display/image
```

**Integration example (Node.js):**
```javascript
const fs = require('fs');
const FormData = require('form-data');

// Upload baseline JPEG to ESP32
const form = new FormData();
form.append('image', fs.createReadStream('photo.jpg'));

fetch('http://192.168.1.120/api/display/image', {
    method: 'POST',
    body: form
})
.then(res => res.json())
.then(data => console.log(data));
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
- Both `/api/display/image` and `/api/display/image/strips` render in **raw panel coordinates**.
- UI rotation settings do not rotate uploaded images.
- Prepare the image at the raw panel size (typically **240×280**) and rotate on the client if you want a different orientation.

---

### Memory Management

**Insight:** On ESP32-C3 with ~80KB free RAM, choose formats that minimize decode overhead.

**Recommendation:**
- ✅ Use strip-based baseline JPEG uploads for predictable memory usage
- ❌ Avoid full-image decode formats for high-res images
- ✅ Profile actual RAM usage with realistic images

**Rule of thumb:** Strip uploads keep peak memory bounded by a small working buffer plus a small line/strip buffer.

---

### Color Space Verification

**Insight:** Always verify display's native color format (RGB vs BGR).

**How to check:**
1. Display solid red (RGB: 0xFF0000, RGB565: 0xF800)
2. If appears blue → BGR display
3. If appears red → RGB display

**Guideline:** Upload normal RGB JPEGs (no channel swapping). The firmware packs pixels to the correct RGB565/BGR565 format internally.

---

### Streaming vs Buffering

**Finding:** True streaming (fetch on-demand from network) impractical for image decoders.

**Why:**
- JPEG decoders need random access (seek operations)
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
- Reliable decoding (baseline JPEG re-encode)
- Resolution validation (resize to fit display)
- Format validation (reject unsupported formats)
 - Orientation control (rotate to match desired appearance)

**Recommendation:**
- ✅ Accept preprocessing as part of workflow
- ✅ Automate it (scripts, API wrappers)
- ✅ Validate early (fail fast on invalid inputs)

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
- White screen with garbage → image session state overwritten mid-decode
- Crashes after upload → LVGL called from wrong task
- Second upload fails → memory not freed before allocation

**Solution checklist:**
- ✅ Avoid UI updates while image session is active
- ✅ State machine tracks upload progress
- ✅ Operation IDs prevent duplicate processing
- ✅ Visibility checks prevent widget updates when screen hidden
- ✅ Memory freed before new allocation

**Recommendation:** For any embedded LVGL project with async networking (WiFi, Bluetooth), implement deferred operations from day one.

---

## Performance Metrics

### Memory Usage

| Stage | Heap Used | Notes |
|-------|-----------|-------|
| Idle | 0 bytes | No image session active |
| Upload buffer | Varies | Single upload buffers compressed JPEG while receiving |
| Decode working buffers | Small, bounded | JPEG decoder working state + small line/strip buffer |
| **Total** | **Bounded** | Does not require a full-frame RGB buffer |

### Timing

| Operation | Duration | Notes |
|-----------|----------|-------|
| Client preprocessing | Varies | Resize/letterbox/rotate; optional baseline JPEG re-encode |
| Upload (WiFi) | Varies | Depends on JPEG size and link quality |
| Decode + blit | Varies | Depends on JPEG complexity and fragment sizing |

### File Sizes

| Format | Size | Compression Ratio |
|--------|------|-------------------|
| Original JPG | 24.8KB | Baseline |
| PNG | 45.7KB | 184% of JPG |

---

## References

### Technical Documentation

- **LVGL Documentation:** https://docs.lvgl.io/8/
- **TJpgDec Library:** http://elm-chan.org/fsw/tjpgd/00index.html
- **ST7789V2 Datasheet:** [Display manufacturer specs]

### Related Code

- `src/app/screen_direct_image.h/cpp` - Direct-to-LCD image session
- `src/app/strip_decoder.h/cpp` - TJpgDec integration (direct-to-LCD)
- `src/app/image_api.h/cpp` - Image upload API (`/api/display/image`, `/api/display/image/strips`)
- `src/app/jpeg_preflight.h/cpp` - JPEG header preflight (baseline + sampling checks)
- `tools/upload_image.py` - Reference uploader for single + strips

### Similar Projects

- **LVGL Examples:** https://github.com/lvgl/lv_examples
- **ESP32 Image Display:** Community forums, GitHub repos
- **TFT_eSPI Library:** Alternative display driver (no LVGL)

---

## Conclusion

Implementing image display on resource-constrained devices requires careful consideration of:

1. **Format selection** - Baseline JPEG is supported; strip uploads provide a scalable path under tight RAM
2. **Color space handling** - Upload normal RGB JPEGs; firmware handles RGB/BGR packing internally
3. **Memory architecture** - Direct-to-LCD decoding avoids large full-frame buffers
4. **User workflow** - Use `tools/upload_image.py` for consistent encoding and endpoint usage

**Key takeaway:** Design around predictable memory use. Baseline JPEG + direct-to-LCD decode is the supported path.

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
