# Web Portal REST API Reference

Complete API documentation for the ESP32 Energy Monitor web portal. All endpoints return JSON responses with proper HTTP status codes.

## Base URL

All API endpoints are relative to the device's base URL:

- **WiFi Connected**: `http://<device-ip>/api/` or `http://<device-name>.local/api/`
- **AP Mode**: `http://192.168.4.1/api/`

## Authentication

**Current Implementation:** No authentication required.

> **Security Note:** This is suitable for local/trusted networks only. For production use, consider adding HTTP Basic Auth or API keys.

## Device Information

### `GET /api/info`

Returns comprehensive device information including hardware specs, firmware version, and network identity.

**Response:**
```json
{
  "version": "1.2.0",
  "build_date": "Dec 13 2025",
  "build_time": "14:30:00",
  "chip_model": "ESP32-C3",
  "chip_revision": 4,
  "chip_cores": 1,
  "cpu_freq": 160,
  "flash_chip_size": 4194304,
  "psram_size": 0,
  "free_heap": 250000,
  "sketch_size": 1048576,
  "free_sketch_space": 2097152,
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "wifi_hostname": "energy-monitor",
  "mdns_name": "energy-monitor.local",
  "hostname": "energy-monitor"
}
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `version` | string | Firmware version (from `src/version.h`) |
| `build_date` | string | Compilation date |
| `build_time` | string | Compilation time |
| `chip_model` | string | ESP32 variant (ESP32, ESP32-C3, etc.) |
| `chip_revision` | number | Silicon revision |
| `chip_cores` | number | Number of CPU cores |
| `cpu_freq` | number | CPU frequency in MHz |
| `flash_chip_size` | number | Flash size in bytes |
| `psram_size` | number | PSRAM size in bytes (0 if not available) |
| `free_heap` | number | Available RAM in bytes |
| `sketch_size` | number | Firmware size in bytes |
| `free_sketch_space` | number | Available flash for OTA updates |
| `mac_address` | string | Device MAC address |
| `wifi_hostname` | string | WiFi/DHCP hostname |
| `mdns_name` | string | Full mDNS name (hostname.local) |
| `hostname` | string | Short hostname |

**Use Cases:**
- Device discovery and identification
- Firmware version checking
- Hardware capability detection
- OTA update size validation

## Health Monitoring

### `GET /api/health`

Returns real-time device health statistics including CPU, memory, temperature, and WiFi signal.

**Response:**
```json
{
  "uptime_seconds": 3600,
  "reset_reason": "Power On",
  "cpu_usage": 15,
  "cpu_freq": 160,
  "temperature": 42,
  "heap_free": 250000,
  "heap_min": 240000,
  "heap_size": 327680,
  "heap_fragmentation": 5,
  "flash_used": 1048576,
  "flash_total": 3145728,
  "wifi_rssi": -45,
  "wifi_channel": 6,
  "ip_address": "192.168.1.100",
  "hostname": "energy-monitor"
}
```

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `uptime_seconds` | number | Seconds since boot |
| `reset_reason` | string | Reason for last reset/reboot |
| `cpu_usage` | number | CPU usage percentage (0-100) |
| `cpu_freq` | number | Current CPU frequency in MHz |
| `temperature` | number/null | Internal temperature in °C (null if not supported) |
| `heap_free` | number | Free heap RAM in bytes |
| `heap_min` | number | Minimum free heap since boot |
| `heap_size` | number | Total heap size in bytes |
| `heap_fragmentation` | number | Heap fragmentation percentage (0-100) |
| `flash_used` | number | Flash memory used in bytes |
| `flash_total` | number | Total flash memory in bytes |
| `wifi_rssi` | number/null | WiFi signal strength in dBm (null if not connected) |
| `wifi_channel` | number/null | WiFi channel (null if not connected) |
| `ip_address` | string/null | Current IP address (null if not connected) |
| `hostname` | string | Device hostname |

**Notes:**
- `temperature`: Only available on ESP32-C3, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C6, ESP32-H2
- WiFi fields are `null` when device is in AP mode or not connected
- CPU usage calculated using FreeRTOS IDLE task monitoring
- Updates reflect real-time state (polled by web portal every 5-10 seconds)

**Use Cases:**
- System monitoring dashboards
- Performance tracking
- Memory leak detection
- WiFi signal quality monitoring

## Display Control

### `GET /api/brightness`

Returns current LCD backlight brightness level.

**Response:**
```json
{
  "brightness": 75
}
```

**Fields:**
- `brightness` (number): Current brightness (0-100%)

**Notes:**
- Returns real-time brightness value
- Value may differ from saved config if adjusted without saving

### `POST /api/brightness`

Set LCD backlight brightness in real-time (does not persist to NVS).

**Request Body:**
```json
{
  "brightness": 50
}
```

**Or plain text:**
```
50
```

**Response:**
```json
{
  "success": true
}
```

**Parameters:**
- `brightness` (number): Brightness level (0-100%)
  - Values outside range are clamped
  - 0 = Display off
  - 100 = Maximum brightness

**Notes:**
- Changes apply **immediately** to hardware
- **Not persisted** - value resets on reboot unless saved via `/api/config`
- Used for real-time slider updates in web UI
- To persist, include `lcd_brightness` in `/api/config` POST

**Example:**
```bash
# Get current brightness
curl http://energy-monitor.local/api/brightness

# Set brightness to 75%
curl -X POST http://energy-monitor.local/api/brightness -d "75"

# Or JSON format
curl -X POST http://energy-monitor.local/api/brightness \
  -H "Content-Type: application/json" \
  -d '{"brightness": 75}'
```

## Configuration Management

### `GET /api/config`

Returns current device configuration (passwords excluded for security).

**Response:**
```json
{
  "wifi_ssid": "MyNetwork",
  "wifi_password": "",
  "device_name": "Energy Monitor",
  "device_name_sanitized": "energy-monitor",
  "fixed_ip": "",
  "subnet_mask": "",
  "gateway": "",
  "dns1": "",
  "dns2": "",
  "mqtt_broker": "192.168.1.100",
  "mqtt_port": 1883,
  "mqtt_username": "esp32",
  "mqtt_password": "",
  "mqtt_topic_solar": "home/solar/power",
  "mqtt_topic_grid": "home/grid/power",
  "mqtt_solar_value_path": ".",
  "mqtt_grid_value_path": ".",
  "lcd_brightness": 100,
  "grid_threshold_0": 0.0,
  "grid_threshold_1": 0.5,
  "grid_threshold_2": 2.5,
  "home_threshold_0": 0.5,
  "home_threshold_1": 1.0,
  "home_threshold_2": 2.0,
  "solar_threshold_0": 0.5,
  "solar_threshold_1": 1.5,
  "solar_threshold_2": 3.0,
  "color_good": 65280,
  "color_ok": 16777215,
  "color_attention": 16753920,
  "color_warning": 16711680
}
```

**Notes:**
- Passwords always return empty string (security)
- `device_name_sanitized`: Read-only, auto-generated for mDNS
- `mqtt_solar_value_path` / `mqtt_grid_value_path`: JSON field name or `.` for direct values
- Color values are RGB565 format as decimal integers
- Empty strings indicate field not set (using defaults)

### `POST /api/config`

Save new configuration. Device reboots after successful save (unless `no_reboot` parameter is set).

**Request Body (Partial Update):**
```json
{
  "wifi_ssid": "NewNetwork",
  "wifi_password": "password123",
  "device_name": "Living Room Monitor"
}
```

**Request Body (Full Configuration):**
```json
{
  "wifi_ssid": "NewNetwork",
  "wifi_password": "password123",
  "device_name": "Living Room Monitor",
  "fixed_ip": "192.168.1.100",
  "subnet_mask": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns1": "8.8.8.8",
  "dns2": "8.8.4.4",
  "mqtt_broker": "192.168.1.50",
  "mqtt_port": 1883,
  "mqtt_username": "esp32",
  "mqtt_password": "secret",
  "mqtt_topic_solar": "home/solar/power",
  "mqtt_topic_grid": "home/grid/power",
  "mqtt_solar_value_path": ".",
  "mqtt_grid_value_path": "value",
  "lcd_brightness": 75,
  "grid_threshold_0": 0.0,
  "grid_threshold_1": 0.5,
  "grid_threshold_2": 2.5,
  "home_threshold_0": 0.5,
  "home_threshold_1": 1.0,
  "home_threshold_2": 2.0,
  "solar_threshold_0": 0.5,
  "solar_threshold_1": 1.5,
  "solar_threshold_2": 3.0,
  "color_good": 65280,
  "color_ok": 16777215,
  "color_attention": 16753920,
  "color_warning": 16711680
}
```

**Value Path Options:**
- `mqtt_solar_value_path`: `.` for direct values, or JSON field name (e.g., `power`)
- `mqtt_grid_value_path`: `.` for direct values, or JSON field name (e.g., `value`)

**Query Parameters:**
- `no_reboot=1`: Skip automatic reboot after saving

**Response:**
```json
{
  "success": true,
  "message": "Configuration saved"
}
```

**Behavior:**
- **Partial Updates**: Only fields present in request are updated
- **Password Handling**: 
  - Empty string = no change
  - Non-empty = update password
- **Automatic Reboot**: Device reboots after save (unless `no_reboot=1`)
- **Validation**: Invalid values rejected with error response

**Examples:**

```bash
# Update WiFi settings (with reboot)
curl -X POST http://energy-monitor.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"wifi_ssid":"NewNetwork","wifi_password":"password123"}'

# Update thresholds without rebooting
curl -X POST http://energy-monitor.local/api/config?no_reboot=1 \
  -H "Content-Type: application/json" \
  -d '{"grid_threshold_2":3.0}'

# Update MQTT broker
curl -X POST http://energy-monitor.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"mqtt_broker":"192.168.1.50","mqtt_port":1883}'
```

### `DELETE /api/config`

Reset configuration to factory defaults. Device reboots in AP mode after reset.

**Response:**
```json
{
  "success": true,
  "message": "Configuration reset to factory defaults"
}
```

**Notes:**
- All settings cleared (WiFi, MQTT, thresholds, etc.)
- Device reboots into Access Point mode
- No automatic reconnection - manual reconnection to AP required
- Cannot be undone - backup configuration before resetting

**Example:**
```bash
curl -X DELETE http://energy-monitor.local/api/config
```

## Portal Mode

### `GET /api/mode`

Returns current portal operating mode (AP or WiFi connected).

**Response:**
```json
{
  "mode": "full",
  "ap_active": false
}
```

**Modes:**
- `"core"`: AP mode with captive portal (192.168.4.1)
- `"full"`: WiFi connected mode (normal operation)

**Fields:**
- `mode` (string): Current operating mode
- `ap_active` (boolean): Whether Access Point is active

**Use Cases:**
- Determine available features (some only in full mode)
- UI adaptation based on mode
- Network diagnostics

## Image Display

### `POST /api/display/image`

Upload and display a **plain baseline JPEG** image on the LCD with configurable timeout.

**Request:**
- **Content-Type**: `multipart/form-data`
- **Field name**: `image`
- **File**: JPEG file
- **Max size**: 100KB
- **Query parameter**: `timeout` (optional) - Display duration in seconds

**Query Parameters:**
- `timeout` (number, optional): Display timeout in seconds
  - `0` = Permanent display (no auto-dismiss)
  - `1-86400` = Timeout in seconds (max 24 hours)
  - Default: `10` seconds
  - Timer starts when upload completes (accounts for 1-3s decode time)

**Response (Success):**
```json
{
  "success": true,
  "message": "Image queued for display (30s timeout)"
}
```

**Response (Error - Invalid Format):**
```json
{
  "success": false,
  "message": "Invalid JPEG file"
}
```

**Response (Error - Insufficient Memory):**
```json
{
  "success": false,
  "message": "Insufficient memory: need 35KB, have 28KB. Try reducing image size."
}
```

**Notes:**
- Supported format: JPEG (0xFF 0xD8 0xFF)
- Images stored in RAM only (lost on reboot)
- JPEG decoding supports a subset of baseline JPEG encodings (progressive JPEG is rejected)
- Image dimensions must match the panel (currently `240×280`)
- No client-side RGB↔BGR conversion is required for this API
- Concurrent uploads: second upload waits up to 1 second for first to complete
- After timeout, display returns to power screen automatically
- `timeout=0` makes image permanent until manually dismissed

**Examples:**
```bash
# Display image with default 10 second timeout
curl -X POST -F "image=@photo.jpg" http://energy-monitor.local/api/display/image

# Display image for 30 seconds
curl -X POST -F "image=@photo.jpg" 'http://energy-monitor.local/api/display/image?timeout=30'

# Permanent display (no auto-dismiss)
curl -X POST -F "image=@photo.jpg" 'http://energy-monitor.local/api/display/image?timeout=0'

# Display for 1 hour
curl -X POST -F "image=@photo.jpg" 'http://energy-monitor.local/api/display/image?timeout=3600'

# Using the provided upload tool
cd tools
python3 upload_image.py 192.168.1.100 photo.jpg --mode single
```

### `POST /api/display/image/strips`

Upload and display an image as a sequence of **independent JPEG strips**.

**Request:**
- **Content-Type**: `application/octet-stream`
- **Body**: a single JPEG strip (baseline JPEG)
- **Query parameters**:
  - `strip_index` (required): 0-based strip index (must be uploaded in order)
  - `strip_count` (required): total number of strips
  - `width` (required): full image width
  - `height` (required): full image height
  - `timeout` (optional): display timeout in seconds

**Notes:**
- Each request is stateless/atomic: the device allocates a buffer for that request, decodes, then frees it.
- The device advances the vertical offset based on each decoded fragment's height, so fragments must be sent sequentially.

**Example:**
```bash
curl -X POST \
  --data-binary @strip0.jpg \
  -H 'Content-Type: application/octet-stream' \
  'http://energy-monitor.local/api/display/image/strips?strip_index=0&strip_count=9&width=240&height=280&timeout=10'
```

### `DELETE /api/display/image`

Manually dismiss the currently displayed image and return to power screen.

**Response:**
```json
{
  "success": true,
  "message": "Image dismissed"
}
```

**Notes:**
- Works for both timed and permanent (`timeout=0`) images
- No effect if no image is currently displayed
- Immediately returns to power screen

**Example:**
```bash
curl -X DELETE http://energy-monitor.local/api/display/image
```

**See Also:**
- [Image Display User Guide](../user/image-display.md) - Usage examples and troubleshooting
- [Image Display Implementation](image-display-implementation.md) - Technical deep-dive
- [Home Assistant Integration](../user/home-assistant-integration.md) - Camera snapshot automation

## System Control

### `POST /api/reboot`

Reboot the device without saving configuration changes.

**Response:**
```json
{
  "success": true,
  "message": "Rebooting device..."
}
```

**Notes:**
- Device reboots immediately
- No configuration changes saved
- Web portal polls for reconnection after ~5 seconds
- Use after testing temporary changes

**Example:**
```bash
curl -X POST http://energy-monitor.local/api/reboot
```

## Firmware Updates

### `POST /api/update`

Upload new firmware binary for over-the-air (OTA) update.

**Request:**
- **Content-Type**: `multipart/form-data`
- **Field name**: `update`
- **File**: Firmware `.bin` file

**Response (Success):**
```json
{
  "success": true,
  "message": "Update successful! Rebooting..."
}
```

**Response (Error):**
```json
{
  "success": false,
  "message": "Update failed: Not enough space"
}
```

**Error Messages:**
- `"Update failed: Not enough space"` - Firmware too large for OTA partition
- `"Update failed: Invalid firmware"` - Corrupted or wrong binary format
- `"Update failed: Flash write error"` - Hardware flash error

**Notes:**
- Only `.bin` files accepted
- File size validated against available OTA partition space
- Device automatically reboots after successful update
- Progress logged to serial monitor
- Web portal shows upload progress bar

**Example (curl):**
```bash
curl -X POST http://energy-monitor.local/api/update \
  -F "update=@build/esp32c3/app.ino.bin"
```

**Example (JavaScript):**
```javascript
const formData = new FormData();
formData.append('update', fileInput.files[0]);

fetch('/api/update', {
  method: 'POST',
  body: formData
})
.then(response => response.json())
.then(data => {
  if (data.success) {
    console.log('Update successful!');
  }
});
```

## Color Format

Power threshold colors are stored as RGB565 decimal integers:

**RGB565 Format:**
- 16-bit color (5 bits red, 6 bits green, 5 bits blue)
- Stored as decimal integer in API

**Common Colors:**

| Color | Hex | RGB565 Decimal |
|-------|-----|----------------|
| Green | `#00FF00` | 2016 (or 65280 for full green) |
| White | `#FFFFFF` | 65535 |
| Orange | `#FFA500` | 64512 |
| Red | `#FF0000` | 63488 |

**Default Theme:**
- Good: 65280 (Green `#00FF00`)
- OK: 16777215 (White `#FFFFFF`)
- Attention: 16753920 (Orange `#FFA500`)
- Warning: 16711680 (Red `#FF0000`)

**Converting Hex to RGB565:**
```python
def hex_to_rgb565(hex_color):
    r = (int(hex_color[1:3], 16) >> 3) & 0x1F
    g = (int(hex_color[3:5], 16) >> 2) & 0x3F
    b = (int(hex_color[5:7], 16) >> 3) & 0x1F
    return (r << 11) | (g << 5) | b
```

## Error Responses

All endpoints return appropriate HTTP status codes:

**Success:**
- `200 OK` - Request successful
- `204 No Content` - Successful with no response body

**Client Errors:**
- `400 Bad Request` - Invalid request format or parameters
- `404 Not Found` - Endpoint doesn't exist
- `413 Payload Too Large` - File upload too large

**Server Errors:**
- `500 Internal Server Error` - Server-side error
- `503 Service Unavailable` - Service temporarily unavailable

**Example Error Response:**
```json
{
  "success": false,
  "error": "Invalid brightness value",
  "message": "Brightness must be between 0 and 100"
}
```

## Rate Limiting

**Current Implementation:** No rate limiting.

**Recommendations for Production:**
- Implement rate limiting on configuration endpoints
- Throttle health monitoring requests
- Add exponential backoff for failed requests

## CORS

**Current Implementation:** No CORS headers.

**Notes:**
- API accessible only from same-origin requests
- For cross-origin access, CORS headers would need to be added

## Automation Examples

### Monitor Device Health

```bash
#!/bin/bash
# Check device health every 60 seconds

while true; do
  curl -s http://energy-monitor.local/api/health | jq '.cpu_usage, .heap_free, .wifi_rssi'
  sleep 60
done
```

### Backup Configuration

```bash
# Backup current configuration
curl -s http://energy-monitor.local/api/config > config-backup-$(date +%Y%m%d).json
```

### Restore Configuration

```bash
# Restore from backup (without reboot)
curl -X POST http://energy-monitor.local/api/config?no_reboot=1 \
  -H "Content-Type: application/json" \
  -d @config-backup-20251213.json
```

### Automated Brightness Adjustment

```bash
# Dim at night, brighten during day
HOUR=$(date +%H)

if [ $HOUR -ge 22 ] || [ $HOUR -lt 7 ]; then
  curl -X POST http://energy-monitor.local/api/brightness -d "25"
else
  curl -X POST http://energy-monitor.local/api/brightness -d "100"
fi
```

### Monitor Multiple Devices

```python
import requests
import json

devices = [
    "http://monitor-1.local",
    "http://monitor-2.local",
    "http://192.168.1.100"
]

for device in devices:
    try:
        info = requests.get(f"{device}/api/info").json()
        health = requests.get(f"{device}/api/health").json()
        
        print(f"{info['hostname']}: v{info['version']}")
        print(f"  CPU: {health['cpu_usage']}%")
        print(f"  Heap: {health['heap_free']/1024:.1f} KB")
        print(f"  RSSI: {health['wifi_rssi']} dBm")
    except Exception as e:
        print(f"Error connecting to {device}: {e}")
```

## Security Considerations

### Current State

**No Authentication:**
- All endpoints publicly accessible
- Suitable for trusted local networks only
- Configuration and control available to any client

### Production Recommendations

1. **Add Authentication:**
   - HTTP Basic Auth for simple protection
   - API keys for automation
   - JWT tokens for web portal

2. **Enable HTTPS:**
   - Self-signed certificates for encryption
   - Protects credentials in transit

3. **Implement Rate Limiting:**
   - Prevent brute force attacks
   - Protect against DoS

4. **Add CSRF Protection:**
   - Verify requests from legitimate sources
   - Token-based validation

5. **Whitelist Networks:**
   - Restrict API access to local network
   - Firewall rules on router

6. **Audit Logging:**
   - Log configuration changes
   - Track API access

## Related Documentation

- [Configuration Guide](../user/configuration.md) - Web portal user guide
- [OTA Updates](../user/ota-updates.md) - Firmware update guide
- [Troubleshooting](../user/troubleshooting.md) - Common API issues
- [Building from Source](building-from-source.md) - Development workflow

---

**Questions or issues?** [Create an issue](https://github.com/jantielens/esp32-energymon-169lcd/issues) on GitHub.
