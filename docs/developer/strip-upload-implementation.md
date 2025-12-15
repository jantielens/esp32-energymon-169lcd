# Strip-Based Image Upload Implementation

## Overview

This document describes a memory-efficient image upload technique that enables large image display on resource-constrained embedded systems. By uploading and decoding JPEG strips individually rather than buffering entire files, this approach achieves **constant memory usage regardless of image size**.

**Key innovation:** Each SJPG strip is an independent JPEG that can be uploaded, decoded, and displayed separately. The ESP32 never holds the entire image file in memory.

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
1. Upload full image file (e.g., 115KB SJPG)
2. Store in RAM buffer (115KB allocated)
3. Decoder reads from buffer
4. Decode strips one at a time (~15KB decode buffer)

Total RAM: 115KB + 15KB = 130KB
```

**Problem:** As screen resolution increases, file size grows proportionally. Eventually the file alone exceeds available RAM.

### Memory Requirements by Screen Size

| Screen Size | SJPG File | Single-File RAM | Strip-Based RAM | Savings |
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

**SJPG files contain independent JPEG strips** that can be decoded separately:

```
SJPG File Structure:
┌─────────────────────────────────────────┐
│ Header: Width, Height, Strip Count      │ 22-40 bytes
├─────────────────────────────────────────┤
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
│  2. Color swap R↔B for BGR display (jpg2sjpg.sh)           │
│      ↓                                                       │
│  3. Split into JPEG strips (jpg_to_sjpg.py)                 │
│     → photo.sjpg (contains N independent JPEGs)             │
│      ↓                                                       │
│  4. Extract individual strips (upload_strips.py)             │
│     Parses SJPG header, extracts strip offsets               │
│      ↓                                                       │
│  5. Upload each strip via HTTP                               │
│     POST /api/display/strip?index=0&total=N&width=W&height=H │
│     POST /api/display/strip?index=1&total=N&width=W&height=H │
│     ...                                                      │
│     POST /api/display/strip?index=N-1&total=N&width=W&height=H│
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
│  │    - Convert RGB888 → BGR565             │                │
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

### 1. SJPG Strip Encoding

**Strip size is configured in the encoder script:**

```python
# tools/jpg_to_sjpg.py
JPEG_SPLIT_HEIGHT = 32  # Pixels per strip (configurable: 8, 16, 32, 64...)

# The script:
# 1. Splits source image into horizontal slices of JPEG_SPLIT_HEIGHT
# 2. Encodes each slice as independent JPEG
# 3. Packages JPEGs into SJPG container with header
# 4. Header includes: width, height, strip_count, strip_height, offsets
```

**Strip size trade-offs:**

| Strip Height | Strips (280px) | Strip Size | Overhead | Decode RAM | Speed |
|--------------|----------------|------------|----------|------------|-------|
| 8px | 35 | ~600 bytes | High | 8 KB | Slower (more requests) |
| 16px | 18 | ~1.2 KB | Medium | 11 KB | Balanced ✅ |
| 32px | 9 | ~2.5 KB | Low | 23 KB | Faster (fewer requests) |
| 64px | 5 | ~5 KB | Very low | 46 KB | Fastest (but more RAM) |

**Recommendation:** 16-32px strikes best balance between memory usage, compression efficiency, and upload speed.

---

### 2. Client-Side Strip Extraction

**Python script to parse SJPG and extract strips:**

```python
# tools/upload_strips.py
import struct

def parse_sjpg(sjpg_file):
    """Parse SJPG file format and extract strips."""
    with open(sjpg_file, 'rb') as f:
        data = f.read()
    
    # Validate magic bytes
    if data[0:4] != b'_SJP':
        raise ValueError("Invalid SJPG file")
    
    # Parse header (little-endian)
    width = struct.unpack('<H', data[14:16])[0]
    height = struct.unpack('<H', data[16:18])[0]
    num_strips = struct.unpack('<H', data[18:20])[0]
    block_height = struct.unpack('<H', data[20:22])[0]
    
    print(f"SJPG: {width}x{height}, {num_strips} strips ({block_height}px each)")
    
    # Parse strip offsets (2 bytes per offset, starting at byte 22)
    offsets = []
    pos = 22
    for i in range(num_strips):
        offset = struct.unpack('<H', data[pos:pos+2])[0]
        offsets.append(offset)
        pos += 2
    
    # Header size = 22 + (num_strips * 2)
    header_size = pos
    
    # Extract each strip (JPEG data between offsets)
    strips = []
    for i in range(num_strips):
        start = header_size + offsets[i]
        if i + 1 < num_strips:
            end = header_size + offsets[i + 1]
        else:
            end = len(data)
        
        strip_jpeg = data[start:end]
        strips.append(strip_jpeg)
        print(f"  Strip {i}: {len(strip_jpeg)} bytes (offset {offsets[i]})")
    
    return width, height, block_height, strips
```

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

### 5. Output Callback: RGB → BGR Conversion

**TJpgDec outputs RGB888, but ST7789V2 displays expect BGR565:**

```cpp
static int jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegSessionContext* session = (JpegSessionContext*)jd->device;
    JpegOutputContext* ctx = &session->output;
    
    uint8_t* src = (uint8_t*)bitmap;  // RGB888 from TJpgDec
    uint16_t* line = ctx->line_buffer;
    
    // Convert RGB888 → BGR565 (swap R and B during packing)
    for (int y = rect->top; y <= rect->bottom; y++) {
        int pixel_count = rect->right - rect->left + 1;
        
        for (int i = 0; i < pixel_count; i++) {
            uint8_t r = *src++;  // Red
            uint8_t g = *src++;  // Green
            uint8_t b = *src++;  // Blue
            
            // Pack as BGR565 (swap red and blue)
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
POST /api/display/strip
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
  "http://192.168.1.111/api/display/strip?index=0&total=9&width=240&height=280&timeout=10" \
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
#!/bin/bash
# tools/test-strip-api.sh
ESP32_IP="192.168.1.111"
IMAGE="test240x280.jpg"
TIMEOUT=10

# 1. Convert to SJPG (with BGR color swap)
./jpg2sjpg.sh "$IMAGE" temp.sjpg

# 2. Upload strips
python3 upload_strips.py "$ESP32_IP" temp.sjpg "$TIMEOUT"

# 3. Cleanup
rm temp.sjpg
```

**Python uploader (upload_strips.py):**

```python
def upload_strips(esp32_ip, sjpg_file, timeout=10, start_strip=None, end_strip=None):
    width, height, block_height, strips = parse_sjpg(sjpg_file)
    
    # Determine strip range
    first = start_strip if start_strip is not None else 0
    last = end_strip if end_strip is not None else len(strips) - 1
    
    print(f"\nUploading strips {first}-{last} to {esp32_ip}...")
    
    for i in range(first, last + 1):
        strip_data = strips[i]
        
        # Build URL with metadata
        url = f"http://{esp32_ip}/api/display/strip"
        params = {
            'index': i,
            'total': len(strips),
            'width': width,
            'height': height,
            'timeout': timeout
        }
        
        # Upload strip
        response = requests.post(
            url,
            params=params,
            data=strip_data,
            headers={'Content-Type': 'application/octet-stream'},
            timeout=5
        )
        
        if response.status_code == 200:
            print(f"  Strip {i}/{len(strips)-1}: ✓ ({len(strip_data)} bytes)")
        else:
            print(f"  Strip {i}: ✗ HTTP {response.status_code}")
            return False
    
    print("\n✅ Upload complete!")
    return True
```

**Usage:**

```bash
# Upload full image
python3 upload_strips.py 192.168.1.111 photo.sjpg 10

# Upload specific strips (for debugging)
python3 upload_strips.py 192.168.1.111 photo.sjpg 10 2 2  # Only strip 2
python3 upload_strips.py 192.168.1.111 photo.sjpg 10 0 5  # Strips 0-5
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
    
    // Convert RGB → BGR565 and write to LCD
    for (int x = 0; x < width; x++) {
        uint8_t r = line_buffer[x*3];
        uint8_t g = line_buffer[x*3 + 1];
        uint8_t b = line_buffer[x*3 + 2];
        
        uint16_t bgr565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
        lcd_draw_pixel(x, line_y, bgr565);
    }
}
```

#### Example: BMP strip streaming

```python
# Server: Split BMP into strips
def split_bmp(bmp_file, strip_height=32):
    img = Image.open(bmp_file)
    
    for i in range(0, img.height, strip_height):
        strip = img.crop((0, i, img.width, min(i + strip_height, img.height)))
        
        # Save as raw RGB565
        rgb565_data = convert_to_rgb565(strip)
        
        # Upload strip
        requests.post(f"{esp32}/api/display/strip/{i//strip_height}", 
                     data=rgb565_data)
```

### Key Requirements for Adaptation

1. **Format must be splittable** (scanlines, tiles, strips)
2. **Each chunk processable independently** (no dependencies on previous chunks)
3. **Client-side preprocessing acceptable** (extraction/conversion on PC/server)
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
│ VFS setup           │ 5 ms   │ (pointer assignment)
│ LVGL SJPG decode    │ 200 ms │ (all strips)
│ Buffer cleanup      │ 5 ms   │
├─────────────────────┼────────┤
│ Total               │ 510 ms │
└─────────────────────┴────────┘
```

**Result:** Strip-based is ~3× slower for small images, but **enables large images that wouldn't fit at all**.

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

**Cause:** Display expects BGR565 but receiving RGB565.

**Fix:** Swap R and B during pixel packing in output callback:

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

**Validation:** Extract strip from SJPG and verify it's a valid JPEG:

```bash
python3 extract_strip.py test240x280.sjpg 2 strip2.jpg
file strip2.jpg  # Should say: "JPEG image data..."
```

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

- [Image Display Implementation](image-display-implementation.md) - Single-file SJPG approach (for small images)
- [LVGL Display System](lvgl-display-system.md) - LVGL integration details
- [Multi-Board Support](multi-board-support.md) - ESP32 vs ESP32-C3 differences

### External Resources

- [TJpgDec Library](http://elm-chan.org/fsw/tjpgd/00index.html) - JPEG decoder documentation
- [LVGL SJPG Format](https://docs.lvgl.io/8/libs/sjpg.html) - Split JPEG specification
- [ST7789V2 Datasheet](https://www.newhavendisplay.com/appnotes/datasheets/LCDs/ST7789V2.pdf) - Display controller

### Project Files

**Server-side tools:**
- [tools/jpg_to_sjpg.py](../../tools/jpg_to_sjpg.py) - SJPG encoder (configurable strip height)
- [tools/jpg2sjpg.sh](../../tools/jpg2sjpg.sh) - Wrapper with BGR color swap
- [tools/upload_strips.py](../../tools/upload_strips.py) - Strip upload client
- [tools/extract_strip.py](../../tools/extract_strip.py) - SJPG strip extractor (debugging)

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
4. **Client-side preprocessing** - All splitting/conversion done on PC/server
5. **Graceful degradation** - Can fall back to single-file for small images

### When to Use

**✅ Strip-based upload:**
- Images > 50KB on ESP32-C3
- Display resolution > 400×300
- Need predictable memory footprint
- Client-side preprocessing acceptable

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
| **Display rotation** | ✅ LVGL auto-rotate | ⚠️ Manual pre-rotate | Single |

### Critical Difference: Display Rotation

**⚠️ IMPORTANT:** The two upload methods handle display rotation differently!

#### Single-File SJPG (via LVGL)

Uses LVGL's image widget (`lv_img_set_src()`), which **automatically respects LVGL rotation settings**:

```cpp
// board_config.h
#define LCD_ROTATION 1  // 1 = 90° landscape mode

// display_manager.cpp  
disp_drv.sw_rotate = 1;
disp_drv.rotated = LV_DISP_ROT_90;

// screen_image.cpp
lv_img_set_src(img_obj, "M:mem.sjpg");  // ✅ Auto-rotates to landscape
```

**Result:** Upload a **240×280 portrait** image, LVGL automatically rotates it to **280×240 landscape** display.

**Image preparation:**
- Create image in **physical display dimensions** (240×280 portrait)
- LVGL handles rotation automatically based on `LCD_ROTATION` setting
- No manual rotation needed

---

#### Strip-Based Upload (Direct LCD Write)

Writes pixels **directly to LCD hardware** via `lcd_push_pixels_at()`, which **bypasses LVGL rotation**:

```cpp
// strip_decoder.cpp - Output callback
lcd_push_pixels_at(lcd_x, lcd_y, line_width, 1, line_buffer);
// ⚠️ Pixels written exactly as provided - NO rotation!
```

**Result:** Pixels are written to LCD in **exact coordinates** provided - no rotation applied.

**Image preparation:**
- Must pre-rotate image to match **display orientation**
- If `LCD_ROTATION = 1` (90° landscape), upload **280×240 landscape** image
- If `LCD_ROTATION = 0` (portrait), upload **240×280 portrait** image
- Manual rotation required before upload

**Example for landscape mode (LCD_ROTATION=1):**

```bash
# ✅ Correct - pre-rotate to landscape
convert photo.jpg -rotate 90 -resize 280x240 rotated.jpg
python3 upload_image.py 192.168.1.111 rotated.jpg --mode strip

# ❌ Wrong - will display sideways
python3 upload_image.py 192.168.1.111 photo.jpg --mode strip
```

---

### Rotation Summary

| Upload Method | Rotation Handling | Image Dimensions | Notes |
|---------------|-------------------|------------------|-------|
| **Single-file SJPG** | ✅ Automatic (LVGL) | Physical (240×280) | LVGL rotates based on `LCD_ROTATION` |
| **Strip-based** | ⚠️ Manual pre-rotate | Logical (280×240 for landscape) | Must match display orientation |

**Recommendation:** If your display uses `LCD_ROTATION != 0`, **prefer single-file upload** for images unless memory constraints force strip-based mode.

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

