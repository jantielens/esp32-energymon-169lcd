# Image Display Implementation - Future Enhancements

## Overview

This document explores advanced image display strategies for ESP32 devices with memory constraints, particularly for scaling to larger displays. It captures research into various approaches and proposes an optimal solution for handling large images that exceed available RAM.

**Context:** The current implementation (described in [image-display-implementation.md](image-display-implementation.md)) works excellently for small to medium displays (≤400×300 pixels) but faces memory limitations for larger screens.

**Target audience:** Developers implementing image display on larger screens or memory-constrained embedded systems.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Current Implementation Limitations](#current-implementation-limitations)
3. [Explored Alternatives](#explored-alternatives)
4. [Recommended Solution: Strip-by-Strip Streaming](#recommended-solution-strip-by-strip-streaming)
5. [Implementation Guide](#implementation-guide)
6. [Adaptive Strategy](#adaptive-strategy)
7. [Performance Comparison](#performance-comparison)

---

## Problem Statement

### The Memory Scaling Challenge

The current SJPG implementation requires buffering the entire compressed file in RAM:

```
Current approach:
1. Upload full SJPG file → Store in RAM buffer
2. Point LVGL VFS at RAM buffer
3. LVGL SJPG decoder reads from buffer (with random access)
4. Decode strips one at a time, display to LCD

Memory requirement: compressed_file_size + strip_decode_buffer
```

**Problem:** As screen resolution increases, SJPG file size grows proportionally, eventually exceeding available RAM.

### Memory Requirements by Screen Size

| Screen Size | SJPG File | Strip Decode | **Total RAM** | Fits in ESP32-C3 (80KB)? |
|-------------|-----------|--------------|---------------|-------------------------|
| 280×240 (current) | 22 KB | 13 KB | **35 KB** | ✅ Yes |
| 320×240 | 28 KB | 15 KB | **43 KB** | ✅ Yes |
| 400×300 | 38 KB | 19 KB | **57 KB** | ✅ Yes |
| 480×320 | 55 KB | 23 KB | **78 KB** | ⚠️ Tight |
| **480×800** | **115 KB** | 23 KB | **138 KB** | ❌ **No** |
| 800×480 | 115 KB | 38 KB | **153 KB** | ❌ No |
| 1024×600 | 180 KB | 49 KB | **229 KB** | ❌ No |

**Breakpoint:** Screens larger than ~400×300 pixels exceed ESP32-C3 RAM capacity.

---

## Current Implementation Limitations

### Why Current Approach Fails for Large Screens

```cpp
// Current implementation requires full file buffer
bool ImageScreen::load_image(const uint8_t* jpeg_data, size_t jpeg_size) {
    // Allocate buffer for entire SJPG file
    image_buffer = (uint8_t*)malloc(jpeg_size);  // ❌ For 115KB, this fails!
    
    if (!image_buffer) {
        return false;  // Out of memory
    }
    
    memcpy(image_buffer, jpeg_data, jpeg_size);
    
    // LVGL VFS points at full buffer
    vfs_jpeg_data = image_buffer;
    vfs_jpeg_size = jpeg_size;
    
    // LVGL decoder needs random access to entire file
    lv_img_set_src(img_obj, "M:mem.sjpg");
}
```

**Root cause:** LVGL's SJPG decoder requires random access (seek operations) to read strip offsets from the header, then jump to specific strips.

---

## Explored Alternatives

### Option 1: SPIFFS Instead of RAM

**Idea:** Store images in SPIFFS filesystem instead of RAM.

**Analysis:**
```
Pros:
+ Images persist across reboots
+ Smaller RAM footprint

Cons:
- Flash wear (10K-100K write cycles)
  - Every 10 min uploads = 52,560/year
  - Lifespan: 0.8-11 years depending on flash quality
- Slower (flash read: 1-10μs vs RAM: 100ns)
- File management overhead
- Not needed for temporary displays
```

**Verdict:** ❌ Not suitable for frequent temporary image updates. Flash wear is unacceptable.

---

### Option 2: Line-by-Line Raw Pixels

**Idea:** Send uncompressed pixel data one line at a time.

```
POST /api/display/line/0 → 280 pixels × 2 bytes = 560 bytes
POST /api/display/line/1 → 560 bytes
...
POST /api/display/line/239 → 560 bytes

Write each line directly to LCD, no buffering
```

**Analysis:**
```
For 280×240 screen:
- Requests: 240
- Transfer: 134KB (raw) + 48KB (HTTP overhead) = 182KB
- Time: ~12-24 seconds
- Peak RAM: 560 bytes ✅

vs Current SJPG:
- Requests: 1
- Transfer: 22KB
- Time: 0.3 seconds
- Peak RAM: 35KB ✅

Data overhead: 8.3× larger
Time overhead: 40-80× slower
```

**Verdict:** ❌ Extremely inefficient. No compression, excessive requests, very slow.

---

### Option 3: Batched Raw Pixels (10 Lines)

**Idea:** Send raw pixels in batches to reduce request count.

```
POST /api/display/chunk/0 → 10 lines = 5.6KB
POST /api/display/chunk/1 → 5.6KB
...
POST /api/display/chunk/23 → 5.6KB

For 280×240: 24 requests
For 800×480: 192 requests
```

**Analysis:**
```
For 800×480 screen:
- Requests: 192
- Transfer: 768KB (raw)
- Time: ~15 seconds
- Peak RAM: 16KB ✅

vs Strip streaming (proposed):
- Requests: 30
- Transfer: 115KB (compressed)
- Time: ~3 seconds
- Peak RAM: 15KB ✅

Data overhead: 6.7× larger
Time overhead: 5× slower
```

**Verdict:** ❌ Better than line-by-line, but still much worse than compressed approaches.

---

### Option 4: Chunked SJPG to SPIFFS

**Idea:** Upload SJPG in chunks, write to SPIFFS, then display from filesystem.

```
POST /api/display/chunk/0 → 4KB → Append to /image.sjpg
POST /api/display/chunk/1 → 4KB → Append to /image.sjpg
...
POST /api/display/chunk/N → Complete → Display from SPIFFS
```

**Analysis:**
```
For 800×480 screen:
- Requests: 29 (chunks)
- Transfer: 115KB (compressed) ✅
- Time: ~3 seconds
- Peak RAM: 17KB ✅
- Flash wear: Yes ❌ (every upload)

Pros:
+ Compressed transfer (6× smaller than raw)
+ Low RAM usage
+ Works with LVGL decoder

Cons:
- Flash wear (10K-100K cycle limit)
- SPIFFS overhead
- Slower than RAM
```

**Verdict:** ⚠️ Works, but flash wear is concerning for frequent updates.

---

### Option 5: Chunked SJPG to RAM

**Idea:** Upload SJPG in chunks, store in RAM array.

**Analysis:**
```
Whether you allocate:
- 1 × 115KB buffer
- 29 × 4KB buffers

Total RAM used: ~115KB (same!)

Does NOT solve the memory problem.
```

**Verdict:** ❌ No benefit. Still exceeds available RAM for large images.

---

## Recommended Solution: Strip-by-Strip Streaming

### The Key Insight

**Each SJPG strip is an independent JPEG image that can be decoded standalone.**

```
SJPG file structure:
┌─────────────────────────────────────────┐
│ SJPG Header (strip offsets)              │
├─────────────────────────────────────────┤
│ Strip 0: [JPEG of lines 0-15]   (1.5KB) │ ← Valid standalone JPEG!
│ Strip 1: [JPEG of lines 16-31]  (1.5KB) │ ← Valid standalone JPEG!
│ Strip 2: [JPEG of lines 32-47]  (1.5KB) │ ← Valid standalone JPEG!
│ ...                                      │
│ Strip N: [JPEG of lines ...]            │
└─────────────────────────────────────────┘
```

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│ Client / Server                                         │
│                                                          │
│  photo.jpg                                              │
│      ↓                                                  │
│  [jpg2sjpg.sh] → photo.sjpg                           │
│      ↓                                                  │
│  [extract_strips.py] → Parse SJPG, extract strips      │
│      ↓                                                  │
│  [upload_strips.py]                                    │
│      ├─ POST /api/display/strip/0 (strip 0, 1.5KB)   │
│      ├─ POST /api/display/strip/1 (strip 1, 1.5KB)   │
│      ├─ POST /api/display/strip/2 (strip 2, 1.5KB)   │
│      ...                                                │
│      └─ POST /api/display/strip/N (strip N, 1.5KB)   │
└─────────────────────────────────────────────────────────┘
                       ↓ WiFi (each strip)
┌─────────────────────────────────────────────────────────┐
│ ESP32-C3 Device                                         │
│                                                          │
│  ┌────────────────────────────────────────────┐        │
│  │ POST /api/display/strip/{n} Handler         │        │
│  │   ├─ Receive strip data (~1.5KB)           │        │
│  │   ├─ Allocate temp buffer (1.5KB)          │        │
│  │   └─ Call decode_and_display_strip()       │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ JPEG Decoder (TJpgDec)                      │        │
│  │   ├─ Decode strip to RGB888 (~13KB buffer) │        │
│  │   └─ Call output callback per MCU block     │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ Output Callback                             │        │
│  │   ├─ Convert RGB888 → BGR565                │        │
│  │   └─ Write pixels directly to LCD           │        │
│  └────────────────────────────────────────────┘        │
│                       ↓                                  │
│  ┌────────────────────────────────────────────┐        │
│  │ ST7789V2 LCD Display                        │        │
│  │   Renders BGR565 pixels                     │        │
│  └────────────────────────────────────────────┘        │
│                                                          │
│  After all strips: Free buffers, image complete!       │
└─────────────────────────────────────────────────────────┘
```

### Memory Footprint

```
Peak RAM usage (constant for ANY screen size):
- Strip upload buffer: ~1.5KB (temporary)
- JPEG decode buffer: ~13KB (for 280px width, 16px height)
- TJpgDec work area: ~3KB
Total: ~17.5KB → round up to 20KB for safety

For 280×240 screen: 20KB
For 800×480 screen: 20KB (same!)
For 1024×600 screen: 20KB (same!)
```

**This scales to ANY screen size!** ✅

---

## Implementation Guide

### Phase 1: Server-Side Strip Extraction

Create a Python tool to extract strips from SJPG files:

```python
#!/usr/bin/env python3
"""
Extract individual JPEG strips from SJPG file.
"""
import struct
import sys

def extract_sjpg_strips(sjpg_path, output_dir):
    """Parse SJPG and extract individual strips."""
    
    with open(sjpg_path, 'rb') as f:
        data = f.read()
    
    # Validate SJPG magic
    if data[0:4] != b'_SJP':
        raise ValueError("Not a valid SJPG file")
    
    # Parse header
    width = struct.unpack('>H', data[14:16])[0]
    height = struct.unpack('>H', data[16:18])[0]
    num_strips = struct.unpack('>H', data[18:20])[0]
    strip_height = struct.unpack('>H', data[20:22])[0]
    
    print(f"Image: {width}×{height}, {num_strips} strips of {strip_height}px height")
    
    # Parse strip offsets (starts at offset 22)
    offsets = []
    offset_pos = 22
    for i in range(num_strips):
        offset = struct.unpack('>I', data[offset_pos:offset_pos+4])[0]
        offsets.append(offset)
        offset_pos += 4
    
    # Extract each strip
    strips = []
    for i in range(num_strips):
        start = offsets[i]
        end = offsets[i+1] if i+1 < num_strips else len(data)
        
        strip_data = data[start:end]
        strips.append(strip_data)
        
        # Optionally save to file
        if output_dir:
            with open(f"{output_dir}/strip_{i:03d}.jpg", 'wb') as f:
                f.write(strip_data)
        
        print(f"Strip {i}: {len(strip_data)} bytes")
    
    return strips

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: extract_strips.py <sjpg_file> [output_dir]")
        sys.exit(1)
    
    sjpg_file = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else None
    
    strips = extract_sjpg_strips(sjpg_file, output_dir)
    print(f"\nExtracted {len(strips)} strips")
```

### Phase 2: Server-Side Upload Script

Create a script to upload strips to ESP32:

```python
#!/usr/bin/env python3
"""
Upload SJPG strips to ESP32 for display.
"""
import requests
import sys
from extract_strips import extract_sjpg_strips

def upload_strips(esp32_host, sjpg_file):
    """Extract strips and upload to ESP32."""
    
    # Extract strips
    strips = extract_sjpg_strips(sjpg_file, output_dir=None)
    
    print(f"\nUploading {len(strips)} strips to {esp32_host}...")
    
    # Upload each strip
    for i, strip_data in enumerate(strips):
        url = f"http://{esp32_host}/api/display/strip/{i}"
        
        try:
            response = requests.post(
                url, 
                data=strip_data,
                headers={'Content-Type': 'application/octet-stream'},
                timeout=5
            )
            
            if response.status_code == 200:
                print(f"Strip {i}/{len(strips)-1}: OK ({len(strip_data)} bytes)")
            else:
                print(f"Strip {i}: ERROR {response.status_code}")
                return False
                
        except Exception as e:
            print(f"Strip {i}: FAILED - {e}")
            return False
    
    print("\n✅ Upload complete!")
    return True

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: upload_strips.py <esp32_ip> <sjpg_file>")
        sys.exit(1)
    
    esp32_host = sys.argv[1]
    sjpg_file = sys.argv[2]
    
    success = upload_strips(esp32_host, sjpg_file)
    sys.exit(0 if success else 1)
```

### Phase 3: ESP32 Implementation

#### Add TJpgDec Decoder

```cpp
// strip_decoder.h
#ifndef STRIP_DECODER_H
#define STRIP_DECODER_H

#include <Arduino.h>

class StripDecoder {
public:
    StripDecoder();
    ~StripDecoder();
    
    // Initialize for new image
    void begin(int image_width, int image_height);
    
    // Decode and display a single strip
    bool decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index);
    
    // Complete the image display
    void end();
    
private:
    int width;
    int height;
    int current_y;  // Current Y position in image
    
    static const int STRIP_HEIGHT = 16;  // Standard SJPG strip height
    
    // JPEG decoder callback
    static unsigned int jpeg_output_callback(void* jd, void* bitmap, void* rect);
};

#endif
```

```cpp
// strip_decoder.cpp
#include "strip_decoder.h"
#include "lcd_driver.h"
#include "log_manager.h"
#include <tjpgd.h>  // TJpgDec library

// Context for JPEG decoder
struct DecodeContext {
    StripDecoder* decoder;
    int strip_y_offset;
};

StripDecoder::StripDecoder() : width(0), height(0), current_y(0) {
}

StripDecoder::~StripDecoder() {
}

void StripDecoder::begin(int image_width, int image_height) {
    width = image_width;
    height = image_height;
    current_y = 0;
    
    Logger.logMessagef("StripDecoder", "Begin %dx%d image", width, height);
}

bool StripDecoder::decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index) {
    Logger.logBegin("Decode Strip");
    Logger.logLinef("Strip %d, size: %u bytes", strip_index, jpeg_size);
    Logger.logLinef("Free heap: %u bytes", ESP.getFreeHeap());
    
    // TJpgDec structures
    JDEC jdec;
    uint8_t work[3100];  // Work area for decoder
    
    // Prepare decoder
    JRESULT res = jd_prepare(&jdec, 
        // Input function (read from buffer)
        [](JDEC* jd, uint8_t* buff, unsigned int nbyte) -> unsigned int {
            // Custom input function - read from our buffer
            // (Implementation details depend on how you pass the buffer)
            return nbyte;
        },
        work, 
        sizeof(work), 
        (void*)jpeg_data
    );
    
    if (res != JDR_OK) {
        Logger.logEndf("ERROR: jd_prepare failed: %d", res);
        return false;
    }
    
    // Decode and output
    DecodeContext ctx = {this, current_y};
    
    res = jd_decomp(&jdec, 
        // Output callback
        [](JDEC* jd, void* bitmap, JRECT* rect) -> unsigned int {
            DecodeContext* ctx = (DecodeContext*)jd->device;
            StripDecoder* decoder = ctx->decoder;
            int y_offset = ctx->strip_y_offset;
            
            // Convert RGB888 to BGR565 and write to LCD
            uint8_t* src = (uint8_t*)bitmap;
            
            for (int y = rect->top; y <= rect->bottom; y++) {
                for (int x = rect->left; x <= rect->right; x++) {
                    uint8_t r = *src++;
                    uint8_t g = *src++;
                    uint8_t b = *src++;
                    
                    // RGB → BGR swap for ST7789V2
                    uint16_t bgr565 = ((b & 0xF8) << 8) | 
                                     ((g & 0xFC) << 3) | 
                                     (r >> 3);
                    
                    // Write pixel to display
                    lcd_draw_pixel(x, y_offset + y, bgr565);
                }
            }
            
            return 1;  // Continue
        },
        0,  // Scale (0 = 1:1)
        &ctx
    );
    
    if (res != JDR_OK) {
        Logger.logEndf("ERROR: jd_decomp failed: %d", res);
        return false;
    }
    
    // Move Y position for next strip
    current_y += STRIP_HEIGHT;
    
    Logger.logLinef("Strip decoded, new Y position: %d", current_y);
    Logger.logEnd();
    
    return true;
}

void StripDecoder::end() {
    Logger.logMessagef("StripDecoder", "Image complete at Y=%d", current_y);
    current_y = 0;
}
```

#### HTTP Handler

```cpp
// In web_portal.cpp

static StripDecoder strip_decoder;
static int expected_strips = 0;
static int received_strips = 0;
static bool strip_upload_in_progress = false;

// POST /api/display/strip/{strip_index}
void handleStripUpload(AsyncWebServerRequest *request, 
                       uint8_t *data, size_t len, 
                       size_t index, size_t total) {
    
    // Get strip index from URL
    String url = request->url();
    int strip_index = url.substring(url.lastIndexOf('/') + 1).toInt();
    
    static uint8_t* strip_buffer = nullptr;
    static size_t strip_size = 0;
    
    if (index == 0) {
        // First chunk of this strip
        
        // On first strip, initialize decoder
        if (strip_index == 0) {
            strip_upload_in_progress = true;
            received_strips = 0;
            
            // TODO: Get image dimensions from first strip or separate API call
            // For now, assume full screen
            strip_decoder.begin(LCD_WIDTH, LCD_HEIGHT);
        }
        
        // Allocate buffer for this strip
        strip_buffer = (uint8_t*)malloc(total);
        if (!strip_buffer) {
            Logger.logMessage("StripUpload", "ERROR: Out of memory");
            request->send(507, "application/json", 
                "{\"success\":false,\"error\":\"Out of memory\"}");
            return;
        }
        strip_size = 0;
    }
    
    // Accumulate strip data
    if (strip_buffer && strip_size + len <= total) {
        memcpy(strip_buffer + strip_size, data, len);
        strip_size += len;
    }
    
    // Final chunk of this strip
    if (index + len >= total) {
        Logger.logMessagef("StripUpload", "Strip %d complete: %u bytes", 
            strip_index, strip_size);
        
        // Decode and display this strip
        bool success = strip_decoder.decode_strip(strip_buffer, strip_size, strip_index);
        
        // Free strip buffer
        free(strip_buffer);
        strip_buffer = nullptr;
        strip_size = 0;
        
        if (success) {
            received_strips++;
            
            // Check if this is the last strip
            // (Client should send expected count, or we detect based on image height)
            int expected = (LCD_HEIGHT + 15) / 16;  // Round up
            
            if (received_strips >= expected) {
                strip_decoder.end();
                strip_upload_in_progress = false;
                
                Logger.logMessage("StripUpload", "Image complete!");
            }
            
            request->send(200, "application/json", 
                "{\"success\":true}");
        } else {
            request->send(500, "application/json", 
                "{\"success\":false,\"error\":\"Decode failed\"}");
        }
    }
}

// Register route in setup_web_portal()
void setup_web_portal() {
    // ... existing routes ...
    
    // Strip upload endpoint
    server->on("/api/display/strip/*", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        handleStripUpload
    );
}
```

### Phase 4: Integration Notes

**TJpgDec Library:**
- Already included with LVGL (used internally by SJPG decoder)
- Located in `~/Arduino/libraries/lvgl/src/extra/libs/tjpgd/`
- May need to include directly: `#include <lvgl/src/extra/libs/tjpgd/tjpgd.h>`

**BGR Color Swap:**
- Must happen in output callback (same as SJPG preprocessing)
- Convert RGB888 → BGR565 during pixel write

**Error Handling:**
- Validate JPEG magic bytes for each strip
- Handle missing/out-of-order strips
- Timeout if strips stop arriving

---

## Adaptive Strategy

### Smart Selection Based on Available RAM

```cpp
enum UploadMode {
    MODE_SINGLE_SJPG_RAM,    // Current approach (fast)
    MODE_STRIP_STREAMING,    // Proposed approach (memory efficient)
    MODE_CHUNKED_SPIFFS      // Fallback (persistent)
};

UploadMode select_upload_mode(size_t file_size) {
    size_t free_ram = ESP.getFreeHeap();
    size_t needed_for_single = file_size + 20000;  // File + decode + safety
    
    if (free_ram > needed_for_single) {
        // Plenty of RAM: Use fast single-file upload
        Logger.logMessage("Upload", "Mode: Single SJPG to RAM (fast)");
        return MODE_SINGLE_SJPG_RAM;
    } else if (free_ram > 25000) {
        // Limited RAM but enough for streaming: Use strip-by-strip
        Logger.logMessage("Upload", "Mode: Strip streaming (memory efficient)");
        return MODE_STRIP_STREAMING;
    } else {
        // Very limited RAM: Use SPIFFS (persistent)
        Logger.logMessage("Upload", "Mode: Chunked SPIFFS (fallback)");
        return MODE_CHUNKED_SPIFFS;
    }
}
```

### Implementation

```cpp
void handleImageUpload(AsyncWebServerRequest *request, ...) {
    if (index == 0) {
        size_t total_size = request->contentLength();
        upload_mode = select_upload_mode(total_size);
    }
    
    switch (upload_mode) {
        case MODE_SINGLE_SJPG_RAM:
            handle_single_sjpg_upload(request, data, len, index, total);
            break;
            
        case MODE_STRIP_STREAMING:
            // Client must send strips separately, not full SJPG
            // This endpoint just validates and provides instructions
            request->send(400, "application/json",
                "{\"error\":\"Use strip streaming API for large images\"}");
            break;
            
        case MODE_CHUNKED_SPIFFS:
            handle_chunked_spiffs_upload(request, data, len, index, total);
            break;
    }
}
```

---

## Performance Comparison

### Small Screen (280×240 pixels)

| Approach | Requests | Transfer | RAM | Speed | Flash | Verdict |
|----------|----------|----------|-----|-------|-------|---------|
| **Single SJPG/RAM** | 1 | 22 KB | 35 KB | 0.3s | No | ✅ **Best** |
| Strip streaming | 15 | 22 KB | 15 KB | 1.5s | No | ⚠️ Slower |
| Chunked SPIFFS | 6 | 22 KB | 17 KB | 0.8s | Yes | ⚠️ Flash wear |
| Raw pixel batching | 24 | 136 KB | 5.6 KB | 3s | No | ❌ Too slow |

**Recommendation:** Keep current single SJPG/RAM approach.

---

### Large Screen (800×480 pixels)

| Approach | Requests | Transfer | RAM | Speed | Flash | Verdict |
|----------|----------|----------|-----|-------|-------|---------|
| Single SJPG/RAM | 1 | 115 KB | 153 KB | 0.8s | No | ❌ **Doesn't fit** |
| **Strip streaming** | 30 | 115 KB | 15 KB | 3s | No | ✅ **Best** |
| Chunked SPIFFS | 29 | 115 KB | 17 KB | 3s | Yes | ⚠️ Flash wear |
| Raw pixel batching | 192 | 768 KB | 16 KB | 15s | No | ❌ Too slow |

**Recommendation:** Use strip-by-strip streaming.

---

### Very Large Screen (1024×600 pixels)

| Approach | Requests | Transfer | RAM | Speed | Flash | Verdict |
|----------|----------|----------|-----|-------|-------|---------|
| Single SJPG/RAM | 1 | 180 KB | 229 KB | 1.2s | No | ❌ **Doesn't fit** |
| **Strip streaming** | 38 | 180 KB | 15 KB | 4s | No | ✅ **Best** |
| Chunked SPIFFS | 45 | 180 KB | 17 KB | 5s | Yes | ⚠️ Flash wear |
| Raw pixel batching | 240 | 1.2 MB | 20 KB | 25s | No | ❌ Too slow |

**Recommendation:** Use strip-by-strip streaming.

---

## Summary

### Current Implementation (280×240)
✅ **Keep as-is** - Single SJPG to RAM via VFS is optimal for small screens.

### Future Enhancement (Large Screens)
✅ **Implement strip-by-strip streaming** when scaling to displays >400×300 pixels:
- Constant 15-20KB RAM regardless of screen size
- Compressed transfer (6-8× smaller than raw pixels)
- No flash wear (all in RAM)
- Reasonably fast (3-5 seconds for large images)
- Scales to any display size

### Adaptive Strategy
✅ **Auto-select based on available RAM:**
- Small images that fit: Use single SJPG/RAM (fastest)
- Large images: Use strip streaming (memory efficient)
- Fallback: Chunked SPIFFS (persistent, but flash wear)

---

## References

- [Current Implementation](image-display-implementation.md) - Current SJPG/VFS approach
- [TJpgDec Library](http://elm-chan.org/fsw/tjpgd/00index.html) - JPEG decoder
- [LVGL SJPG Documentation](https://docs.lvgl.io/8/libs/sjpg.html) - Split JPEG format
- ESP32-C3 Memory: 320KB total, ~80KB available after firmware/WiFi

---

**Document version:** 1.0  
**Last updated:** December 14, 2025  
**Status:** Research & Proposal (not yet implemented)
