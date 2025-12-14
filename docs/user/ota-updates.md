# OTA Updates Guide

Guide to updating ESP32 Energy Monitor firmware over WiFi without USB cable.

## Requirements

- Device connected to WiFi network
- Access to web portal
- Latest firmware `.bin` file from [Releases](https://github.com/jantielens/esp32-energymon-169lcd/releases)

## Downloading Firmware

1. Visit [Releases page](https://github.com/jantielens/esp32-energymon-169lcd/releases)
2. Find the latest version
3. Download the correct `.bin` file for your board:
   - `esp32-firmware-vX.Y.Z.bin` for ESP32 DevKit V1
   - `esp32c3-firmware-vX.Y.Z.bin` for ESP32-C3 Super Mini
4. Save to your computer

> **Important:** Download the correct file for your board type! Wrong firmware will fail to boot.

## Update via Web Portal

### Step-by-Step Process

1. **Navigate to Firmware page**:
   - Open web portal: `http://energy-monitor.local`
   - Click **"Firmware"** tab in navigation

2. **Select firmware file**:
   - Click **"Choose File"** button
   - Select the downloaded `.bin` file
   - Verify filename matches your board type

3. **Upload firmware**:
   - Click **"Update Firmware"** button
   - **Do NOT close browser or refresh page**
   - **Do NOT power off device**
   
4. **Wait for upload**:
   - Progress bar shows upload status (0-100%)
   - Upload typically takes 15-30 seconds
   - Larger firmware takes longer

5. **Automatic reboot**:
   - "Update Success!" message appears
   - Device automatically reboots
   - Wait ~10 seconds for full restart

6. **Verify update**:
   - Display should show boot splash screen
   - Web portal should load after reboot
   - Check firmware version in header badges

### Expected Timeline

| Step | Duration | What's Happening |
|------|----------|------------------|
| Upload | 15-30 sec | Transferring firmware to ESP32 flash |
| Verification | 2-3 sec | ESP32 validates firmware checksum |
| Reboot | 5-10 sec | Device restarts with new firmware |
| **Total** | **~30-45 sec** | Complete update process |

## Update via Command Line

For automation or remote updates:

```bash
# Using curl
curl -X POST http://energy-monitor.local/api/update \
  -F "update=@esp32c3-firmware-v1.3.0.bin"

# Or using wget
wget --post-file=esp32c3-firmware-v1.3.0.bin \
  http://energy-monitor.local/api/update
```

## Troubleshooting

### Upload Fails / Times Out

**Symptoms:**
- Progress bar stalls at X%
- "Upload failed" error message
- Browser shows connection timeout

**Solutions:**
1. ✅ Check WiFi connection strength (move closer to router)
2. ✅ Ensure device isn't rebooting during upload
3. ✅ Try again - transient network issues are common
4. ✅ Use wired connection for computer (more stable)
5. ✅ Disable VPN or proxy that might interfere

### Wrong Firmware File

**Symptoms:**
- Device won't boot after update
- Stuck on boot splash screen
- Serial monitor shows boot loop

**Solutions:**
1. ✅ Download correct firmware for your board type
2. ✅ Reflash via USB with correct firmware
3. ✅ Check release notes for breaking changes

### Device Won't Boot After Update

**Symptoms:**
- Display stays dark
- No WiFi access point appears
- Serial monitor shows errors

**Recovery Steps:**

1. **Try power cycle**:
   - Disconnect power (USB)
   - Wait 10 seconds
   - Reconnect power

2. **Flash via USB** (see [GETTING-STARTED.md](../../GETTING-STARTED.md)):
   ```bash
   esptool.py --chip esp32c3 --port /dev/ttyUSB0 \
     write_flash -z 0x0 esp32c3-firmware-vX.Y.Z.bin
   ```

3. **Erase and reflash**:
   ```bash
   esptool.py --chip esp32c3 --port /dev/ttyUSB0 erase_flash
   esptool.py --chip esp32c3 --port /dev/ttyUSB0 \
     write_flash -z 0x0 esp32c3-firmware-vX.Y.Z.bin
   ```

### Configuration Lost After Update

**Note:** Configuration should persist across firmware updates (stored in NVS).

If configuration is lost:
1. This is unexpected - please [report as bug](https://github.com/jantielens/esp32-energymon-169lcd/issues)
2. Reconfigure via captive portal
3. Check for breaking changes in release notes

## Best Practices

### Before Updating

- ✅ **Note current configuration** (WiFi, MQTT, thresholds)
- ✅ **Read release notes** for breaking changes
- ✅ **Ensure stable WiFi** connection
- ✅ **Backup configuration** if possible (manual note or API export)

### During Update

- ❌ **Do NOT close browser**
- ❌ **Do NOT refresh page**
- ❌ **Do NOT power off device**
- ❌ **Do NOT press reset button**
- ✅ **Wait for completion message**

### After Update

- ✅ **Verify firmware version** in web portal header
- ✅ **Test all functions** (display, MQTT, web portal)
- ✅ **Check configuration** is preserved
- ✅ **Monitor serial output** for errors

## Rollback to Previous Version

If new firmware has issues:

1. **Download previous version** from [Releases](https://github.com/jantielens/esp32-energymon-169lcd/releases)
2. **Flash via USB** (OTA may not work if firmware is unstable)
3. **Report issue** on GitHub

## Update Frequency

Recommended update schedule:

- **Security fixes**: Update immediately
- **Bug fixes**: Update when convenient
- **New features**: Review release notes, update if desired
- **Breaking changes**: Plan configuration migration

**Check for updates:**
- Watch GitHub repository (click "Watch" → "Releases only")
- Subscribe to release notifications
- Check manually every few months

## Verifying Firmware Authenticity

For security-conscious users:

1. **Check SHA256 checksum**:
   ```bash
   sha256sum esp32c3-firmware-v1.3.0.bin
   ```
   Compare with `SHA256SUMS.txt` from release

2. **Verify source**: Only download from official GitHub releases

3. **Build from source**: For maximum trust, compile firmware yourself

## Related Documentation

- [GETTING-STARTED.md](../../GETTING-STARTED.md) - Initial USB flashing
- [Configuration Guide](configuration.md) - Web portal settings
- [Troubleshooting](troubleshooting.md) - Common issues
- [Developer: Release Process](../developer/release-process.md) - How releases are created

---

**Updating regularly ensures you have the latest features and security fixes!** ✅
