# Power Thresholds Guide

Detailed guide to customizing power color thresholds and understanding the color-coded display system.

> **Quick Start:** See [configuration.md](configuration.md#power-color-thresholds) for basic threshold configuration.

## Overview

The ESP32 Energy Monitor uses color-coded visual indicators to provide at-a-glance understanding of your energy status. Each power type (Grid, Home, Solar) has 3 configurable thresholds that define 4 color zones.

## Color Zones

### Standard Color Scheme (Defaults)

| Zone | Color | Meaning | Use Case |
|------|-------|---------|----------|
| **Good** | 🟢 Green (`#00FF00`) | Optimal/desired state | Low consumption or exporting |
| **OK** | ⚪ White (`#FFFFFF`) | Normal operation | Typical usage levels |
| **Attention** | 🟠 Orange (`#FFA500`) | Elevated but acceptable | Higher than normal |
| **Warning** | 🔴 Red (`#FF0000`) | High usage alert | Peak consumption |

All colors are fully customizable via web portal or API.

## Grid Power Thresholds

**Default Thresholds:** `[0.0, 0.5, 2.5]` kW

| Power Range | Color | Meaning |
|-------------|-------|---------|
| < 0.0 kW | 🟢 Green | Exporting to grid (selling power) |
| 0.0 - 0.5 kW | ⚪ White | Minimal grid import |
| 0.5 - 2.5 kW | 🟠 Orange | Moderate grid import |
| > 2.5 kW | 🔴 Red | High grid import |

**Customization Tips:**
- **Solar households**: Set Threshold 0 to negative value (e.g., -0.5 kW) to differentiate small export from zero
- **High consumption**: Increase Threshold 2 if you regularly use > 3 kW
- **Budget conscious**: Lower thresholds to encourage conservation

## Home Power Thresholds

**Default Thresholds:** `[0.5, 1.0, 2.0]` kW

| Power Range | Color | Meaning |
|-------------|-------|---------|
| < 0.5 kW | 🟢 Green | Low consumption |
| 0.5 - 1.0 kW | ⚪ White | Normal consumption |
| 1.0 - 2.0 kW | 🟠 Orange | Moderate consumption |
| > 2.0 kW | 🔴 Red | High consumption |

**Customization Tips:**
- **Small household**: Lower thresholds (e.g., [0.3, 0.7, 1.5])
- **Large household**: Raise thresholds (e.g., [1.0, 2.0, 4.0])
- **Conservation goals**: Lower thresholds to see orange/red more often as reminder

## Solar Power Thresholds

**Default Thresholds:** `[0.5, 1.5, 3.0]` kW

| Power Range | Color | Meaning |
|-------------|-------|---------|
| < 0.5 kW | ⚪ White | No/minimal generation |
| 0.5 - 1.5 kW | 🟢 Green | Active generation |
| 1.5 - 3.0 kW | 🟢 Green | Good generation |
| > 3.0 kW | 🟢 Green | Excellent generation |

**Note:** Solar uses green for ALL generation levels above 0.5 kW. This encourages positive feedback for any solar production.

**Customization Tips:**
- **Small system** (< 3 kW): Lower thresholds to match your max output
- **Large system** (> 5 kW): Raise thresholds for better visual differentiation
- **Performance tracking**: Use orange/red for below-expected generation (requires inverting color scheme)

## Configuration Methods

### Web Portal

1. Navigate to **Home** page
2. Scroll to **"Power Color Thresholds"** section
3. For each power type:
   - Adjust threshold sliders (Threshold 0, 1, 2)
   - Click color pickers to change zone colors
4. Click **"Save"** to apply changes
5. Display updates **within 1 second** (no reboot needed!)

**Validation:**
- Thresholds must be in ascending order: T0 ≤ T1 ≤ T2
- Invalid values show error message
- Changes are validated on client and server

### REST API

Get current thresholds:

```bash
curl http://energy-monitor.local/api/config
```

Update thresholds (without reboot):

```bash
curl -X POST http://energy-monitor.local/api/config?no_reboot=1 \
  -H "Content-Type: application/json" \
  -d '{
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
  }'
```

**Color Format:** RGB565 as decimal integer:
- Green (`#00FF00`): 65280
- White (`#FFFFFF`): 16777215  
- Orange (`#FFA500`): 16753920
- Red (`#FF0000`): 16711680

### Factory Reset

Restore all thresholds and colors to defaults:

1. Web portal: Click **"Restore Factory Defaults"** button
2. Or factory reset device via Firmware page
3. Or via API:
   ```bash
   curl -X DELETE http://energy-monitor.local/api/config
   ```

## Example Configurations

### Conservative (Encourage Low Usage)

**Goal:** See orange/red frequently to remind yourself to reduce consumption.

```json
{
  "grid_threshold_0": -0.2,
  "grid_threshold_1": 0.2,
  "grid_threshold_2": 1.0,
  "home_threshold_0": 0.3,
  "home_threshold_1": 0.6,
  "home_threshold_2": 1.2
}
```

### Large Household

**Goal:** Thresholds match higher typical consumption.

```json
{
  "grid_threshold_0": 0.0,
  "grid_threshold_1": 1.0,
  "grid_threshold_2": 4.0,
  "home_threshold_0": 1.0,
  "home_threshold_1": 2.5,
  "home_threshold_2": 5.0
}
```

### Solar-Focused

**Goal:** Highlight export status and solar performance.

```json
{
  "grid_threshold_0": -0.5,
  "grid_threshold_1": 0.0,
  "grid_threshold_2": 1.5,
  "solar_threshold_0": 0.3,
  "solar_threshold_1": 1.0,
  "solar_threshold_2": 2.0
}
```

### Custom Colors (Blue Theme)

**Goal:** Match home decor with blue color scheme.

```json
{
  "color_good": 255,       // Blue (#0000FF)
  "color_ok": 16777215,    // White (#FFFFFF)
  "color_attention": 65535, // Cyan (#00FFFF)
  "color_warning": 31     // Dark Blue (#00001F)
}
```

## Understanding the Visual Display

### What Changes Color?

When thresholds are crossed, these elements change color:

- **Icons**: Sun (solar), house (home), lightning (grid)
- **Power Values**: Numeric kW values
- **Unit Labels**: "kW" text

### What Doesn't Change?

- **Bar charts**: Always use zone colors (good=bottom, warning=top)
- **Background**: Always black
- **Arrows**: Always white
- **Labels**: "SOLAR", "HOME", "GRID" text (always white)

### Real-Time Updates

- **Update frequency**: MQTT messages trigger immediate color recalculation
- **Animation**: None (instant color transitions)
- **Latency**: < 100ms from MQTT message to color change

## Troubleshooting

### Colors Don't Change

1. **Check MQTT data**: Ensure power values are being received
2. **Verify thresholds**: Open web portal to confirm saved values
3. **Check ranges**: Ensure actual power crosses threshold boundaries
4. **Serial monitor**: Look for "update_power_colors()" debug output

### Thresholds Won't Save

1. **Validation errors**: Ensure T0 ≤ T1 ≤ T2
2. **NVS storage full**: Try factory reset to clear storage
3. **Network issues**: Check web portal connection stability

### Wrong Colors Displayed

1. **RGB/BGR issue**: If all colors are inverted, report as bug (firmware handles conversion)
2. **Custom colors**: Verify color format (RGB565 decimal or hex)
3. **Display calibration**: Some LCDs may have slight color variations

## Advanced Usage

### Automation Examples

Change thresholds based on time of day:

```bash
# Morning: Conservative thresholds
curl -X POST http://energy-monitor.local/api/config?no_reboot=1 \
  -d '{"grid_threshold_2": 1.5}'

# Evening: Relaxed thresholds  
curl -X POST http://energy-monitor.local/api/config?no_reboot=1 \
  -d '{"grid_threshold_2": 3.0}'
```

### Seasonal Adjustments

Solar output varies by season - adjust thresholds accordingly:

**Summer** (high solar output):
```json
{"solar_threshold_0": 1.0, "solar_threshold_1": 2.5, "solar_threshold_2": 4.0}
```

**Winter** (low solar output):
```json
{"solar_threshold_0": 0.2, "solar_threshold_1": 0.8, "solar_threshold_2": 1.5}
```

## Related Documentation

- [Configuration Guide](configuration.md) - Web portal settings
- [MQTT Integration](mqtt-integration.md) - Topic setup
- [API Reference](../developer/web-portal-api.md) - REST API details

---

**Share your threshold configurations!** [Create a discussion](https://github.com/jantielens/esp32-energymon-169lcd/discussions) to help others optimize their settings.
