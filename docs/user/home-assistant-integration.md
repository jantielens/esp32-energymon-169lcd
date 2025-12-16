# Home Assistant Integration Guide

Display camera snapshots on your ESP32 Energy Monitor from Home Assistant automations.

## Overview

This guide shows how to send camera images from Home Assistant to your ESP32 device using AppDaemon. When a doorbell rings, motion is detected, or any other trigger occurs, the camera snapshot will display on the ESP32's LCD screen (default: 10 seconds, configurable up to 24 hours; 0 = permanent).

**Architecture:**
```
[HA Camera] → [AppDaemon Script] → Resize + baseline JPEG → [ESP32 Display]
```

**Features:**
- Single Python file + Pillow (image processing)
- Automatic aspect ratio preservation (letterboxing)
- Optimized file sizes for ESP32 memory
- Memory-safe processing (keeps uploads within device limits)

---

## Prerequisites

Before starting, ensure you have:

- ✅ ESP32 Energy Monitor running firmware v1.4.0 or later
- ✅ Home Assistant OS (or Supervised) running
- ✅ Camera entities configured in Home Assistant
- ✅ Network connectivity between HA and ESP32

---

## Installation

### Step 1: Install AppDaemon Add-on

1. Open Home Assistant
2. Navigate to **Settings → Add-ons → Add-on Store**
3. Search for **"AppDaemon 4"**
4. Click **Install**
5. Wait for installation to complete

### Step 2: Configure AppDaemon Dependencies

AppDaemon needs Pillow (Python image library) to process images.

1. Navigate to **Settings → Add-ons → AppDaemon 4 → Configuration**
2. Find the **python_packages** section
3. Add the following:

```yaml
python_packages:
  - Pillow
```

4. Click **Save**
5. Go to **Info** tab and click **Restart**
6. Wait for AppDaemon to restart (this installs Pillow)

### Step 3: Install the AppDaemon Script

You need to copy the `camera_to_esp32.py` file to your AppDaemon apps directory.

**Option A: Using File Editor Add-on (Easiest)**

1. Install **File Editor** add-on if not already installed
2. Navigate to `/addon_configs/a0d7b954_appdaemon/apps/`
3. Create the file below

**Option B: Using SSH/Terminal**

```bash
cd /addon_configs/a0d7b954_appdaemon/apps/
# Copy file here
```

**File to create:**

#### `camera_to_esp32.py`

This is a single self-contained script that handles:
- Camera snapshot fetching
- Image resizing with aspect ratio preservation
- Upload to ESP32 via HTTP

**Location:** Included in this repository at:
```
tools/camera_to_esp32.py
```

**Copy it to:**
```
/addon_configs/a0d7b954_appdaemon/apps/camera_to_esp32.py
```

### Step 4: Register the AppDaemon App

Edit the AppDaemon configuration file:

**File:** `/addon_configs/a0d7b954_appdaemon/apps/apps.yaml`

**Add this configuration:**

```yaml
camera_to_esp32:
  module: camera_to_esp32
  class: CameraToESP32
  # Optional: Set default ESP32 IP if you only have one device
  # default_esp32_ip: "192.168.1.111"
```

### Step 5: Restart AppDaemon

1. Go to **Settings → Add-ons → AppDaemon 4**
2. Click **Restart**
3. Check the **Log** tab for any errors
4. You should see: `INFO camera_to_esp32: CameraToESP32 initialized`

---

## Configuration

### Basic Automation Example

Manual test automation (trigger manually from UI or automation editor):

```yaml
alias: "Camera to ESP32 (Manual Test)"
description: "Send camera snapshot to ESP32 display"
mode: single
triggers: []
conditions: []
actions:
  - event: camera_to_esp32
    event_data:
      camera_entity: camera.front_door
      esp32_ip: "192.168.1.111"
```

**To run:** Go to Settings → Automations & Scenes → Find this automation → Click the ▶ (Run) button

### Advanced Examples

#### Multiple Cameras to Different ESP32 Devices

```yaml
automation:
  - alias: "Front Door to Kitchen Display"
    trigger:
      - platform: state
        entity_id: binary_sensor.front_door
        to: "on"
    action:
      - event: camera_to_esp32
        event_data:
          camera_entity: camera.front_door
          esp32_ip: "192.168.1.111"  # Kitchen ESP32

  - alias: "Back Door to Bedroom Display"
    trigger:
      - platform: state
        entity_id: binary_sensor.back_door
        to: "on"
    action:
      - event: camera_to_esp32
        event_data:
          camera_entity: camera.back_door
          esp32_ip: "192.168.1.112"  # Bedroom ESP32
```

#### Motion Detection with Delay

```yaml
automation:
  - alias: "Driveway Motion to ESP32"
    trigger:
      - platform: state
        entity_id: binary_sensor.driveway_motion
        to: "on"
    action:
      # Wait 2 seconds for person to be in frame
      - delay: "00:00:02"
      - event: camera_to_esp32
        event_data:
          camera_entity: camera.driveway
          esp32_ip: "192.168.1.111"
```

#### Scheduled Camera Snapshot

```yaml
automation:
  - alias: "Daily Front Door Snapshot"
    trigger:
      - platform: time
        at: "08:00:00"
    action:
      - event: camera_to_esp32
        event_data:
          camera_entity: camera.front_door
          esp32_ip: "192.168.1.111"
```

#### With Notification

```yaml
automation:
  - alias: "Doorbell with Notification"
    trigger:
      - platform: state
        entity_id: binary_sensor.doorbell
        to: "on"
    action:
      - event: camera_to_esp32
        event_data:
          camera_entity: camera.front_door
          esp32_ip: "192.168.1.111"
      - service: notify.mobile_app
        data:
          title: "Doorbell"
          message: "Someone at the front door"
```

#### Custom Display Timeout

```yaml
automation:
  - alias: "Security Camera - Long Display"
    trigger:
      - platform: state
        entity_id: binary_sensor.security_motion
        to: "on"
    action:
      - event: camera_to_esp32
        event_data:
          camera_entity: camera.security
          esp32_ip: "192.168.1.111"
          timeout: 60  # Display for 60 seconds
```

---

## Event Parameters

The `camera_to_esp32` event accepts the following parameters:

| Parameter | Required | Description | Example |
|-----------|----------|-------------|---------|
| `camera_entity` | Yes | Camera entity ID | `camera.front_door` |
| `esp32_ip` | Yes* | ESP32 IP address | `192.168.1.111` |
| `timeout` | No | Display timeout in seconds (0-86400) | `30` |

\* Not required if `default_esp32_ip` is set in `apps.yaml`

**Timeout Notes:**
- Default: 10 seconds if not specified
- Range: 0 (permanent display), 1-86400 seconds (24 hours maximum)
- Setting `timeout: 0` makes image permanent (no auto-dismiss)
- After timeout, display returns to power screen automatically
- Timer starts when upload completes (full display time, accounting for decode time)

---

## Image Quality Settings

### Recommended Camera Resolutions

For best results, configure your cameras to output:
- **Landscape mode (recommended):** 280×240 pixels or 640×360 pixels
- **Portrait mode:** 240×280 pixels

The script automatically:
- Resizes images to fit 280×240 display
- Maintains aspect ratio (no stretching)
- Adds black letterboxing as needed
- Optimizes quality for ESP32 memory (~20KB final size)

### Camera Configuration Example (Frigate)

```yaml
cameras:
  front_door:
    snapshots:
      enabled: true
      width: 240
      height: 280
      quality: 85
```

### Camera Configuration Example (Generic)

Most cameras allow setting snapshot resolution in their integration settings:

1. Go to **Settings → Devices & Services**
2. Find your camera integration
3. Configure snapshot width/height to match the panel: 240×280

If your camera/integration only supports 280×240, keep that and rotate on the client before upload.

---

## Troubleshooting

### Check AppDaemon Logs

1. Go to **Settings → Add-ons → AppDaemon 4 → Log**
2. Look for entries from `camera_to_esp32`

**Success looks like:**
```
INFO camera_to_esp32: Processing camera snapshot: camera.front_door → 192.168.1.111
INFO camera_to_esp32: Fetched camera snapshot: 45123 bytes
INFO camera_to_esp32: Resizing to 240x280 and encoding as baseline JPEG
INFO camera_to_esp32: Prepared JPEG: 22456 bytes
INFO camera_to_esp32: Upload successful!
```

### Common Issues

#### "Pillow not found" Error

**Problem:** Pillow not installed in AppDaemon environment

**Solution:**
1. Go to AppDaemon configuration
2. Add `Pillow` to `python_packages` list
3. Restart AppDaemon

#### "Failed to fetch camera snapshot" Error

**Problem:** Camera entity doesn't support snapshots or is offline

**Solution:**
1. Test camera in HA: Go to camera entity → More Info → Should see live view
2. Check camera integration is working
3. Verify camera entity ID is correct in automation

#### "Upload failed: Connection refused"

**Problem:** Cannot reach ESP32 device

**Solution:**
1. Verify ESP32 is powered on and connected to WiFi
2. Check IP address is correct: `ping 192.168.1.111`
3. Try accessing web portal: `http://192.168.1.111`
4. Check firewall settings

#### "Upload failed: 507 Insufficient Memory"

**Problem:** Image too large for ESP32 RAM

**Solution:**
1. Reduce camera snapshot resolution
2. Lower JPEG quality in camera settings
3. Recommended max: 100KB file size

#### Image Colors Look Wrong

**Problem:** Image was preprocessed with RGB↔BGR swapping

**Solution:**
1. Ensure you are uploading a normal RGB JPEG (no client-side RGB↔BGR conversion)
2. If you have scripts that swap channels, disable that step
3. ESP32 firmware should be v1.4.0+

#### Image Not Showing on ESP32

**Problem:** Invalid/unsupported JPEG encoding or upload incomplete

**Solution:**
1. Check ESP32 serial logs for errors
2. Ensure the JPEG is baseline (not progressive) and uses standard chroma subsampling
3. Try uploading via the provided tool first: `python3 tools/upload_image.py 192.168.1.111 test.jpg --mode single`

---

## Testing

### Manual Test via Developer Tools

1. Go to **Developer Tools → Events**
2. Event Type: `camera_to_esp32`
3. Event Data:
```yaml
camera_entity: camera.front_door
esp32_ip: "192.168.1.111"
```
4. Click **Fire Event**
5. Check AppDaemon logs for success/errors

### Test with Static Image File

If cameras aren't set up yet, you can test with a file:

1. Upload a test JPEG to `/config/www/test.jpg`
2. Modify `camera_to_esp32.py` to load from file instead of camera
3. Fire the event

---

## Performance Notes

**Typical Processing Times:**
- Camera snapshot fetch: 100-500ms
- Resize + JPEG (re)encode: 50-250ms
- HTTP upload: 100-200ms
- **Total: ~500ms-1s** from trigger to display

**Network Bandwidth:**
- Camera snapshot: 30-50KB (depends on quality/resolution)
- JPEG upload: typically 15-60KB (depends on quality/resolution)

**ESP32 Memory Usage:**
- Automatically freed after timeout expires (default: 10 seconds, configurable)

---

## Advanced Configuration

### Using REST Service Instead of Event

If you prefer calling a service instead of firing an event, modify `camera_to_esp32.py`:

```python
# In initialize():
self.register_service("camera_to_esp32/send", self.handle_service_call)

def handle_service_call(self, namespace, domain, service, kwargs, cb_args):
    camera_entity = kwargs.get("camera_entity")
    esp32_ip = kwargs.get("esp32_ip", self.default_esp32_ip)
    self.process_camera_snapshot(camera_entity, esp32_ip)
```

**Then call from automation:**
```yaml
action:
  - service: camera_to_esp32.send
    data:
      camera_entity: camera.front_door
      esp32_ip: "192.168.1.111"
```

### Multiple ESP32 Devices with Auto-Discovery

Use input_select to choose which ESP32 to display on:

```yaml
input_select:
  esp32_display:
    name: "ESP32 Display Target"
    options:
      - Kitchen (192.168.1.111)
      - Bedroom (192.168.1.112)
      - Living Room (192.168.1.113)

automation:
  - alias: "Show Camera on Selected ESP32"
    trigger:
      - platform: state
        entity_id: binary_sensor.doorbell
        to: "on"
    action:
      - event: camera_to_esp32
        event_data:
          camera_entity: camera.front_door
          esp32_ip: >
            {% set device = states('input_select.esp32_display') %}
            {% if 'Kitchen' in device %}192.168.1.111
            {% elif 'Bedroom' in device %}192.168.1.112
            {% elif 'Living Room' in device %}192.168.1.113
            {% endif %}
```

---

## Security Considerations

### Network Security

- ESP32 API is **not authenticated** by default
- Ensure devices are on a trusted network (e.g., IoT VLAN)
- Consider firewall rules to restrict access

### Privacy

- Camera snapshots are processed locally in Home Assistant
- Images are sent directly to ESP32 (not cloud services)
- ESP32 displays image for configured duration then discards it (default: 10 seconds)
- No persistent storage of images on ESP32

---

## Support

For issues specific to:
- **Home Assistant setup:** Check HA community forums
- **AppDaemon errors:** Check AppDaemon logs and documentation
- **ESP32 firmware:** Check project GitHub issues
- **Image quality:** Adjust camera settings and resolution

---

## See Also

- [Image Display API Documentation](image-display.md)
- [MQTT Integration](mqtt-integration.md)
- [Troubleshooting Guide](troubleshooting.md)
