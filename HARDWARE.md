# Hardware Guide

## Overview

The ESP32 Energy Monitor is a compact device that displays real-time solar and grid power consumption on a vibrant 1.69" color LCD. It connects to your home automation system via MQTT and provides at-a-glance energy monitoring with color-coded visual indicators.

**What You'll Build:**
- Real-time energy monitoring display
- WiFi-connected MQTT client
- Web-based configuration interface
- OTA firmware updates over WiFi

## Bill of Materials

### Required Components

| Component | Specification | Quantity | Estimated Cost | Notes |
|-----------|--------------|----------|----------------|-------|
| **ESP32 Board** | ESP32 DevKit V1 **OR** ESP32-C3 Super Mini | 1 | $3-8 USD | Choose one based on availability. Support for other ESP32 boards can be done by updating the setup.sh script. |
| **LCD Display** | Waveshare 1.69" LCD Module (240×280, ST7789V2) | 1 | $8-12 USD | Includes GH1.25 8-pin connector |

### Optional Components

| Component | Purpose | Estimated Cost |
|-----------|---------|----------------|
| **USB Power Adapter** | 5V 1A minimum for permanent installation | $3-8 USD |

## LCD Module Specifications

- **Display Size**: 1.69 inch IPS LCD
- **Resolution**: 240 × 280 pixels
- **Controller**: ST7789V2
- **Interface**: 4-wire SPI
- **Operating Voltage**: 3.3V (ESP32 compatible)
- **Connector**: GH1.25 8-pin
- **Viewing Angle**: 170° (IPS technology)
- **Backlight**: White LED with PWM brightness control

## Wiring Guide

The project supports two ESP32 variants with different pinouts. Choose the wiring diagram that matches your board.

### ESP32 DevKit V1 Wiring

**8 connections required:**

| LCD Pin | Function | ESP32 GPIO | ESP32 Pin Label | Notes |
|---------|----------|------------|-----------------|-------|
| **VCC** | Power | 3.3V | 3V3 | ⚠️ Do NOT use 5V |
| **GND** | Ground | GND | GND | Common ground |
| **DIN** | SPI MOSI | GPIO 23 | D23 | Data In (VSPI MOSI) |
| **CLK** | SPI Clock | GPIO 18 | D18 | Clock (VSPI SCK) |
| **CS** | Chip Select | GPIO 5 | D5 | Slave select |
| **DC** | Data/Command | GPIO 16 | RX2 | Mode selection |
| **RST** | Reset | GPIO 17 | TX2 | Hardware reset |
| **BL** | Backlight | GPIO 4 | D4 | PWM for brightness control |

**Visual Diagram:**

```
ESP32 DevKit V1                  1.69" LCD Module
┌───────────────┐               ┌──────────────┐
│               │               │              │
│   3V3 (3.3V)  ├──────────────►│ VCC          │
│   GND         ├──────────────►│ GND          │
│   D23 (GPIO23)├──────────────►│ DIN (MOSI)   │
│   D18 (GPIO18)├──────────────►│ CLK (SCK)    │
│   D5  (GPIO5) ├──────────────►│ CS           │
│   RX2 (GPIO16)├──────────────►│ DC           │
│   TX2 (GPIO17)├──────────────►│ RST          │
│   D4  (GPIO4) ├──────────────►│ BL           │
│               │               │              │
└───────────────┘               └──────────────┘
```

SVG diagram: [docs/pinout-esp32-devkitv1-wires.svg](docs/pinout-esp32-devkitv1-wires.svg)

### ESP32-C3 Super Mini Wiring

**8 connections required:**

| LCD Pin | Function | ESP32-C3 GPIO | Notes |
|---------|----------|---------------|-------|
| **VCC** | Power | 3V3 | ⚠️ Do NOT use 5V |
| **GND** | Ground | GND | Common ground |
| **DIN** | SPI MOSI | GPIO 6 | LCD data in |
| **CLK** | SPI Clock | GPIO 4 | LCD clock |
| **CS** | Chip Select | GPIO 7 | SPI SS |
| **DC** | Data/Command | GPIO 3 | Control pin |
| **RST** | Reset | GPIO 20 | Control pin |
| **BL** | Backlight | GPIO 1 | PWM-capable |

**Visual Diagram:**

```
ESP32-C3 Super Mini             1.69" LCD Module
┌───────────────┐               ┌──────────────┐
│ 3V3           ├──────────────►│ VCC          │
│ GND           ├──────────────►│ GND          │
│ GPIO6  (MOSI) ├──────────────►│ DIN          │
│ GPIO4  (SCK)  ├──────────────►│ CLK          │
│ GPIO7  (CS)   ├──────────────►│ CS           │
│ GPIO3  (DC)   ├──────────────►│ DC           │
│ GPIO20 (RST)  ├──────────────►│ RST          │
│ GPIO1  (BL)   ├──────────────►│ BL           │
└───────────────┘               └──────────────┘
```

**Physical Pin Layout (ESP32-C3 Super Mini):**

```
                           USB-C
            .---------------------------------.
            | GPIO5     o            o     5V  |
            | GPIO6*    o            o    GND* |
            | GPIO7*    o            o    3V3* |---> LCD VCC
            | GPIO8     o            o  GPIO4* |---> LCD CLK
            | GPIO9     o            o  GPIO3* |---> LCD DC
            | GPIO10    o            o   GPIO2 |
LCD RST <---| GPIO20*   o            o  GPIO1* |---> LCD BL
            | GPIO21    o            o  GPIO0  |
            '---------------------------------'
```

Legend: `*` = used for LCD connection

SVG diagram: [docs/pinout-esp32c3-supermini-wires.svg](docs/pinout-esp32c3-supermini-wires.svg)

> **Important:** ESP32-C3 boards from different manufacturers may have slightly different pin labels. Always verify against the silkscreen on your specific board.

## Assembly Instructions

### Prototyping Setup (Recommended for Testing)

1. **Connect the LCD to ESP32:**
   - Use 8 female-to-female jumper wires
   - Follow the wiring table for your board variant
   - Keep wires as short as possible (< 20cm) for signal integrity

2. **Color Coding (Optional but Recommended):**
   - Red: VCC (3.3V)
   - Black: GND
   - White/Yellow: DIN (MOSI)
   - Green: CLK (SCK)
   - Blue: CS
   - Orange: DC
   - Purple: RST
   - Gray: BL

3. **Verify Connections:**
   - Double-check VCC is connected to 3.3V, **NOT 5V**
   - Ensure all 8 pins are firmly connected
   - Check for loose connections

### Permanent Installation (Optional)

For a permanent installation, consider:

1. **Soldering:**
   - Solder jumper wires or a cable directly to the ESP32 header pins
   - Solder the GH1.25 connector or wires to a prototype PCB
   - Use heat shrink tubing for insulation

2. **PCB Design:**
   - Design a custom PCB to mount both the ESP32 and LCD
   - Include mounting holes for enclosure
   - Add a USB connector for easy firmware updates

3. **Enclosure:**
   - 3D print or purchase an enclosure
   - Ensure adequate ventilation for the ESP32
   - Design a window or front panel for the LCD

## Power Considerations

### Voltage Requirements

- **ESP32 Input**: 5V via USB or 3.3V via 3V3 pin
- **LCD Operating Voltage**: 3.3V only (do NOT use 5V)
- **ESP32 3.3V Output**: Can supply up to 600mA (sufficient for LCD)

### Current Draw

| Component | Typical Current | Maximum Current |
|-----------|----------------|-----------------|
| ESP32 (WiFi active) | 80-160 mA | 240 mA |
| LCD Display | 30 mA | 90 mA (full brightness) |
| **Total System** | 110-190 mA | 330 mA |

**Power Supply Recommendations:**
- Minimum: 5V @ 500 mA (USB standard)
- Recommended: 5V @ 1A for reliable operation
- Use quality USB power adapters (avoid cheap knock-offs)

### Backlight Control

The LCD backlight brightness can be controlled via PWM on the BL pin:
- 0% brightness: `analogWrite(BL_PIN, 0)` - Display off
- 50% brightness: `analogWrite(BL_PIN, 128)` - Power saving
- 100% brightness: `analogWrite(BL_PIN, 255)` - Maximum visibility

Brightness is also configurable via the web portal and REST API after initial setup.

## GH1.25 Connector Cable

The Waveshare LCD uses a GH1.25 8-pin connector:

**Pin Order (Left to Right, looking at connector):**
1. VCC (often marked with colored wire or triangle)
2. GND
3. DIN (MOSI)
4. CLK (SCK)
5. CS
6. DC
7. RST
8. BL

If your LCD came with a cable:
- Use the included cable if it has labeled or color-coded wires
- Cut the opposite end and strip wires for connection to ESP32
- Use a multimeter to verify pin mapping if unsure

## Testing Before Assembly

Before permanent assembly, test the setup:

1. **Power Test:**
   - Connect only VCC and GND
   - Verify ESP32 powers on (onboard LED should light)
   - Measure voltage at LCD VCC pin (should be 3.3V)

2. **Display Test:**
   - Connect all 8 wires
   - Flash the firmware (see [GETTING-STARTED.md](GETTING-STARTED.md))
   - Display should show boot splash screen
   - If blank, try adjusting backlight pin

3. **WiFi Test:**
   - Configure WiFi via captive portal
   - Verify web portal loads
   - Test MQTT connection

## Troubleshooting

### Display Not Working

**Symptom:** No display, blank screen, or no backlight.

**Solutions:**
1. ✅ Check power connections (VCC to 3.3V, **NOT 5V**)
2. ✅ Verify all 8 connections are secure
3. ✅ Check backlight pin (BL) - try full brightness via web portal
4. ✅ Measure voltage at LCD VCC (should be 3.3V ±0.1V)
5. ✅ Try a different USB cable or power supply

### Garbled or Corrupted Display

**Symptom:** Random pixels, stripes, or incorrect colors.

**Solutions:**
1. ✅ Reduce SPI clock speed (developer modification)
2. ✅ Check for loose connections
3. ✅ Shorten jumper wire lengths (< 15cm recommended)
4. ✅ Verify SPI mode is MODE3 (firmware default)

### ESP32 Won't Boot

**Symptom:** ESP32 doesn't start, stuck in boot loop, or no serial output.

**Solutions:**
1. ✅ Disconnect GPIO 5 (CS) temporarily - ensure it's not pulled LOW
2. ✅ For ESP32-C3: Avoid strapping pins (GPIO 2, 8, 9)
3. ✅ Check for shorts between VCC and GND
4. ✅ Try a different power supply or USB port

### WiFi Connection Issues

**Symptom:** Can't connect to captive portal or WiFi network.

**Solutions:**
1. ✅ Check WiFi antenna is not obstructed (especially in metal enclosures)
2. ✅ Verify WiFi credentials are correct
3. ✅ Ensure 2.4 GHz WiFi network is available (ESP32 doesn't support 5 GHz)
4. ✅ Check router firewall settings

## Board Variant Differences

### ESP32 DevKit V1 vs ESP32-C3 Super Mini

| Feature | ESP32 DevKit V1 | ESP32-C3 Super Mini |
|---------|-----------------|---------------------|
| **Size** | Larger (55×28mm) | Compact (22.5×18mm) |
| **USB** | Micro-USB | USB-C |
| **CPU** | Dual-core 240 MHz | Single-core 160 MHz |
| **RAM** | 520 KB | 400 KB |
| **Flash** | 4 MB (typical) | 4 MB (typical) |
| **WiFi** | 802.11 b/g/n | 802.11 b/g/n |
| **GPIO Pins** | 30+ | 22 |
| **Price** | $5-8 USD | $3-5 USD |
| **Best For** | More GPIOs, dual-core performance | Compact projects, USB-C convenience |

**Both boards are fully supported** with identical features and performance for this project.

## Next Steps

Once your hardware is assembled:

1. **Flash Firmware**: Follow the [GETTING-STARTED.md](GETTING-STARTED.md) guide
2. **Configure WiFi**: Connect to the captive portal on first boot
3. **Set Up MQTT**: Configure your broker and topics
4. **Start Monitoring**: Watch real-time energy data on the display!

## Additional Resources

- [ESP32 DevKit V1 Pinout Reference](https://lastminuteengineers.com/esp32-pinout-reference/)
- [ESP32-C3 Super Mini Guide](https://randomnerdtutorials.com/getting-started-esp32-c3-super-mini/)
- [Waveshare 1.69" LCD Wiki](https://www.waveshare.com/wiki/1.69inch_LCD_Module)
- [ST7789V2 Datasheet](https://files.waveshare.com/upload/c/c9/ST7789V2.pdf)
- [Developer Pinout Documentation](docs/pinout.md) - Detailed technical reference

## Support

Having issues? Check the troubleshooting sections in:
- This hardware guide (above)
- [GETTING-STARTED.md](GETTING-STARTED.md) - Software setup
- [docs/user/troubleshooting.md](docs/user/troubleshooting.md) - Common problems
- [GitHub Issues](https://github.com/jantielens/esp32-energymon-169lcd/issues) - Report bugs or ask questions
