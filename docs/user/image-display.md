# Image Display API

Display custom JPEG images on the ESP32 Energy Monitor's 1.69" LCD screen via REST API.

## Features

- **JPEG Support**: Upload JPEG images up to 100KB
- **Auto-Timeout**: Images display for 10 seconds then auto-return to power screen
- **Manual Dismiss**: DELETE endpoint to immediately return to power screen
- **Memory Safe**: Works within ESP32-C3's 320KB RAM constraints
- **MQTT Friendly**: Power monitoring continues in background during image display

## API Endpoints

### Upload and Display Image

```http
POST /api/display/image
Content-Type: multipart/form-data

<JPEG image file>
```

**Response (Success):**
```json
{
  "success": true,
  "message": "Image displayed (10s timeout)"
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

**Upload image:**
```bash
curl -X POST -F "image=@photo.jpg" http://192.168.1.100/api/display/image
```

**Dismiss image:**
```bash
curl -X DELETE http://192.168.1.100/api/display/image
```

### Using Python

```python
import requests

# Upload image
with open('photo.jpg', 'rb') as f:
    files = {'image': ('photo.jpg', f, 'image/jpeg')}
    response = requests.post('http://192.168.1.100/api/display/image', files=files)
    print(response.json())

# Dismiss image
response = requests.delete('http://192.168.1.100/api/display/image')
print(response.json())
```

### Using Test Scripts

**Bash:**
```bash
./tools/test-image-api.sh 192.168.1.100 photo.jpg
```

**Python:**
```bash
python3 tools/test_image_api.py 192.168.1.100 photo.jpg
```

## Image Requirements

### Recommended Specifications

- **Format**: JPEG (.jpg, .jpeg)
- **Size**: Under 50KB recommended, 100KB maximum
- **Resolution**: 240×280 pixels (matches screen exactly)
- **Color**: RGB color (24-bit)
- **Quality**: 70-80% JPEG quality provides good balance

### Creating Compatible Images

**Using ImageMagick:**
```bash
# Resize and optimize for device
convert input.jpg -resize 240x280 -quality 75 output.jpg
```

**Using Python (Pillow):**
```python
from PIL import Image

img = Image.open('input.jpg')
img = img.resize((240, 280), Image.LANCZOS)
img.save('output.jpg', 'JPEG', quality=75, optimize=True)
```

## Behavior

### Display Flow

1. **Upload**: Client sends JPEG via POST request
2. **Validation**: Device checks file size, magic bytes (JPEG header)
3. **Memory Check**: Ensures sufficient heap available (~50KB headroom)
4. **Decode & Display**: SJPG decoder renders image to screen
5. **Timeout**: After 10 seconds, automatically returns to power screen
6. **Cleanup**: Image buffer freed from RAM

### Image Scaling

- **Exact match (240×280)**: Displayed pixel-perfect
- **Smaller images**: Centered with black bars
- **Larger images**: May be clipped at edges (implementation-dependent)

### Memory Usage

| Component | Memory (bytes) |
|-----------|----------------|
| JPEG file (compressed) | 3,000 - 50,000 |
| Upload buffer | Same as file size |
| SJPG decoder working | ~4,000 |
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

### Image Displays Corrupted

**Problem**: SJPG decoder incompatibility  
**Solution**:
1. Re-save JPEG as "baseline" (not progressive)
2. Use JPEG quality 70-90% (avoid extreme compression)
3. Ensure RGB color space (not CMYK or grayscale)

```bash
# Force baseline JPEG
convert input.jpg -interlace none -quality 75 output.jpg
```

### Timeout Too Short/Long

**Problem**: 10-second hardcoded timeout  
**Solution**: Currently fixed in firmware. To change:
1. Edit `src/app/screen_image.h` line 37: `DISPLAY_TIMEOUT_MS`
2. Rebuild and flash firmware

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
| Auto-dismiss timeout | 10 seconds |

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

- Uses LVGL's SJPG decoder (`LV_USE_SJPG`)
- Images rendered to `lv_img_t` objects
- Screen managed by `ImageScreen` class
- Automatic color space conversion (RGB ↔ BGR for ST7789V2)

### Memory Management

- Upload buffer: `malloc()` during upload, freed after display
- Image buffer: `malloc()` in `ImageScreen`, freed on timeout
- No flash storage (RAM-only, lost on reboot)
- Heap monitoring prevents out-of-memory crashes

### Timeout Implementation

- Timer starts on `ImageScreen::show()`
- Checked every frame in `display_update()`
- Automatically calls `display_hide_image()` after 10s
- Returns to previous screen (typically power screen)

## Future Enhancements

Potential improvements (not yet implemented):

- [ ] Configurable timeout via API parameter
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
