# Strip-Based Image Upload Implementation

## Overview

This document describes a memory-efficient image upload technique that enables large image display on resource-constrained embedded systems. By uploading and decoding JPEG strips individually rather than buffering entire files, this approach achieves **constant memory usage regardless of image size**.

**Key innovation:** Each uploaded strip is an independent **baseline JPEG fragment** that can be uploaded, decoded, and displayed separately. The ESP32 never holds the entire image file in memory.

**Target audience:** 
- Developers implementing image display on memory-constrained embedded systems
- Anyone needing to display images larger than available RAM
- Systems where image preprocessing on the client side is acceptable

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Solution Architecture](#solution-architecture)
3. [Implementation Details](#implementation-details)
4. [API Specification](#api-specification)
5. [Client Tools](#client-tools)
6. [Generic Pattern](#generic-pattern)
7. [Performance Analysis](#performance-analysis)
8. [Debugging Guide](#debugging-guide)
9. [References](#references)

---

## Problem Statement

### The Memory Scaling Challenge

Traditional image display approaches buffer the entire compressed file in RAM before decoding:

```
Traditional approach:
1. Upload full image file (e.g., ~115KB JPEG)
2. Store in RAM buffer (115KB allocated)
3. Decoder reads from buffer
4. Decode strips one at a time (~15KB decode buffer)

Total RAM: 115KB + 15KB = 130KB
```

**Problem:** As screen resolution increases, file size grows proportionally. Eventually the file alone exceeds available RAM.

### Memory Requirements by Screen Size

| Screen Size | JPEG File | Single-File RAM | Strip-Based RAM | Savings |
|-------------|-----------|-----------------|-----------------|---------|
| 240×280 | 22 KB | 35 KB | 15 KB | 57% |
| 320×240 | 28 KB | 43 KB | 15 KB | 65% |
| 400×300 | 38 KB | 57 KB | 18 KB | 68% |
| 480×320 | 55 KB | 78 KB | 20 KB | 74% |
| **800×480** | **115 KB** | **138 KB** ❌ | **23 KB** ✅ | **83%** |
| 1024×600 | 180 KB | 229 KB ❌ | 26 KB ✅ | 89% |

**Key insight:** Strip-based upload maintains constant memory usage (~15-30KB) regardless of total image size.

---

## Solution Architecture

### The Core Concept

**Independent JPEG fragments** can be decoded separately:

```
Conceptual Structure:
┌─────────────────────────────────────────┐
│ Strip 0: [Complete JPEG, lines 0-31]    │ ← Standalone JPEG!
│ Strip 1: [Complete JPEG, lines 32-63]   │ ← Standalone JPEG!
│ Strip 2: [Complete JPEG, lines 64-95]   │ ← Standalone JPEG!
│ ...                                      │
│ Strip N: [Complete JPEG, final lines]   │ ← Standalone JPEG!
└─────────────────────────────────────────┘

Each strip is a valid JPEG image of size: [width] × [strip_height]
```

### End-to-End Flow

```
┌──────────────────────────────────────────────────────────────┐
│ Client / Server Preprocessing                                │
│                                                               │
│  1. Original image (photo.jpg, RGB)                          │
│      ↓                                                       │
│  2. Split into strips and encode each strip as baseline JPEG  │
│      ↓                                                       │
│  3. Upload each strip via HTTP                               │
│     POST /api/display/image/chunks?index=0&total=N&width=W&height=H │
│     POST /api/display/image/chunks?index=1&total=N&width=W&height=H │
│     ...                                                      │
│     POST /api/display/image/chunks?index=N-1&total=N&width=W&height=H│
└──────────────────────────────────────────────────────────────┘
                          ↓ WiFi (each strip independently)
┌──────────────────────────────────────────────────────────────┐
│ ESP32-C3 Device (per strip processing)                       │
│                                                               │
│  ┌─────────────────────────────────────────┐                │
│  │ HTTP Handler (AsyncWebServer Task)      │                │
│  │  1. Receive strip JPEG data (~1-3KB)    │                │
│  │  2. Allocate temporary buffer            │                │
│  │  3. Validate JPEG magic bytes            │                │
│  │  4. Call display_decode_strip()          │                │
│  │  5. Free strip buffer                    │                │
│  └─────────────────────────────────────────┘                │
│                     ↓                                         │
│  ┌─────────────────────────────────────────┐                │
│  │ StripDecoder::decode_strip()             │                │
│  │  1. Allocate work buffer (~4KB)          │                │
│  │  2. Allocate line buffer (~480 bytes)    │                │
│  │  3. Setup JPEG decoder (TJpgDec)         │                │
│  │  4. Call jd_prepare() → parse JPEG       │                │
│  │  5. Call jd_decomp() → decode pixels     │                │
│  │  6. Update current_y position            │                │
│  │  7. Free buffers                          │                │
│  └─────────────────────────────────────────┘                │
│                     ↓                                         │
│  ┌─────────────────────────────────────────┐                │
│  │ TJpgDec Callbacks                        │                │
│  │  • jpeg_input_func(): Read JPEG data    │                │
│  │  • jpeg_output_func(): Write RGB pixels  │                │
│  │    - Convert RGB888 → RGB565             │                │
│  │    - Write directly to LCD               │                │
│  └─────────────────────────────────────────┘                │
│                     ↓                                         │
│  ┌─────────────────────────────────────────┐                │
│  │ LCD Driver (ST7789V2)                    │                │
│  │  Renders BGR565 pixels to display        │                │
│  └─────────────────────────────────────────┘                │
│                                                               │
│  After last strip: Image complete! Free all buffers.         │
└──────────────────────────────────────────────────────────────┘
```

### Memory Footprint (Per Strip)

```
Peak RAM usage during strip decode (constant for ANY image size):
┌──────────────────────────────────┬────────┐
│ Strip upload buffer              │ 1-3 KB │ (temporary during HTTP receive)
│ TJpgDec work buffer              │ 4 KB   │ (JPEG decode workspace)
│ Line buffer (width × 2 bytes)    │ 0.5 KB │ (for pixel conversion)
│ JPEG decode internal buffers     │ 8 KB   │ (TJpgDec internal)
│ Stack + overhead                 │ 2 KB   │ (function calls, etc.)
├──────────────────────────────────┼────────┤
│ TOTAL PER STRIP                  │ ~15 KB │ ✅ Constant!
└──────────────────────────────────┴────────┘

Compare to single-file buffering:
- 240×280 image: 22KB file + 13KB decode = 35KB
- 800×480 image: 115KB file + 23KB decode = 138KB ❌ (doesn't fit ESP32-C3)

Strip-based stays at ~15KB regardless of total image size! ✅
```

---

## Implementation Details

### 1. Client-Side Strip Encoding

Each request body is a standalone baseline JPEG fragment.

Instead, the client splits the source image into horizontal strips and encodes **each strip** as an independent **baseline JPEG fragment**. Each fragment is uploaded to the device via:

`POST /api/display/image/chunks?index=N&total=T&width=W&height=H&timeout=seconds`

This keeps the device memory usage constant and avoids buffering an entire image file.

**Strip size trade-offs:**

| Strip Height | Strips (280px) | Strip Size | Overhead | Decode RAM | Speed |
|--------------|----------------|------------|----------|------------|-------|
| 8px | 35 | ~600 bytes | High | 8 KB | Slower (more requests) |
| 16px | 18 | ~1.2 KB | Medium | 11 KB | Balanced ✅ |
| 32px | 9 | ~2.5 KB | Low | 23 KB | Faster (fewer requests) |
| 64px | 5 | ~5 KB | Very low | 46 KB | Fastest (but more RAM) |

**Recommendation:** 16-32px strikes best balance between memory usage, compression efficiency, and upload speed.

---

### 2. Client Tooling

The repository includes a reference uploader that performs the split-and-encode step and uploads each fragment in order:

```bash
python3 tools/upload_image.py 192.168.1.111 photo.jpg --mode strip --strip-height 32
```

Notes:
- The tool encodes fragments as **baseline JPEG** with **4:2:0** chroma subsampling by default (TJpgDec-friendly).
- Clients should send **normal RGB JPEG input**; no client-side RGB↔BGR swapping is required.

Metadata is provided via query parameters; the firmware accepts raw JPEG fragments directly.

---

### 3. ESP32 Strip Decoder

**Core decoder class (strip_decoder.cpp):**

```cpp
class StripDecoder {
public:
    void begin(int image_width, int image_height);
    bool decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index);
    void end();

private:
    int width;        // Image width
    int height;       // Total image height
    int current_y;    // Current Y position (updated after each strip)
};

bool StripDecoder::decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index) {
    // Allocate work buffer for TJpgDec
    void* work = malloc(4096);  // ~4KB workspace
    
    // Allocate line buffer for pixel conversion
    uint16_t* line_buffer = (uint16_t*)malloc(width * sizeof(uint16_t));
    
    // Setup session context (shared between input/output callbacks)
    JpegSessionContext session_ctx;
    session_ctx.input.data = jpeg_data;
    session_ctx.input.size = jpeg_size;
    session_ctx.input.pos = 0;
    session_ctx.output.decoder = this;
    session_ctx.output.strip_y_offset = current_y;  // Current display Y position
    session_ctx.output.line_buffer = line_buffer;
    session_ctx.output.buffer_width = width;
    
    // Prepare JPEG decoder
    JDEC jdec;
    JRESULT res = jd_prepare(&jdec, jpeg_input_func, work, 4096, &session_ctx);
    if (res != JDR_OK) return false;
    
    // Decode JPEG → RGB888 → output callback
    res = jd_decomp(&jdec, jpeg_output_func, 0);  // 0 = 1:1 scale
    if (res != JDR_OK) return false;
    
    // Update Y position for next strip (using actual decoded height)
    current_y += jdec.height;  // Auto-adapts to any strip height! ✅
    
    // Free buffers
    free(line_buffer);
    free(work);
    
    return true;
}
```

**Key insight:** The decoder reads the actual strip dimensions from the JPEG header during `jd_prepare()`, so it automatically adapts to any strip height without hardcoded values.

---

### 4. TJpgDec Integration

**Critical bug fix: Shared context for both callbacks**

TJpgDec uses a single `device` pointer for the entire decode session. Both input and output callbacks must access their state through this same pointer.

**WRONG (causes crashes):**

```cpp
// ❌ BAD: Separate contexts, device pointer gets overwritten
JpegInputContext input_ctx;
JpegOutputContext output_ctx;

jd_prepare(&jdec, jpeg_input_func, work, work_size, &input_ctx);  // device = &input_ctx
jd_decomp(&jdec, jpeg_output_func, 0, &output_ctx);               // device = &output_ctx ❌

// During jd_decomp():
// - Input callback receives &output_ctx (WRONG TYPE!) → crash at strip 2+
```

**CORRECT (stable):**

```cpp
// ✅ GOOD: Single session context containing both
struct JpegSessionContext {
    JpegInputContext input;
    JpegOutputContext output;
};

JpegSessionContext session_ctx;
// ... initialize session_ctx.input and session_ctx.output ...

jd_prepare(&jdec, jpeg_input_func, work, work_size, &session_ctx);  // device = &session_ctx
jd_decomp(&jdec, jpeg_output_func, 0);  // Uses same device pointer ✅

// Both callbacks cast device → JpegSessionContext*
static size_t jpeg_input_func(JDEC* jd, uint8_t* buff, size_t ndata) {
    JpegSessionContext* session = (JpegSessionContext*)jd->device;
    JpegInputContext* ctx = &session->input;  // ✅ Access input sub-context
    // ... read from ctx->data + ctx->pos ...
}

static int jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegSessionContext* session = (JpegSessionContext*)jd->device;
    JpegOutputContext* ctx = &session->output;  // ✅ Access output sub-context
    // ... write RGB pixels to LCD ...
}
```

**Why this matters:**
- Small strips (0-1) fit in TJpgDec's internal buffer during `jd_prepare()`, so `jd_decomp()` doesn't need to read more data
- Large strips (2+) require additional reads during `jd_decomp()`, triggering the input callback
- If the device pointer was overwritten, the input callback receives the wrong context type → **Load access fault crash**

---

### 5. Output Callback: RGB888 → RGB565/BGR565 Packing

TJpgDec outputs RGB888. The firmware packs pixels to RGB565 (or BGR565) based on the active display pipeline.

```cpp
static int jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegSessionContext* session = (JpegSessionContext*)jd->device;
    JpegOutputContext* ctx = &session->output;
    
    uint8_t* src = (uint8_t*)bitmap;  // RGB888 from TJpgDec
    uint16_t* line = ctx->line_buffer;
    
    // Pack RGB888 → RGB565/BGR565 (selectable)
    for (int y = rect->top; y <= rect->bottom; y++) {
        int pixel_count = rect->right - rect->left + 1;
        
        for (int i = 0; i < pixel_count; i++) {
            uint8_t r = *src++;  // Red
            uint8_t g = *src++;  // Green
            uint8_t b = *src++;  // Blue
            
            // Example: pack as BGR565 (swap red and blue)
            line[i] = ((b & 0xF8) << 8) |   // Blue → bits 15-11
                      ((g & 0xFC) << 3) |   // Green → bits 10-5
                      (r >> 3);              // Red → bits 4-0
        }
        
        // Write line to LCD
        int lcd_y = ctx->strip_y_offset + y;
        lcd_draw_pixels(rect->left, lcd_y, pixel_count, line);
    }
    
    return 1;  // Continue decoding
}
```

---

## API Specification

### Strip Upload Endpoint

```
POST /api/display/image/chunks
Query Parameters:
  - index:   Strip index (0-based)
  - total:   Total number of strips in image
  - width:   Image width in pixels
  - height:  Image height in pixels
  - timeout: Display timeout in seconds (optional, default: 10)

Body: Raw JPEG data (binary)

Response: 200 OK
{
  "success": true,
  "strip": 0,
  "total": 9,
  "message": "Strip 0/9 decoded"
}

Response: 500 Error
{
  "success": false,
  "message": "Decode failed"
}
```

**Example request:**

```bash
curl -X POST \
    "http://192.168.1.111/api/display/image/chunks?index=0&total=9&width=240&height=280&timeout=10" \
  --data-binary "@strip_0.jpg" \
  -H "Content-Type: application/octet-stream"
```

### State Management

**First strip (index=0):**
- Initializes `StripDecoder::begin(width, height)`
- Resets `current_y = 0`
- Prepares for new image session

**Middle strips:**
- Decodes JPEG
- Writes to LCD at `current_y` position
- Advances `current_y += decoded_height`

**Last strip (index=total-1):**
- Completes decode
- Calls `StripDecoder::end()`
- Logs "All strips decoded"

---

## Client Tools

### Upload Script

```bash
ESP32_IP="192.168.1.111"
IMAGE="test240x280.jpg"

# Upload as sequential JPEG fragments (strip/chunk mode)
python3 tools/upload_image.py "$ESP32_IP" "$IMAGE" --mode strip --strip-height 32 --timeout 10
```
**Usage:**

```bash
# Upload full image
python3 tools/upload_image.py 192.168.1.111 photo.jpg --mode strip --strip-height 32 --timeout 10

# Upload specific strips (for debugging)
python3 tools/upload_image.py 192.168.1.111 photo.jpg --mode strip --strip-height 32 --timeout 10 --start 2 --end 2
```

---

## Generic Pattern

### When to Use Strip-Based Upload

**✅ Use strip-based upload when:**
- Total file size > 50% of available RAM
- Display resolution > 400×300 pixels on ESP32-C3
- Need predictable memory usage
- Images are preprocessed on server/PC

**❌ Don't use strip-based upload when:**
- Image fits comfortably in RAM (faster with single upload)
- Client can't preprocess images
- Network latency is very high (>100ms per request)

### Adapting to Other Formats

**The pattern works for any splittable format:**

#### Example: PNG with scanline streaming

```python
# Server-side: Extract PNG scanlines
from PIL import Image
import zlib

img = Image.open('photo.png')
pixels = img.load()

for y in range(img.height):
    # Extract one line of RGB pixels
    line = bytes([pixels[x, y][c] for x in range(img.width) for c in range(3)])
    
    # Compress line (optional)
    compressed = zlib.compress(line, level=6)
    
    # Upload line
    requests.post(f"{esp32}/api/display/line/{y}", data=compressed)
```

```cpp
// ESP32: Receive and decode line
void handleLineUpload(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    int line_y = get_line_from_url(request->url());
    
    // Decompress line (if compressed)
    uint8_t line_buffer[width * 3];
    decompress(data, len, line_buffer);
    
    // Convert RGB → RGB565 and write to LCD
    for (int x = 0; x < width; x++) {
        uint8_t r = line_buffer[x*3];
        uint8_t g = line_buffer[x*3 + 1];
        uint8_t b = line_buffer[x*3 + 2];
        
        uint16_t bgr565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
        lcd_draw_pixel(x, line_y, bgr565);
    }
}
```

#### Example: JPEG strip streaming

```python
# Server: split an image into strips and upload each as a baseline JPEG fragment
def upload_jpeg_strips(esp32, image_file, strip_height=32, timeout=10):
    img = Image.open(image_file).convert('RGB')
    total = math.ceil(img.height / strip_height)

    for idx, y in enumerate(range(0, img.height, strip_height)):
        strip = img.crop((0, y, img.width, min(y + strip_height, img.height)))

        # Encode strip as baseline JPEG (device decodes each fragment independently)
        buf = io.BytesIO()
        strip.save(buf, format='JPEG', quality=90, subsampling=2, progressive=False)

        requests.post(
            f"{esp32}/api/display/image/chunks",
            params={"index": idx, "total": total, "width": img.width, "height": img.height, "timeout": timeout},
            headers={"Content-Type": "application/octet-stream"},
            data=buf.getvalue(),
        )
```

### Key Requirements for Adaptation

1. **Format must be splittable** (scanlines, tiles, strips)
2. **Each chunk processable independently** (no dependencies on previous chunks)
3. **Client-side splitting/re-encoding acceptable** (generate baseline JPEG fragments on PC/server)
4. **Sequential write pattern** (top-to-bottom for displays)

---

## Performance Analysis

### Timing Breakdown (240×280 image, 32px strips, 9 total)

```
Per-strip timeline:
┌─────────────────────┬────────┐
│ HTTP transfer       │ 50 ms  │ (1.5KB over WiFi)
│ JPEG decode         │ 80 ms  │ (TJpgDec processing)
│ LCD write           │ 30 ms  │ (SPI transfer to display)
│ Buffer cleanup      │ 5 ms   │ (free() calls)
├─────────────────────┼────────┤
│ Per-strip total     │ 165 ms │
└─────────────────────┴────────┘

Total for 9 strips: 165ms × 9 = 1.48 seconds
```

**Compare to single-file upload (same image):**

```
Single-file timeline:
┌─────────────────────┬────────┐
│ HTTP transfer       │ 300 ms │ (22KB over WiFi)
│ JPEG decode + blit   │ 200 ms │ (decode + LCD write)
│ Buffer cleanup      │ 5 ms   │
├─────────────────────┼────────┤
│ Total               │ 505 ms │
└─────────────────────┴────────┘
```

**Result:** Chunked uploads are typically slower for small images (more HTTP overhead), but keep memory usage predictable and can start drawing earlier.

### Network Considerations

**Request overhead:**

```
Strip-based (9 strips):
- HTTP requests: 9
- Total headers: ~4.5KB (9 × 500 bytes)
- Payload: 22KB (JPEG data)
- Total transfer: 26.5KB

Single-file:
- HTTP requests: 1
- Total headers: ~500 bytes
- Payload: 22KB
- Total transfer: 22.5KB

Overhead: +17% for strip-based (acceptable trade-off for memory savings)
```

### Scalability

**Large image (800×480, 32px strips, 15 total):**

```
Memory usage:
- Single-file: 115KB + 23KB = 138KB ❌ Doesn't fit ESP32-C3
- Strip-based: 3KB + 23KB = 26KB ✅ Fits comfortably

Upload time:
- Strip-based: 165ms × 15 = 2.5 seconds
- (Single-file N/A - doesn't work)

Network transfer:
- Strips: 115KB payload + 7.5KB headers = 122.5KB
- Overhead: +6.5% (minimal)
```

**Verdict:** Strip-based enables images that are literally impossible with single-file buffering.

---

## Debugging Guide

### Common Issues

#### 1. Crash at Strip 2+

**Symptom:**
```
Strip 0: ✓
Strip 1: ✓
Strip 2: Load access fault at 0x7f99xxxx
```

**Cause:** Device pointer corruption (input/output contexts not shared).

**Fix:** Use `JpegSessionContext` containing both input and output contexts. See [TJpgDec Integration](#4-tjpgdec-integration).

---

#### 2. Colors Swapped (Red ↔ Blue)

**Symptom:** Red objects appear blue, blue appears red.

**Cause:** Pixel packing mismatch (RGB565 vs BGR565).

**Fix (current):** Firmware handles this internally; upload a normal RGB JPEG (no client-side channel swapping).

**Fix (implementation detail):** The decoder output callback packs either RGB565 or BGR565 based on a per-session flag.

```cpp
// ✅ Correct (BGR565)
uint16_t bgr = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);

// ❌ Wrong (RGB565)
uint16_t rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
```

---

#### 3. White Screen / Garbage Display

**Symptom:** Display shows white screen or random artifacts.

**Cause:** Invalid JPEG data or decode failure.

**Debug steps:**

```cpp
// Add logging to decode_strip()
Logger.logLinef("JPEG magic: %02X %02X %02X %02X", 
    jpeg_data[0], jpeg_data[1], jpeg_data[2], jpeg_data[3]);
// Should be: FF D8 FF E0 (JPEG SOI marker)

// Check jd_prepare result
if (res != JDR_OK) {
    Logger.logLinef("jd_prepare failed: %d", res);
    // JDR_INP (2) = invalid data
    // JDR_FMT1 (6) = format error
}
```

**Validation:** Each strip request body is a standalone baseline JPEG fragment. Use `tools/upload_image.py` with `--start/--end` to isolate failures.

---

#### 4. Image Shifted Vertically

**Symptom:** Image displays but appears shifted down or has gaps.

**Cause:** `current_y` not updating correctly.

**Fix:** Ensure Y position advances by actual decoded height:

```cpp
// ✅ Correct (dynamic)
current_y += jdec.height;  // Use actual decoded height

// ❌ Wrong (hardcoded)
current_y += 32;  // Breaks if strip height changes
```

---

#### 5. Out of Memory

**Symptom:**
```
ERROR: Failed to allocate work buffer
Free heap: 12543 bytes
```

**Cause:** Insufficient RAM for decode buffers.

**Debug:** Check heap before each allocation:

```cpp
Logger.logLinef("Free heap before work: %u", ESP.getFreeHeap());
void* work = malloc(4096);
if (!work) {
    Logger.logLinef("ERROR: malloc failed, heap: %u", ESP.getFreeHeap());
}
```

**Solutions:**
- Reduce work buffer size (try 3072 bytes minimum)
- Reduce strip height to decrease decode buffer
- Free other memory before upload (hide previous image, etc.)

---

### Diagnostic Logging

**Comprehensive logging for debugging:**

```cpp
bool StripDecoder::decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index) {
    Logger.logBegin("Strip");
    Logger.logLinef("Strip %d: Y=%d, Size=%u, Heap=%u", 
                   strip_index, current_y, jpeg_size, ESP.getFreeHeap());
    
    // Log JPEG header
    Logger.logLinef("JPEG: %02X %02X %02X %02X", 
                   jpeg_data[0], jpeg_data[1], jpeg_data[2], jpeg_data[3]);
    
    // ... allocations ...
    
    Logger.logLinef("Work buffer: %p (%d bytes)", work, work_size);
    Logger.logLinef("Line buffer: %p (%d bytes)", line_buffer, width * 2);
    
    // ... decode ...
    
    Logger.logLinef("JPEG decoded: %dx%d", jdec.width, jdec.height);
    Logger.logLinef("✓ New Y: %d", current_y);
    Logger.logEnd();
    
    return true;
}
```

---

## References

### Related Documentation

- [Image Display Implementation](image-display-implementation.md) - Direct-to-LCD baseline JPEG APIs
- [LVGL Display System](lvgl-display-system.md) - LVGL integration details
- [Multi-Board Support](multi-board-support.md) - ESP32 vs ESP32-C3 differences

### External Resources

- [TJpgDec Library](http://elm-chan.org/fsw/tjpgd/00index.html) - JPEG decoder documentation
- [LVGL Documentation](https://docs.lvgl.io/8/) - UI framework reference
- [ST7789V2 Datasheet](https://www.newhavendisplay.com/appnotes/datasheets/LCDs/ST7789V2.pdf) - Display controller

### Project Files

**Client tooling:**
- [tools/upload_image.py](../../tools/upload_image.py) - Reference uploader for `/api/display/image` and `/api/display/image/chunks`

**Client tooling:**

Use the reference uploader for both endpoints:
- [tools/upload_image.py](../../tools/upload_image.py)

**ESP32 implementation:**
- [src/app/strip_decoder.h](../../src/app/strip_decoder.h) - Strip decoder interface
- [src/app/strip_decoder.cpp](../../src/app/strip_decoder.cpp) - TJpgDec integration
- [src/app/web_portal.cpp](../../src/app/web_portal.cpp) - HTTP API handler
- [src/app/screen_direct_image.cpp](../../src/app/screen_direct_image.cpp) - LVGL display integration

---

## Summary

### Key Innovations

1. **Constant memory usage** - 15-30KB regardless of image size
2. **Automatic strip height adaptation** - Firmware reads actual dimensions from JPEG
3. **Shared context pattern** - Prevents TJpgDec device pointer corruption
4. **Client-side splitting/re-encoding** - Strips are generated client-side as baseline JPEG fragments
5. **Graceful degradation** - Can fall back to single-file for small images

### When to Use

**✅ Strip-based upload:**
- Images > 50KB on ESP32-C3
- Display resolution > 400×300
- Need predictable memory footprint
- Client-side splitting/re-encoding acceptable

**✅ Single-file upload:**
- Images < 40KB
- Need fastest possible display time
- Client can't preprocess
- Display ≤ 320×240

### Performance Characteristics

| Metric | Single-File | Strip-Based | Winner |
|--------|-------------|-------------|---------|
| **RAM usage (small)** | 35 KB | 15 KB | Strip |
| **RAM usage (large)** | 138 KB ❌ | 23 KB ✅ | Strip |
| **Upload speed** | 0.5s | 1.5s | Single |
| **Network overhead** | +0% | +17% | Single |
| **Scalability** | Limited | Unlimited | Strip |
| **Preprocessing** | None | Required | Single |
| **Display rotation** | ⚠️ Manual pre-rotate | ⚠️ Manual pre-rotate | Tie |

### Display Rotation

Both `/api/display/image` and `/api/display/image/chunks` render **direct-to-LCD** in **raw panel coordinates**.

That means **no LVGL rotation is applied** for either endpoint. Clients should upload images already rotated for the panel orientation.

Practical guidance:
- Prefer generating images at the physical panel size (**240×280**) matching how you want it to appear.
- If you change board rotation settings for the UI, do not assume it affects these image APIs.

**Example for landscape mode (LCD_ROTATION=1):**

```bash
# ✅ Correct - rotate content, but keep output at raw panel size
convert photo.jpg -rotate 90 -resize 240x280 rotated.jpg
python3 upload_image.py 192.168.1.111 rotated.jpg --mode strip

# ❌ Wrong - assumes UI rotation affects the image API
python3 upload_image.py 192.168.1.111 photo.jpg --mode strip
```

---

### Rotation Summary

| Upload Method | Rotation Handling | Image Dimensions | Notes |
|---------------|-------------------|------------------|-------|
| **Single-file** | ⚠️ Manual pre-rotate (client-side) | Raw panel (`LCD_WIDTH`×`LCD_HEIGHT`) | Rendered direct-to-LCD (no LVGL rotation) |
| **Strip-based** | ⚠️ Manual pre-rotate (client-side) | Raw panel (`LCD_WIDTH`×`LCD_HEIGHT`) | Rendered direct-to-LCD (no LVGL rotation) |

**Recommendation:** Generate images for the device's raw panel dimensions (`LCD_WIDTH`×`LCD_HEIGHT`). Do not assume UI rotation affects these image APIs.

### Generic Applicability

This pattern applies to any embedded system with:
- Limited RAM (<1MB)
- Network/serial upload capability  
- Sequential pixel write pattern (displays)
- Client-side preprocessing option

**Adaptable to:** PNG scanlines, BMP strips, tile-based formats, progressive JPEG, video frames, etc.

---

**Document version:** 1.0  
**Last updated:** December 15, 2025  
**Status:** ✅ Implemented and tested

