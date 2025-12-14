# Custom Partition Tables

## Overview

This project uses a custom partition table to maximize available app space while maintaining OTA (Over-The-Air) update capability on ESP32-C3 boards with 4MB flash.

## The Problem

ESP32-C3 Super Mini boards come with pre-defined partition schemes that don't optimally balance OTA support with app size:

| Scheme | App Size | OTA Support | Issue |
|--------|----------|-------------|-------|
| `default` | 1.2MB × 2 | ✅ Yes | Too small for our 1.4MB firmware |
| `minimal` | 1.3MB × 1 | ❌ No | Slightly larger, but no OTA |
| `no_ota` | 2MB × 1 | ❌ No | Large enough, but no OTA |
| `huge_app` | 3MB × 1 | ❌ No | Maximum space, but no OTA |

**Key constraint**: OTA requires two app partitions (app0 + app1) to safely alternate firmware updates. If an update fails, the device can roll back to the previous partition.

## The Solution

Create a custom partition table that:
- Maximizes app partition size (~1.96MB each)
- Maintains dual partitions for OTA safety
- Minimizes unused space (SPIFFS, coredump)

### Custom Partition Layout

File: `partitions_ota_1_9mb.csv`

```csv
# Custom partition table for 4MB flash with OTA support
# App partitions: ~1.87MB each (supports up to 1.92MB firmware)
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xE000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x1E0000,
app1,     app,  ota_1,   0x1F0000, 0x1E0000,
coredump, data, coredump,0x3D0000, 0x10000,
spiffs,   data, spiffs,  0x3E0000, 0x20000,
```

**Partition breakdown:**
- **nvs** (20KB): Non-volatile storage for WiFi, config, etc.
- **otadata** (8KB): OTA metadata for partition switching
- **app0** (1,966,080 bytes = 1.96MB): Primary app partition
- **app1** (1,966,080 bytes = 1.96MB): Secondary app partition (OTA target)
- **coredump** (64KB): Crash dump storage (optional, can be removed)
- **spiffs** (128KB): File system (minimal, rarely used in this project)

**Total**: Exactly 4MB (0x400000)

### Key Rules for Custom Partitions

1. **Alignment**: All partition offsets must be aligned to 64KB (0x10000) boundaries
   - Exception: `nvs` and `otadata` have fixed offsets (0x9000, 0xE000)
   
2. **Fixed partitions**: These must come first and use standard offsets:
   - `nvs`: 0x9000
   - `otadata`: 0xE000 (required for OTA)
   
3. **App partitions**: Must be type `app` with subtypes `ota_0` and `ota_1`

4. **Total size**: Must not exceed flash size (4MB = 0x400000)

## Implementation in This Project

### 1. Create the partition file

Place `partitions_ota_1_9mb.csv` in the project root.

### 2. Install to Arduino ESP32 package

The partition file must be copied to the ESP32 Arduino package directory and registered in `boards.txt`:

```bash
# Copy partition file
cp partitions_ota_1_9mb.csv ~/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/

# Register partition scheme in boards.txt
ESP32_DIR=$(find ~/.arduino15/packages/esp32/hardware/esp32 -maxdepth 1 -type d | tail -1)
cat >> "$ESP32_DIR/boards.txt" << 'EOF'

# Custom OTA partition scheme
nologo_esp32c3_super_mini.menu.PartitionScheme.ota_1_9mb=Custom OTA (1.9MB APP×2)
nologo_esp32c3_super_mini.menu.PartitionScheme.ota_1_9mb.build.partitions=partitions_ota_1_9mb
nologo_esp32c3_super_mini.menu.PartitionScheme.ota_1_9mb.upload.maximum_size=1966080
EOF
```

> **Note**: Both steps are required because:
> 1. The partition file defines the memory layout
> 2. `boards.txt` registration makes it available as a build option in the FQBN

> **Important**: This registration is **automatically handled** by the GitHub Actions workflow for CI/CD builds. Manual installation is only needed for local development environments.

### 3. Configure the board FQBN

In `config.sh`, add the custom partition scheme to the board's FQBN:

```bash
declare -A FQBN_TARGETS=(
    ["esp32:esp32:nologo_esp32c3_super_mini:PartitionScheme=ota_1_9mb,CDCOnBoot=default"]="esp32c3"
)
```

The key part is `PartitionScheme=ota_1_9mb` which matches the partition filename (without `.csv`).

### 4. Build and flash

```bash
./build.sh esp32c3
./upload.sh esp32c3  # Serial flash required for first partition table change
```

After the first flash, OTA updates will work normally.

## Adapting to Other Projects

### Step 1: Calculate your partition sizes

1. **Determine your firmware size**: Build your project and note the binary size
2. **Add headroom**: Add ~30% for future growth
3. **Calculate app partition size**: 
   - Total available = Flash size - nvs - otadata - coredump - spiffs
   - App partition size = Total available ÷ 2 (for OTA)
   - Round down to nearest 64KB boundary

Example for 4MB flash:
```
4MB (0x400000) total
- 0x9000 (before nvs)
- 0x5000 (nvs)
- 0x2000 (otadata)
- 0x10000 (coredump, optional)
- 0x20000 (spiffs, adjust as needed)
= 0x3C0000 available for apps
÷ 2 = 0x1E0000 per app (1,966,080 bytes)
```

### Step 2: Create your partition CSV

Use this template:

```csv
# Custom partition table for [YOUR FLASH SIZE] with OTA
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xE000,   0x2000,
app0,     app,  ota_0,   0x10000,  [APP_SIZE],
app1,     app,  ota_1,   [APP1_OFFSET], [APP_SIZE],
coredump, data, coredump,[COREDUMP_OFFSET], 0x10000,
spiffs,   data, spiffs,  [SPIFFS_OFFSET], [SPIFFS_SIZE],
```

Replace the bracketed values based on your calculations. Remember:
- `[APP1_OFFSET]` = 0x10000 + `[APP_SIZE]`
- `[COREDUMP_OFFSET]` = `[APP1_OFFSET]` + `[APP_SIZE]`
- `[SPIFFS_OFFSET]` = `[COREDUMP_OFFSET]` + 0x10000
- All offsets must be 64KB-aligned (multiples of 0x10000)

### Step 3: Verify the layout

Use this Python snippet to verify alignment and total size:

```python
parts = [
    ("nvs", 0x9000, 0x5000),
    ("otadata", 0xE000, 0x2000),
    ("app0", 0x10000, 0x1E0000),     # Replace with your values
    ("app1", 0x1F0000, 0x1E0000),    # Replace with your values
    ("coredump", 0x3D0000, 0x10000), # Replace with your values
    ("spiffs", 0x3E0000, 0x20000),   # Replace with your values
]

flash_size = 0x400000  # 4MB, adjust for your board

for name, offset, size in parts:
    end = offset + size
    print(f"{name:10} offset=0x{offset:06X} size=0x{size:06X} end=0x{end:06X}")
    if name not in ["nvs", "otadata"] and offset % 0x10000 != 0:
        print(f"  ⚠️  ERROR: Offset not aligned to 64KB!")

total_end = parts[-1][1] + parts[-1][2]
if total_end != flash_size:
    print(f"\n⚠️  WARNING: Partitions end at 0x{total_end:06X}, flash is 0x{flash_size:06X}")
else:
    print(f"\n✅ Partitions exactly fill {flash_size // (1024*1024)}MB flash")
```

### Step 4: Install and configure

1. Copy the partition file to Arduino ESP32 package:
   ```bash
   cp your_custom_partition.csv ~/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/
   ```

2. Update your build configuration to use the custom partition:
   ```bash
   # Arduino CLI
   arduino-cli compile --fqbn "esp32:esp32:YOUR_BOARD:PartitionScheme=your_custom_partition" ...
   
   # Or in platformio.ini
   board_build.partitions = your_custom_partition.csv
   ```

3. Flash with serial (required when changing partition table):
   ```bash
   arduino-cli upload --fqbn "..." --port /dev/ttyUSB0 ...
   ```

## Troubleshooting

### "Partition app1 invalid: Offset not aligned"
- Check that all partition offsets (except nvs/otadata) are multiples of 0x10000 (64KB)

### "Sketch too big"
- Your firmware exceeds the app partition size
- Either reduce firmware size or increase app partition size (may require reducing SPIFFS)

### OTA fails after uploading new partition table
- The OTA data partition may have stale data
- Flash via serial to reset partition metadata
- Or add `EraseFlash=all` option during serial upload

### Build can't find partition file
- Ensure the CSV file is in `~/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/`
- Filename (without .csv) must match `PartitionScheme=` value in FQBN

## Benefits of Custom Partitions

✅ **Optimal space usage**: Maximize app size for your specific needs  
✅ **OTA support maintained**: Safe firmware updates without serial cable  
✅ **Flexible allocation**: Adjust SPIFFS/coredump based on actual usage  
✅ **Future-proof**: Leave headroom for firmware growth  

## When NOT to Use Custom Partitions

- **Prototype/development**: Use standard partitions until firmware size stabilizes
- **Multiple board types**: Standard partitions are more portable
- **Frequent partition changes**: Requires re-flashing via serial each time
- **Minimal technical expertise**: Standard partitions are safer for beginners

## References

- [Espressif Partition Table Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)
- [Arduino ESP32 Partition Schemes](https://github.com/espressif/arduino-esp32/tree/master/tools/partitions)
- [Custom Partitions Guide](https://docs.espressif.com/projects/esp-at/en/latest/esp32c3/Compile_and_Develop/How_to_customize_partitions.html)
