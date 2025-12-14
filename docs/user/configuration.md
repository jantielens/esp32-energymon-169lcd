# Configuration Guide

Complete guide to the web configuration portal and all available settings.

## Accessing the Web Portal

### First Boot (Captive Portal Mode)

When the device has no WiFi configuration:

1. Device creates WiFi Access Point: `ESP32-XXXXXX`
2. Connect to this network
3. Portal should auto-open at `http://192.168.4.1`
4. If not, manually navigate to `http://192.168.4.1`

### Normal Operation (WiFi Connected)

Access the portal using:

- **mDNS**: `http://energy-monitor.local` (Mac/Linux/iOS/Android)
- **IP Address**: `http://192.168.1.123` (check router or serial monitor)
- **Router DHCP Table**: Find device by name "energy-monitor"

## Portal Pages

The web portal has three main pages:

| Page | URL | Purpose | Available When |
|------|-----|---------|----------------|
| **Home** | `/` or `/home.html` | MQTT topics, power thresholds, brightness | WiFi connected |
| **Network** | `/network.html` | WiFi, device name, MQTT broker | Always |
| **Firmware** | `/firmware.html` | OTA updates, factory reset | WiFi connected |

## Network Page

### WiFi Settings

Configure WiFi connection:

- **WiFi SSID**: Your network name (2.4 GHz only, ESP32 doesn't support 5 GHz)
- **WiFi Password**: Network password
- **Use Static IP**: Enable for fixed IP configuration
  - **IP Address**: e.g., `192.168.1.100`
  - **Gateway**: e.g., `192.168.1.1`
  - **Subnet Mask**: e.g., `255.255.255.0`
  - **Primary DNS**: e.g., `192.168.1.1` or `8.8.8.8`
  - **Secondary DNS**: Optional, e.g., `8.8.4.4`

> **Note:** After saving WiFi settings, device will reboot and attempt to connect. If connection fails, it will restart in Access Point mode after 30 seconds.

### Device Settings

Configure device identity:

- **Device Name**: Friendly name (e.g., "Energy Monitor")
  - Used for mDNS hostname (e.g., "Energy Monitor" → `energy-monitor.local`)
  - Appears in router DHCP table
  - Automatically includes last 4 hex digits of chip ID for uniqueness

### MQTT Broker Settings

Configure MQTT connection:

- **MQTT Broker**: IP address or hostname (e.g., `192.168.1.100` or `homeassistant.local`)
- **MQTT Port**: Default `1883` (or your broker's port)
- **MQTT Username**: Optional, if broker requires authentication
- **MQTT Password**: Optional, if broker requires authentication

> **Note:** MQTT topics are configured on the Home page, not here.

## Home Page

### MQTT Topics

Configure the topics to subscribe to for power data:

- **Solar Power Topic**: e.g., `home/solar/power`
  - Must publish power in **kilowatts (kW)**
  - Default format: direct numeric value (e.g., `2.5`)
  
- **Solar Value Path**: How to extract value from message
  - Enter `.` for direct numeric values (default)
  - Enter field name for JSON (e.g., `value`, `power`, `state`)
  
- **Grid Power Topic**: e.g., `home/grid/power`
  - Must publish power in **kilowatts (kW)**
  - Default format: direct numeric value (e.g., `1.2`)
  
- **Grid Value Path**: How to extract value from message
  - Enter `.` for direct numeric values (default)
  - Enter field name for JSON (e.g., `value` for `{"value": 1.2}`)

**Examples**:
- Direct value: Topic publishes `2.5` → Value Path: `.`
- JSON field: Topic publishes `{"value": 2.5}` → Value Path: `value`
- Custom field: Topic publishes `{"power": 2.5}` → Value Path: `power`

See [mqtt-integration.md](mqtt-integration.md) for Home Assistant examples.

### LCD Brightness Control

Adjust display backlight in real-time:

- **Brightness Slider**: 0-100%
  - 0% = Display off
  - 50% = Power saving
  - 100% = Maximum brightness
- **Instant Update**: Brightness changes immediately (no save required)
- **Not Persisted**: Setting is not saved across reboots
- **Default**: Device always starts at 100% brightness

> **Tip:** Lower brightness to reduce power consumption or use in dark environments.

### Power Color Thresholds

Customize when icons and text change color based on power levels:

#### Grid Power Thresholds

Configure when grid display changes color:

- **Threshold 0**: Transition from Good (green) to OK
  - Default: `0.0 kW` (negative = exporting, positive = importing)
- **Threshold 1**: Transition from OK to Attention
  - Default: `0.5 kW`
- **Threshold 2**: Transition from Attention to Warning
  - Default: `2.5 kW`

**Color Zones:**
- **Good (Green)**: < Threshold 0 (exporting to grid)
- **OK (White)**: Threshold 0 to Threshold 1 (low import)
- **Attention (Orange)**: Threshold 1 to Threshold 2 (medium import)
- **Warning (Red)**: > Threshold 2 (high import)

#### Home Power Thresholds

Configure when home consumption display changes color:

- **Threshold 0**: Good → OK transition (default: `0.5 kW`)
- **Threshold 1**: OK → Attention transition (default: `1.0 kW`)
- **Threshold 2**: Attention → Warning transition (default: `2.0 kW`)

#### Solar Power Thresholds

Configure when solar generation display changes color:

- **Threshold 0**: Good → OK transition (default: `0.5 kW`)
- **Threshold 1**: OK → Attention transition (default: `1.5 kW`)
- **Threshold 2**: Attention → Warning transition (default: `3.0 kW`)

**Note:** Solar uses slightly different color logic:
- **OK (White)**: < 0.5 kW (low/no generation)
- **Good (Green)**: ≥ 0.5 kW (active generation)

#### Customizing Colors

Each of the 4 color zones can be customized:

1. Click the **color picker** next to each zone
2. Select your preferred color
3. Color updates immediately on display
4. Click **Save** to persist changes

**Default Colors:**
- **Good**: `#00FF00` (Green)
- **OK**: `#FFFFFF` (White)
- **Attention**: `#FFA500` (Orange)
- **Warning**: `#FF0000` (Red)

#### Restore Factory Defaults

Click **"Restore Factory Defaults"** button to reset all thresholds and colors to original values.

> **Tip:** Make a note of your custom thresholds before restoring defaults!

See [power-thresholds.md](power-thresholds.md) for examples and tips.

## Firmware Page

### OTA Firmware Updates

Update firmware without USB cable:

1. Download latest `.bin` file from [Releases](https://github.com/jantielens/esp32-energymon-169lcd/releases)
2. Click **"Choose File"** and select the firmware file
3. Click **"Update Firmware"**
4. Wait for upload progress bar to reach 100%
5. Device will automatically reboot with new firmware

> **Important:** Do not power off device during update! Wait for "Update Success" message.

See [ota-updates.md](ota-updates.md) for detailed guide.

### Factory Reset

Reset all settings to defaults:

1. Click **"Factory Reset"** button
2. Confirm the action
3. Device reboots and enters Access Point mode
4. All WiFi, MQTT, and threshold settings are cleared

> **Warning:** This action cannot be undone! Make note of your settings before resetting.

## Real-Time Health Monitoring

Click the orange **Health badge** in the header to expand the overlay showing:

- **Uptime**: How long device has been running
- **Reset Reason**: Why device last restarted
- **CPU Usage**: Current processor utilization
- **Core Temp**: Internal temperature (if available)
- **Free Heap**: Available RAM
- **Min Heap**: Minimum RAM since boot
- **Fragmentation**: Memory fragmentation percentage
- **Flash Usage**: Firmware storage used
- **RSSI**: WiFi signal strength
- **IP Address**: Current network IP

Updates automatically every 5 seconds while expanded.

## REST API

All configuration can be managed via REST API for automation:

```bash
# Get device info
curl http://energy-monitor.local/api/info

# Get current config
curl http://energy-monitor.local/api/config

# Update config (with reboot)
curl -X POST http://energy-monitor.local/api/config \
  -H "Content-Type: application/json" \
  -d '{"wifi_ssid":"MyNetwork","wifi_password":"secret"}'

# Update config (without reboot)
curl -X POST http://energy-monitor.local/api/config?no_reboot=1 \
  -H "Content-Type: application/json" \
  -d '{"device_name":"Living Room Monitor"}'

# Get/Set brightness
curl http://energy-monitor.local/api/brightness
curl -X POST http://energy-monitor.local/api/brightness -d "75"

# Reboot device
curl -X POST http://energy-monitor.local/api/reboot
```

See [../developer/web-portal-api.md](../developer/web-portal-api.md) for complete API reference.

## Tips & Best Practices

### WiFi Configuration

- Use **2.4 GHz networks only** (ESP32 doesn't support 5 GHz)
- Choose **strong WiFi passwords** (WPA2)
- Consider **static IP** for critical monitoring devices
- Place device within **good WiFi range** (RSSI > -70 dBm)

### MQTT Configuration

- Use **local MQTT broker** for lowest latency
- Enable **MQTT authentication** for security
- Choose **descriptive topic names** (e.g., `home/solar/power` not `sensor1`)
- Ensure topics publish at **≥ 1 Hz** for smooth display updates

### Power Thresholds

- Set thresholds based on **your typical usage patterns**
- Grid thresholds: Consider your solar generation capacity
- Home thresholds: Match your household consumption ranges
- Test different values to find what works for you

### Brightness Control

- Lower brightness in **dark environments** to reduce eye strain
- Higher brightness in **bright rooms** for visibility
- Dimming saves ~20-30 mA per 50% reduction

## Troubleshooting

### Can't Access Web Portal

1. Verify device is connected (check serial monitor or router)
2. Try IP address if mDNS isn't working
3. Check firewall settings on your computer
4. Ensure you're on the same network as the device

### WiFi Connection Fails

1. Double-check SSID and password
2. Ensure 2.4 GHz network is available
3. Check router isn't blocking new devices
4. Try factory reset and reconfigure

### MQTT Not Connecting

1. Verify broker IP and port are correct
2. Check username/password if authentication enabled
3. Test broker with `mosquitto_sub` from another device
4. Check firewall isn't blocking port 1883

### Configuration Not Saving

1. Wait for "Saved successfully" message
2. Check serial monitor for error messages
3. Try factory reset if NVS storage is corrupted

## Related Documentation

- [MQTT Integration Guide](mqtt-integration.md) - Home Assistant examples
- [Power Thresholds Guide](power-thresholds.md) - Color configuration details
- [OTA Updates Guide](ota-updates.md) - Firmware update process
- [Troubleshooting Guide](troubleshooting.md) - Common issues

---

**Need help?** Check [troubleshooting.md](troubleshooting.md) or [create an issue](https://github.com/jantielens/esp32-energymon-169lcd/issues).
