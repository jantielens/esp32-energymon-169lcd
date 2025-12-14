# Getting Started Guide

This guide will walk you through setting up your ESP32 Energy Monitor from hardware assembly to monitoring your first energy readings.

## Prerequisites

Before you begin, make sure you have:

- ✅ **Hardware assembled** - See [HARDWARE.md](HARDWARE.md) for wiring guide
- ✅ **USB cable** - Micro-USB (ESP32) or USB-C (ESP32-C3)
- ✅ **WiFi network** - 2.4 GHz (ESP32 doesn't support 5 GHz)
- ✅ **MQTT broker** - Home Assistant, Mosquitto, or any MQTT broker
- ✅ **Computer** - Windows, Mac, or Linux for initial flashing

## Step 1: Download Firmware

### Option A: Pre-Built Firmware (Recommended for Users)

1. Go to the [Releases page](https://github.com/jantielens/esp32-energymon-169lcd/releases)
2. Download the latest firmware for your board:
   - `esp32-firmware-vX.Y.Z.bin` for ESP32 DevKit V1
   - `esp32c3-firmware-vX.Y.Z.bin` for ESP32-C3 Super Mini
3. Save the file to your computer

### Option B: Build from Source (For Developers)

```bash
git clone https://github.com/jantielens/esp32-energymon-169lcd.git
cd esp32-energymon-169lcd
./setup.sh      # Install dependencies
./build.sh      # Build for all boards
```

See [docs/developer/building-from-source.md](docs/developer/building-from-source.md) for details.

## Step 2: Flash Firmware

### Using ESP Tool (Cross-Platform)

#### Install ESP Tool

```bash
# Python 3 required
pip install esptool
```

#### Flash the Firmware

**For ESP32 DevKit V1:**
```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
  write_flash -z 0x10000 esp32-firmware-vX.Y.Z.bin
```

**For ESP32-C3 Super Mini:**
```bash
esptool.py --chip esp32c3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash -z 0x0 esp32c3-firmware-vX.Y.Z.bin
```

> **Note:** Replace `/dev/ttyUSB0` with your actual port:
> - Linux/Mac: `/dev/ttyUSB0`, `/dev/ttyACM0`, `/dev/cu.usbserial-*`
> - Windows: `COM3`, `COM4`, etc.

#### Erase Flash (If Needed)

If you're having issues, try erasing the flash first:

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 erase_flash
```

### Using Arduino IDE

1. Open Arduino IDE
2. Install ESP32 board support:
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Tools → Board → ESP32 Arduino → Select your board
4. Tools → Port → Select your USB port
5. Sketch → Upload (or Ctrl+U)

### Using Web Flasher (Chrome/Edge Only)

1. Visit [ESP Web Tools](https://web.esphome.io/)
2. Connect your ESP32 via USB
3. Click "Connect" and select your device
4. Click "Install" and select the firmware file
5. Wait for flashing to complete

### Using Platform IO

```bash
pio run --target upload --environment esp32   # ESP32 DevKit V1
pio run --target upload --environment esp32c3 # ESP32-C3 Super Mini
```

## Step 3: First Boot - Verify Display

After flashing:

1. **Disconnect and reconnect USB** (or press RESET button)
2. **Watch the serial monitor** (optional):
   ```bash
   # Linux/Mac
   screen /dev/ttyUSB0 115200
   
   # Or use Arduino IDE Serial Monitor
   ```
3. **Check the LCD display**:
   - Should show **boot splash screen** with progress bar
   - Progress updates: "Initializing...", "Starting WiFi...", "Ready"
   - After ~5 seconds, transitions to **power screen**

**If display is blank:**
- Check backlight connection (BL pin)
- Verify all 8 wires are connected correctly
- See [HARDWARE.md](HARDWARE.md) troubleshooting section

## Step 4: WiFi Configuration (Captive Portal)

When the device boots for the first time (or WiFi credentials are invalid):

### Connect to Access Point

1. **Look for WiFi network**: `ESP32-XXXXXX` (where XXXXXX is chip ID)
2. **Connect** to this network from your phone or laptop
3. **Captive portal should auto-open** at `http://192.168.4.1`
   - If it doesn't, manually navigate to `http://192.168.4.1`

### Configure Network Settings

In the captive portal:

1. **WiFi Settings:**
   - **WiFi SSID**: Your home network name
   - **WiFi Password**: Your network password
   - **Use Static IP**: Leave unchecked (DHCP) or configure static IP

2. **Device Settings:**
   - **Device Name**: `energy-monitor` (or your choice)
     - Used for mDNS: `http://energy-monitor.local`
     - Used for hostname in router DHCP table

3. **Click "Save & Reboot"**

### Verify Connection

After reboot:

1. **Check serial monitor** for IP address:
   ```
   WiFi connected
   IP address: 192.168.1.123
   mDNS responder started: energy-monitor.local
   ```

2. **Find device on network:**
   - Router DHCP table: Look for "energy-monitor"
   - mDNS: `http://energy-monitor.local` (Mac/Linux/iOS/Android)
   - IP address: `http://192.168.1.123` (from serial monitor)

## Step 5: MQTT Configuration

### Access Web Portal

Open web browser and navigate to:
- `http://energy-monitor.local` (mDNS)
- `http://192.168.1.123` (IP address)

### Configure MQTT Broker

1. **Click "Network" tab** in navigation
2. **Scroll to MQTT Settings:**
   - **MQTT Broker**: `192.168.1.100` (your broker IP or hostname)
   - **MQTT Port**: `1883` (default, or your broker's port)
   - **MQTT Username**: `your-username` (if authentication required)
   - **MQTT Password**: `your-password` (if authentication required)

3. **Configure Topics:**
   - **Solar Power Topic**: `home/solar/power`
     - Must publish power in **kilowatts (kW)** as plain float
     - Example: `2.5` = 2.5 kW
   
   - **Grid Power Topic**: `home/grid/power`
     - Must publish JSON with `value` field in **kilowatts (kW)**
     - Example: `{"value": 1.2}` = 1.2 kW

4. **Click "Save & Reboot"**

### MQTT Topic Examples

#### Home Assistant Integration

If using Home Assistant with solar sensors:

**Solar Topic** (`home/solar/power`):
```yaml
# configuration.yaml
sensor:
  - platform: mqtt
    name: "Solar Power"
    state_topic: "home/solar/power"
    unit_of_measurement: "kW"
    value_template: "{{ value_json.power | float }}"
```

**Grid Topic** (`home/grid/power`):
```yaml
sensor:
  - platform: mqtt
    name: "Grid Power"
    state_topic: "home/grid/power"
    unit_of_measurement: "kW"
    value_template: "{{ value_json.value | float }}"
```

See [docs/user/mqtt-integration.md](docs/user/mqtt-integration.md) for complete Home Assistant setup guide.

#### Testing with Mosquitto

Publish test values to verify configuration:

```bash
# Install mosquitto clients
sudo apt install mosquitto-clients

# Publish solar power (2.5 kW)
mosquitto_pub -h 192.168.1.100 -t home/solar/power -m "2.5"

# Publish grid power (1.2 kW)
mosquitto_pub -h 192.168.1.100 -t home/grid/power -m '{"value": 1.2}'
```

**Expected Result:** Display should update within 1-2 seconds showing:
- Solar: 2.5 kW (green if > 0.5 kW)
- Grid: 1.2 kW (white if < 0.5 kW)
- Home: 3.7 kW (calculated as solar + grid)

## Step 6: Customize Power Thresholds (Optional)

The display uses color-coded indicators to show power status. You can customize these thresholds.

### Default Thresholds

| Power Type | Good (Green) | OK (White) | Attention (Orange) | Warning (Red) |
|------------|--------------|------------|-------------------|---------------|
| **Grid** | < 0.0 kW (export) | 0.0-0.5 kW | 0.5-2.5 kW | > 2.5 kW |
| **Home** | < 0.5 kW | 0.5-1.0 kW | 1.0-2.0 kW | > 2.0 kW |
| **Solar** | < 0.5 kW (white) | 0.5-1.5 kW | 1.5-3.0 kW | > 3.0 kW |

### Customize Thresholds

1. **Open web portal** → **Home** tab
2. **Scroll to "Power Color Thresholds" section**
3. **Adjust thresholds** for each power type:
   - Drag sliders or type values
   - Click color pickers to change colors
4. **Click "Save"** (no reboot required - updates immediately!)

See [docs/user/power-thresholds.md](docs/user/power-thresholds.md) for detailed configuration guide.

## Step 7: Verify Energy Monitoring

Your energy monitor should now be displaying real-time data:

### Display Layout

```
┌────────────────────────────────────────┐
│         SOLAR    HOME    GRID          │
│          ☀️      🏠      ⚡           │
│         2.5kW → 3.7kW ← 1.2kW         │
│          ▓▓      ▓▓▓      ▓            │ (bar charts)
└────────────────────────────────────────┘
```

### Visual Indicators

- **Icons & Text**: Color changes based on power thresholds
- **Arrows**: Show power flow direction
  - Solar → Home (always)
  - Grid → Home (importing) or Home → Grid (exporting)
- **Bar Charts**: Proportional to power (0-3 kW scale)
- **Min/Max Lines**: White overlay lines show 10-minute statistics

### Verify Calculations

**Home Power = Grid Power + Solar Power**

Examples:
- Solar 2.0 kW + Grid 1.0 kW = **Home 3.0 kW** ✅
- Solar 3.0 kW + Grid -0.5 kW = **Home 2.5 kW** ✅ (exporting 0.5 kW)
- Solar 0.0 kW + Grid 1.5 kW = **Home 1.5 kW** ✅ (night time)

## Step 8: Adjust LCD Brightness (Optional)

### Via Web Portal

1. **Open web portal** → **Home** tab
2. **Find "LCD Brightness" slider**
3. **Drag slider** (0-100%)
4. **Brightness updates immediately** (no save needed)

### Via REST API

```bash
# Get current brightness
curl http://energy-monitor.local/api/brightness

# Set brightness to 75%
curl -X POST http://energy-monitor.local/api/brightness -d "75"

# Set brightness to 0% (display off)
curl -X POST http://energy-monitor.local/api/brightness -d "0"
```

> **Note:** Brightness setting is **not persisted** across reboots. Device always starts at 100% brightness.

## Troubleshooting

### Display Issues

**Blank screen:**
- Check BL (backlight) pin connection
- Verify 3.3V power supply
- Try setting brightness to 100% via API

**Garbled display:**
- Reduce SPI clock speed (developer modification needed)
- Check for loose wires
- Shorten jumper wire lengths (< 15 cm)

**Wrong colors:**
- Firmware uses RGB→BGR conversion for ST7789V2
- If colors are inverted, report as bug

### WiFi Issues

**Can't find ESP32 access point:**
- Press RESET button on ESP32
- Wait 5-10 seconds for AP to start
- Check device isn't already connected to a network

**Can't connect to WiFi network:**
- Verify 2.4 GHz network (ESP32 doesn't support 5 GHz)
- Check WiFi password is correct
- Ensure router allows new devices (no MAC filtering)

**Lost IP address:**
- Check router DHCP table
- Use mDNS: `http://energy-monitor.local`
- Connect to serial monitor to see IP

### MQTT Issues

**Display shows "-- kW":**
- MQTT not connected or no data received yet
- Check broker IP and credentials
- Verify topics are correct
- Test topics with `mosquitto_sub`:
  ```bash
  mosquitto_sub -h 192.168.1.100 -t home/solar/power -v
  mosquitto_sub -h 192.168.1.100 -t home/grid/power -v
  ```

**Home calculation is wrong:**
- Verify solar and grid values are in **kilowatts (kW)**, not watts
- Check MQTT message format matches expected format
- Enable serial monitor to see parsed values

**MQTT disconnects frequently:**
- Check WiFi signal strength (move router closer)
- Increase reconnect interval (developer modification)
- Verify broker isn't rate limiting

### Serial Monitor Debug

Connect to serial monitor at 115200 baud to see debug output:

```bash
# Linux/Mac
screen /dev/ttyUSB0 115200

# Or use Arduino IDE Serial Monitor
```

**Look for:**
- WiFi connection status
- IP address
- MQTT connection status
- Received MQTT messages with parsed values
- Error messages

## Next Steps

### Explore Features

- **[Configuration Guide](docs/user/configuration.md)** - All web portal settings explained
- **[Power Thresholds](docs/user/power-thresholds.md)** - Customize color indicators
- **[MQTT Integration](docs/user/mqtt-integration.md)** - Home Assistant examples
- **[OTA Updates](docs/user/ota-updates.md)** - Update firmware over WiFi

### Advanced Usage

- **REST API** - Automate with scripts ([docs/developer/web-portal-api.md](docs/developer/web-portal-api.md))
- **Custom Icons** - Add your own PNG icons ([docs/developer/icon-system.md](docs/developer/icon-system.md))
- **Code Modifications** - Extend functionality ([docs/developer/adding-features.md](docs/developer/adding-features.md))

### Community

- **Report Issues** - [GitHub Issues](https://github.com/jantielens/esp32-energymon-169lcd/issues)
- **Ask Questions** - [GitHub Discussions](https://github.com/jantielens/esp32-energymon-169lcd/discussions)
- **Share Your Build** - Post photos and modifications!

## Support

If you encounter issues not covered here:

1. Check [docs/user/troubleshooting.md](docs/user/troubleshooting.md)
2. Search [existing issues](https://github.com/jantielens/esp32-energymon-169lcd/issues)
3. Create a new issue with:
   - Hardware details (ESP32 variant, LCD model)
   - Firmware version
   - Serial monitor output
   - Description of problem

---

**Congratulations! Your ESP32 Energy Monitor is now operational.** 🎉

Enjoy real-time energy monitoring! ⚡☀️
