# Image Display API

Display custom JPEG images on the ESP32 Energy Monitor's 1.69" LCD screen via REST API.

## Features

- **JPEG Support**: Upload JPEG images up to 100KB
- **Auto-Timeout**: Configurable display timeout (default: 10 seconds, 0=permanent, max: 24 hours)
- **Manual Dismiss**: DELETE endpoint to immediately return to power screen
- **Memory Safe**: Works within ESP32-C3's 320KB RAM constraints
- **MQTT Friendly**: Power monitoring continues in background during image display

## API Endpoints

### Upload and Display Image

```http
POST /api/display/image?timeout=<seconds>
Content-Type: multipart/form-data

<JPEG image file>
```

**Query Parameters:**
- `timeout` (optional): Display timeout in seconds (0=permanent, 1-86400, default: 10)

**Response (Success):**
```json
{
  "success": true,
  "message": "Image queued for display (30s timeout)"
}
```

**Response (Error):**
```json
{
  "success": false,
  "message": "Image too large (max 100KB)"
}
```

**HTTP Status Codes:**
- `200` - Image uploaded and displayed successfully
- `400` - Invalid request (not a JPEG, missing data)
- `507` - Insufficient memory

### Manually Dismiss Image

```http
DELETE /api/display/image
```

**Response:**
```json
{
  "success": true,
  "message": "Image dismissed"
}
```

## Usage Examples

### Using cURL

**Upload image (default 10 second timeout):**
```bash
curl -X POST -F "image=@photo.jpg" http://192.168.1.100/api/display/image
```

**Upload with custom timeout (30 seconds):**
```bash
curl -X POST -F "image=@photo.jpg" 'http://192.168.1.100/api/display/image?timeout=30'
```

**Upload with permanent display (no auto-dismiss):**
```bash
curl -X POST -F "image=@photo.jpg" 'http://192.168.1.100/api/display/image?timeout=0'
```

**Dismiss image:**
```bash
curl -X DELETE http://192.168.1.100/api/display/image
```

### Using Python

```python
import requests

# Upload image with default timeout
with open('photo.jpg', 'rb') as f:
    files = {'image': ('photo.jpg', f, 'image/jpeg')}
    response = requests.post('http://192.168.1.100/api/display/image', files=files)
    print(response.json())

# Upload image with custom timeout (60 seconds)
with open('photo.jpg', 'rb') as f:
    files = {'image': ('photo.jpg', f, 'image/jpeg')}
    response = requests.post('http://192.168.1.100/api/display/image?timeout=60', files=files)
    print(response.json())

# Dismiss image
response = requests.delete('http://192.168.1.100/api/display/image')
print(response.json())
```

### Using the Upload Tool

```bash
python3 tools/upload_image.py 192.168.1.100 photo.jpg --mode single
```

## Image Requirements

### Recommended Specifications

- **Format**: JPEG (.jpg, .jpeg)
- **Size**: Under 50KB recommended, 100KB maximum
- **Resolution**: 240×280 pixels (physical display dimensions)
- **Color**: RGB color (24-bit)
- **Quality**: 70-80% JPEG quality provides good balance

**Note:** Images are drawn in **raw panel coordinates**. Upload in **240×280** (the physical panel size).

### Creating Compatible Images

**Using ImageMagick:**
```bash
# Resize to 240×280 (physical panel dimensions)
convert input.jpg -resize 240x280 -quality 75 output.jpg
```

**Using Python (Pillow):**
```python
from PIL import Image

img = Image.open('input.jpg')
# Resize to physical dimensions (240×280)
img = img.resize((240, 280), Image.LANCZOS)
img.save('output.jpg', 'JPEG', quality=75, optimize=True)
```

## Behavior

### Display Flow

1. **Upload**: Client sends JPEG via POST request with optional timeout parameter
2. **Validation**: Device checks file size, magic bytes (JPEG header)
3. **Memory Check**: Ensures sufficient heap available (~50KB headroom)
4. **Decode & Display**: JPEG decoder renders image directly to the LCD (timer starts before decoding)
5. **Timeout**: After configured duration (default: 10 seconds), automatically returns to power screen
6. **Cleanup**: Image buffer freed from RAM

**Note on timeout timing:** The timeout timer starts when the upload completes (before image decoding begins). This ensures you get the full requested display time, accounting for the 1-3 seconds of JPEG decode time.

### Image Scaling

- **Currently requires exact 240×280** for `/api/display/image`

### Memory Usage

| Component | Memory (bytes) |
|-----------|----------------|
| JPEG file (compressed) | 3,000 - 50,000 |
| Upload buffer | Same as file size |
| JPEG decoder working | ~4,000 |
| **Peak usage** | **~10-60 KB** |

## Troubleshooting

### Upload Fails with "Image too large"

**Problem**: JPEG file exceeds 100KB limit  
**Solution**: Reduce image quality or resolution

```bash
# Reduce quality
convert input.jpg -quality 60 output.jpg

# Reduce resolution first
convert input.jpg -resize 240x280 -quality 75 output.jpg
```

### Upload Fails with "Insufficient memory"

**Problem**: Device running low on RAM  
**Solution**: 
1. Wait for device to stabilize after boot
2. Reduce image size to <30KB
3. Check `/api/health` endpoint for `heap_free` status

### Upload Fails with "Invalid JPEG file"

**Problem**: File is not a valid JPEG or corrupted  
**Solution**:
1. Verify file is JPEG format (not PNG renamed to .jpg)
2. Re-save image with image editor
3. Check JPEG header: `hexdump -C image.jpg | head -1` should show `FF D8 FF`

### Upload Fails with "Unsupported JPEG"

**Problem**: The device supports a subset of baseline JPEG encodings. Some JPEGs (especially progressive, or uncommon chroma sampling) will be rejected.

**Solution**:
1. Re-save as **baseline JPEG** (not progressive)
2. Prefer standard chroma subsampling (e.g. 4:2:0)
3. Ensure RGB (avoid CMYK)

```bash
# Force baseline JPEG
convert input.jpg -interlace none -sampling-factor 4:2:0 -quality 75 output.jpg
```

### Display Timeout

**Problem**: Need custom display duration  
**Solution**: Use the `timeout` query parameter:

```bash
# Display for 30 seconds
curl -F 'image=@photo.jpg' 'http://192.168.1.111/api/display/image?timeout=30'

# Display for 2 minutes
curl -F 'image=@photo.jpg' 'http://192.168.1.111/api/display/image?timeout=120'
```

**Range**: 1-300 seconds (5 minutes maximum)  
**Default**: 10 seconds if not specified

## Use Cases

### Digital Photo Frame
Display family photos or artwork for ambient decoration

### QR Code Display
Show temporary QR codes for guest WiFi, payment, etc.

### Status Dashboards
Render custom graphs or charts from external services

### Notification Display
Show visual alerts (weather warnings, package delivery, etc.)

### Demo Mode
Display product information at tradeshows or retail

## Performance

| Metric | Value |
|--------|-------|
| Upload time (5KB JPEG, WiFi) | ~1-2 seconds |
| Decode time (240×280 JPEG) | ~1-3 seconds |
| Total latency | ~3-5 seconds |
| Auto-dismiss timeout | 10 seconds (default), 0=permanent, 1-86400 seconds (24 hours max) |

## Security Considerations

### Current Implementation (LAN-only)

- ✅ File size validation (100KB limit)
- ✅ MIME type validation (JPEG magic bytes)
- ✅ Memory safety checks (heap monitoring)
- ⚠️ **No authentication** (suitable for trusted networks only)

### Recommendations for Internet-Exposed Devices

If exposing to internet, add:
1. HTTP Basic Auth or API key authentication
2. Rate limiting (max 1 upload per 5 seconds)
3. IP whitelist
4. HTTPS (TLS) for encrypted transport

## Technical Details

### LVGL Integration

- The UI uses LVGL, but uploaded images are rendered **direct-to-LCD** using TJpgDec
- Screen managed by `DirectImageScreen`

### Memory Management

- Upload buffer: `malloc()` during upload, freed after display
- Image buffer: `malloc()` in `ImageScreen`, freed on timeout
- No flash storage (RAM-only, lost on reboot)
- Heap monitoring prevents out-of-memory crashes

### Timeout Implementation

- Timer starts when upload completes (before image decode), captured in `web_portal.cpp`
- Start time passed through to `DirectImageScreen` via `set_start_time()`
- Checked every frame in `display_update()`
- Automatically returns to the previous screen after configured timeout
- Returns to previous screen (typically power screen)
- **Accurate timing**: Full display duration available regardless of decode time (1-3 seconds)
- **Permanent display**: Setting `timeout=0` disables auto-dismiss (image stays until manually dismissed)

## Future Enhancements

Potential improvements (not yet implemented):

- [x] ~~Configurable timeout via API parameter~~ ✅ Implemented
- [ ] PNG support (requires more RAM)
- [ ] Image persistence to flash (LittleFS)
- [ ] Multiple image slideshow mode
- [ ] Brightness adjustment for images
- [ ] Image preview/thumbnail API
- [ ] Authentication/authorization
- [ ] Image rotation/flip options

## API Version

**Introduced in**: v1.3.0 (December 2025)  
**Endpoint**: `/api/display/image`  
**Status**: Stable
