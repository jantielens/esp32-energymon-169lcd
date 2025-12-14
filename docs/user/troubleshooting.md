# Troubleshooting Guide

Common issues and solutions for the ESP32 Energy Monitor.

> **Quick Tip:** Check [GETTING-STARTED.md](../../GETTING-STARTED.md#troubleshooting) for setup-specific issues.

## Display Issues

### Blank/Dark Screen

**Check these in order:**

1. **Power connections**:
   - VCC connected to 3.3V (NOT 5V)
   - GND properly connected
   - Measure voltage: should be 3.2-3.4V

2. **Backlight (BL pin)**:
   - Verify BL wire is connected to correct GPIO
   - Try web portal: Set brightness to 100%
   - Try API: `curl -X POST http://<ip>/api/brightness -d "255"`

3. **All 8 wires connected**:
   - Verify each connection against [HARDWARE.md](../../HARDWARE.md) pinout table
   - Check for loose connections
   - Wiggle wires to test for intermittent connections

4. **Firmware flashed correctly**:
   - Serial monitor shows boot messages
   - Try reflashing via USB

### Garbled/Corrupted Display

**Visual artifacts, wrong colors, stripes, noise:**

1. **SPI signal integrity**:
   - Shorten jumper wires (< 15 cm recommended)
   - Use quality jumper wires
   - Avoid running wires parallel to power cables

2. **Power supply quality**:
   - Use good quality USB power adapter (≥ 1A)
   - Try different USB cable
   - Avoid USB hubs - connect directly to computer/adapter

3. **Hardware defect** (rare):
   - Try different ESP32 board
   - Try different LCD module

### Wrong Colors

**Colors inverted or shifted:**

- Firmware includes RGB→BGR conversion for ST7789V2
- If issue persists, [report as bug](https://github.com/jantielens/esp32-energymon-169lcd/issues)

### Display Shows Only "-- kW"

**No power data displayed:**

See [MQTT Issues](#mqtt-not-connecting) section below.

## WiFi Issues

### Can't Find ESP32 Access Point

**No "ESP32-XXXXXX" network visible:**

1. **Device is already configured**:
   - Check router DHCP table for "energy-monitor"
   - Try accessing via IP or mDNS
   - Factory reset to force AP mode

2. **Wait longer**:
   - AP starts 5-10 seconds after boot
   - Press RESET button and wait

3. **Check region settings**:
   - Ensure 2.4 GHz WiFi is enabled on your router
   - Some regions restrict certain channels

### Can't Connect to WiFi Network

**Captive portal configured, but won't connect:**

1. **SSID/Password**:
   - Double-check WiFi password (case-sensitive!)
   - Ensure network is 2.4 GHz (ESP32 doesn't support 5 GHz)
   - Try different network if available

2. **Router settings**:
   - Disable MAC address filtering temporarily
   - Check router isn't at maximum connected device limit
   - Try assigning static IP

3. **Signal strength**:
   - Move device closer to router
   - Check RSSI in health monitor (should be > -70 dBm)

### Device Keeps Disconnecting

**WiFi connection drops frequently:**

1. **Signal strength**:
   - Relocate device closer to router
   - Reduce obstacles (walls, metal objects)
   - Check for 2.4 GHz interference (microwaves, cordless phones)

2. **Power supply**:
   - WiFi draws significant power
   - Use quality 5V 1A+ power adapter
   - Avoid long/thin USB cables

3. **Router issues**:
   - Some routers aggressively disconnect idle clients
   - Update router firmware
   - Assign static IP to prevent DHCP issues

## MQTT Issues

### MQTT Not Connecting

**Display shows "-- kW", no data:**

1. **Broker configuration**:
   - Verify broker IP/hostname is correct
   - Check port (usually 1883)
   - Test broker is running: `ping <broker-ip>`

2. **Authentication**:
   - If broker requires auth, verify username/password
   - Try connecting without auth first (if possible)

3. **Network connectivity**:
   - Ensure device is on same network as broker
   - Check firewall isn't blocking port 1883
   - Test with mosquitto client from another device:
     ```bash
     mosquitto_sub -h <broker-ip> -t test -v
     ```

4. **Serial monitor debugging**:
   - Connect via USB and monitor at 115200 baud
   - Look for "MQTT connected" or error messages
   - Check topic subscriptions are confirmed

### Wrong Power Values Displayed

**Numbers don't match expected values:**

1. **Unit conversion**:
   - Topics must publish in **kilowatts (kW)**, not watts!
   - Divide watt values by 1000 before publishing
   - Example: 2500 W = 2.5 kW

2. **Topic format**:
   - Solar: Plain float `2.5`
   - Grid: JSON with value field `{"value": 1.2}`
   - Verify with `mosquitto_sub`:
     ```bash
     mosquitto_sub -h <broker-ip> -t home/solar/power -v
     mosquitto_sub -h <broker-ip> -t home/grid/power -v
     ```

3. **Home calculation**:
   - Home = Solar + Grid
   - Negative grid = exporting to grid
   - Verify math: Solar 2.0 + Grid -0.5 = Home 1.5

### MQTT Disconnects Frequently

**Connection drops and reconnects:**

1. **Broker capacity**:
   - Check broker logs for errors
   - Verify broker can handle connection load
   - Try local broker instead of cloud

2. **Keep-alive settings**:
   - Default 5-second reconnect is appropriate
   - Longer delays reduce broker load

3. **Quality of Service**:
   - Device uses QoS 0 for lowest latency
   - Broker configuration may affect reliability

## Web Portal Issues

### Can't Access Web Portal

**Portal won't load:**

1. **Check connection**:
   - Verify device is on network (check router or serial monitor)
   - Try IP address if mDNS (`http://<ip>`) instead of hostname
   - Ping device: `ping energy-monitor.local`

2. **Browser cache**:
   - Clear browser cache
   - Try different browser
   - Try incognito/private mode

3. **Firewall**:
   - Disable firewall temporarily
   - Allow port 80 (HTTP)

### Settings Won't Save

**Configuration changes don't persist:**

1. **Wait for confirmation**:
   - Look for "Saved successfully" message
   - Some fields require "Save & Reboot"

2. **NVS storage**:
   - Storage may be full/corrupted
   - Try factory reset to clear

3. **Serial monitor**:
   - Check for error messages
   - NVS errors indicate storage issues

## Hardware Issues

### ESP32 Won't Boot

**No serial output, display stays dark:**

1. **Power supply**:
   - Verify 5V USB power
   - Try different USB cable
   - Try different power source

2. **Strapping pins** (ESP32-C3):
   - Disconnect GPIO 2, 8, 9 if used
   - These affect boot mode

3. **Factory reset**:
   - Hold BOOT button while pressing RESET
   - Release RESET, then BOOT
   - Reflash firmware via USB

### Overheating

**Device is hot to touch:**

1. **Normal operation**:
   - ESP32 can reach 50-60°C during WiFi use
   - Check Core Temp in health monitor

2. **Excessive heat**:
   - Ensure adequate ventilation
   - Check for short circuits
   - Reduce LCD brightness

3. **Enclosure**:
   - Add ventilation holes
   - Use heat sinks on ESP32 (optional)

## Getting More Help

### Enable Debug Output

Connect via USB and monitor serial at 115200 baud:

```bash
# Linux/Mac
screen /dev/ttyUSB0 115200

# Or use Arduino IDE Serial Monitor (Tools → Serial Monitor)
```

**Look for:**
- Boot messages and version
- WiFi connection status and IP
- MQTT connection status
- Parsed MQTT messages
- Error messages and stack traces

### Reporting Issues

When creating a [GitHub issue](https://github.com/jantielens/esp32-energymon-169lcd/issues), include:

1. **Hardware details**:
   - ESP32 variant (DevKit V1 or C3 Super Mini)
   - LCD model (Waveshare 1.69")
   
2. **Firmware version**:
   - Check web portal header badges
   
3. **Serial monitor output**:
   - Full boot log
   - Error messages
   
4. **Configuration**:
   - WiFi setup (without passwords!)
   - MQTT broker type
   - Topic names (sanitized)
   
5. **What you tried**:
   - Steps to reproduce
   - Troubleshooting steps already attempted

### Community Support

- **GitHub Discussions**: Ask questions, share configs
- **GitHub Issues**: Report bugs, request features
- **Documentation**: Check all docs in this folder

## Related Documentation

- [GETTING-STARTED.md](../../GETTING-STARTED.md) - Initial setup
- [HARDWARE.md](../../HARDWARE.md) - Wiring and assembly
- [Configuration Guide](configuration.md) - Portal settings
- [MQTT Integration](mqtt-integration.md) - Topic setup

---

**Still stuck?** [Create an issue](https://github.com/jantielens/esp32-energymon-169lcd/issues) with details and we'll help!
