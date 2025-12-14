# ESP32 Energy Monitor with 1.69" LCD

A WiFi-connected real-time energy monitoring display for home solar and grid power consumption. Features a vibrant 1.69" color LCD with color-coded visual indicators, web-based configuration, and MQTT integration with Home Assistant.

![ESP32 Energy Monitor](/assets/energymon.png)
*Real-time energy monitoring at a glance*

## ✨ Features

### 🎨 Visual Energy Monitoring
- **1.69" IPS Color LCD** (240×280 pixels) with 60 FPS smooth rendering
- **Color-Coded Power Indicators** - Instant visual feedback:
  - 🟢 **Green**: Low consumption / Exporting to grid
  - ⚪ **White**: Normal usage
  - 🟠 **Orange**: Medium consumption
  - 🔴 **Red**: High consumption
- **Custom Icons** - Professional PNG icons for solar, home, and grid
- **Proportional Bar Charts** - Visual power comparison (0-3kW scale)
- **Rolling Statistics** - 10-minute min/max overlay on power bars
- **Configurable Thresholds** - Customize colors and thresholds via web portal

### 📡 MQTT Integration
- **Home Assistant Ready** - Subscribe to solar and grid power topics
- **Real-Time Updates** - Power values update every second
- **Auto-Reconnect** - 5-second retry with connection backoff
- **Flexible Topics** - Support for plain values and JSON payloads
- **Auto-Calculated Home Consumption** - Computed as grid + solar

### 🌐 Web Configuration Portal
- **Captive Portal** - First-boot WiFi setup (no USB needed)
- **Responsive Design** - Works on mobile and desktop
- **Full REST API** - `/api/*` endpoints for automation
- **Real-Time Brightness Control** - Adjust LCD backlight instantly
- **OTA Firmware Updates** - Update over WiFi without USB cable
- **Network Configuration** - WiFi, device name, static IP settings
- **MQTT Settings** - Broker, topics, credentials
- **Power Threshold Configuration** - Custom colors and thresholds

### 🔧 Developer-Friendly
- **Multi-Board Support** - ESP32 DevKit V1 and ESP32-C3 Super Mini
- **One-Command Build System** - `./build.sh` compiles for all boards
- **Automated Releases** - GitHub Actions CI/CD with firmware binaries
- **LVGL 8.4.0** - Modern GUI framework with anti-aliased text
- **Icon Generation** - Automatic PNG to LVGL C array conversion
- **Modular Architecture** - Clean separation: display, MQTT, web portal, config

## 🚀 Quick Start

### For Users (Flash Pre-Built Firmware)

1. **Get the Hardware** - See [HARDWARE.md](HARDWARE.md) for complete bill of materials and wiring
   - ESP32 DevKit V1 **OR** ESP32-C3 Super Mini ($3-8) (or any ESP32 you might have)
   - Waveshare 1.69" LCD Module ($8-12)

2. **Download Firmware** - Get the latest `.bin` file from [Releases](https://github.com/jantielens/esp32-energymon-169lcd/releases)
   - `esp32-firmware-vX.Y.Z.bin` (ESP32 DevKit V1)
   - `esp32c3-firmware-vX.Y.Z.bin` (ESP32-C3 Super Mini)

3. **Flash Firmware** - Follow [GETTING-STARTED.md](GETTING-STARTED.md) for step-by-step instructions

4. **Configure Device** - On first boot, connect to WiFi AP `ESP32-XXXXXX` at `http://192.168.4.1`

5. **Start Monitoring** - Configure MQTT broker and topics, watch energy flow in real-time!

### For Developers (Build from Source)

```bash
# Clone and setup
git clone https://github.com/jantielens/esp32-energymon-169lcd.git
cd esp32-energymon-169lcd
./setup.sh

# Build for all boards
./build.sh

# Build, upload, and monitor specific board
./bum.sh esp32      # ESP32 DevKit V1
./bum.sh esp32c3    # ESP32-C3 Super Mini
```

See [docs/developer/building-from-source.md](docs/developer/building-from-source.md) for complete developer guide.

## 📚 Documentation

### User Documentation
- **[HARDWARE.md](HARDWARE.md)** - Bill of materials, wiring diagrams, assembly
- **[GETTING-STARTED.md](GETTING-STARTED.md)** - Flashing firmware, first-boot setup
- **[Configuration Guide](docs/user/configuration.md)** - Web portal features and settings
- **[MQTT Integration](docs/user/mqtt-integration.md)** - Home Assistant setup examples
- **[Power Thresholds](docs/user/power-thresholds.md)** - Customizing color indicators
- **[OTA Updates](docs/user/ota-updates.md)** - Updating firmware over WiFi
- **[Troubleshooting](docs/user/troubleshooting.md)** - Common problems and solutions

### Developer Documentation
- **[Building from Source](docs/developer/building-from-source.md)** - Setup, build system, scripts
- **[Multi-Board Support](docs/developer/multi-board-support.md)** - Adding new board variants
- **[LVGL Display System](docs/developer/lvgl-display-system.md)** - Display architecture, BGR fix
- **[Web Portal API](docs/developer/web-portal-api.md)** - REST API reference
- **[Adding Features](docs/developer/adding-features.md)** - Code structure, contributing
- **[Icon System](docs/developer/icon-system.md)** - PNG to LVGL conversion
- **[Library Management](docs/developer/library-management.md)** - Arduino dependencies
- **[Release Process](docs/developer/release-process.md)** - Creating releases, versioning

### Environment Setup
- **[WSL Development](docs/setup/wsl-development.md)** - USB passthrough for Windows/WSL users
- **[Linux Setup](docs/setup/linux-setup.md)** - Permissions and udev rules
- **[CI/CD](docs/setup/ci-cd.md)** - GitHub Actions workflow

## 🔌 REST API

The device exposes a full REST API for automation and integration:

```bash
# Get device information
curl http://<device-ip>/api/info

# Get real-time health stats
curl http://<device-ip>/api/health

# Get current configuration
curl http://<device-ip>/api/config

# Update configuration
curl -X POST http://<device-ip>/api/config \
  -H "Content-Type: application/json" \
  -d '{"wifi_ssid":"MyNetwork","wifi_password":"secret"}'

# Get/Set LCD brightness (0-100%)
curl http://<device-ip>/api/brightness
curl -X POST http://<device-ip>/api/brightness -d "75"

# Reboot device
curl -X POST http://<device-ip>/api/reboot
```

See [docs/developer/web-portal-api.md](docs/developer/web-portal-api.md) for complete API reference.

## 🧪 Development Workflow

```bash
# One-time setup
./setup.sh                # Install arduino-cli, ESP32 core, libraries

# Development cycle
./build.sh esp32c3        # Build specific board
./upload.sh esp32c3       # Upload firmware
./monitor.sh              # View serial output

# Convenience scripts
./bum.sh esp32c3          # Build + Upload + Monitor (full cycle)
./um.sh esp32c3           # Upload + Monitor
./clean.sh                # Remove build artifacts

# Library management
./library.sh search mqtt  # Find libraries
./library.sh add PubSubClient  # Add library
./library.sh list         # Show configured libraries
```

## 📦 Dependencies

### Arduino Libraries
- **LVGL** (8.4.0) - GUI framework
- **TFT_eSPI** (2.5.43) - Display driver
- **WiFi** (built-in) - ESP32 WiFi
- **WebServer** (built-in) - HTTP server
- **DNSServer** (built-in) - Captive portal
- **Preferences** (built-in) - NVS storage
- **PubSubClient** (2.8) - MQTT client
- **ArduinoJson** (7.2.1) - JSON parsing

See [arduino-libraries.txt](arduino-libraries.txt) for complete list.

### Python Tools
- **Python 3** with **Pillow** - PNG to LVGL icon conversion (auto-installed by `./setup.sh`)

## 🏗️ Project Structure

```
esp32-energymon-169lcd/
├── HARDWARE.md              # Hardware guide (BOM, wiring, assembly)
├── GETTING-STARTED.md       # User setup guide
├── README.md                # This file
├── CHANGELOG.md             # Version history
├── LICENSE                  # MIT License
├── src/
│   ├── app/
│   │   ├── app.ino          # Main Arduino sketch
│   │   ├── screen_power.cpp/h     # Power monitoring screen
│   │   ├── screen_splash.cpp/h    # Boot splash screen
│   │   ├── display_manager.cpp/h  # LVGL display driver
│   │   ├── mqtt_manager.cpp/h     # MQTT client
│   │   ├── config_manager.cpp/h   # NVS configuration
│   │   ├── web_portal.cpp/h       # Web server + REST API
│   │   └── web/               # HTML/CSS/JS assets
│   ├── boards/
│   │   ├── esp32/             # ESP32 DevKit V1 pin config
│   │   └── esp32c3/           # ESP32-C3 Super Mini pin config
│   └── version.h              # Firmware version
├── assets/
│   └── icons/                 # PNG icons (auto-converted to C)
├── docs/
│   ├── user/                  # User documentation
│   ├── developer/             # Developer documentation
│   └── setup/                 # Environment setup guides
├── build/                     # Compiled firmware binaries
├── tools/                     # Build scripts (icon conversion, etc.)
├── *.sh                       # Build/upload/monitor scripts
└── .github/
    └── workflows/
        ├── build.yml          # CI builds
        └── release.yml        # Release automation
```

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow existing code style and architecture
4. Test on both ESP32 and ESP32-C3 if possible
5. Update documentation as needed
6. Commit changes (`git commit -m 'Add amazing feature'`)
7. Push to branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

See [docs/developer/adding-features.md](docs/developer/adding-features.md) for code structure and guidelines.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **LVGL** - Excellent embedded GUI framework
- **TFT_eSPI** - Versatile display driver library
- **Waveshare** - Quality LCD modules with good documentation
- **ESP32 Community** - Arduino core and examples
- **Home Assistant** - MQTT integration inspiration

---

**Made with ❤️ for home energy monitoring enthusiasts**

⭐ Star this repo if you find it useful!
