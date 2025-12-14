# MQTT Integration Guide

Guide to integrating the ESP32 Energy Monitor with MQTT brokers and Home Assistant.

## MQTT Topic Format

The device subscribes to two topics for power data:

### Solar Power Topic

- **Format**: Plain float value in kilowatts (kW)
- **Example topic**: `home/solar/power`
- **Example message**: `2.5` (= 2.5 kW)
- **Update rate**: Recommended ≥ 1 Hz for smooth display

### Grid Power Topic

- **Format**: JSON object with `value` field in kilowatts (kW)
- **Example topic**: `home/grid/power`
- **Example message**: `{"value": 1.2}` (= 1.2 kW)
- **Update rate**: Recommended ≥ 1 Hz for smooth display

### Home Power (Calculated)

- **Not subscribed** - automatically calculated
- **Formula**: Home = Grid + Solar
- **Examples**:
  - Solar 2.0 kW + Grid 1.0 kW = **Home 3.0 kW**
  - Solar 3.0 kW + Grid -0.5 kW = **Home 2.5 kW** (exporting 0.5 kW)

## Home Assistant Integration

### Example Configuration

Add to `configuration.yaml`:

```yaml
# Solar Power Sensor
mqtt:
  sensor:
    - name: "Solar Power"
      state_topic: "home/solar/power"
      unit_of_measurement: "kW"
      device_class: power
      state_class: measurement
      
    - name: "Grid Power"  
      state_topic: "home/grid/power"
      unit_of_measurement: "kW"
      device_class: power
      state_class: measurement
      value_template: "{{ value_json.value }}"
```

### Publishing from Home Assistant

If you have power sensors in watts, convert to kilowatts:

```yaml
# Convert watts to kilowatts and publish via automation
automation:
  - alias: "Publish Solar Power to MQTT"
    trigger:
      - platform: state
        entity_id: sensor.solar_power_watts
    action:
      - service: mqtt.publish
        data:
          topic: "home/solar/power"
          payload: "{{ (states('sensor.solar_power_watts') | float / 1000) | round(3) }}"
          
  - alias: "Publish Grid Power to MQTT"
    trigger:
      - platform: state
        entity_id: sensor.grid_power_watts
    action:
      - service: mqtt.publish
        data:
          topic: "home/grid/power"
          payload: '{"value": {{ (states("sensor.grid_power_watts") | float / 1000) | round(3) }}}'
```

### Using Template Sensors

Create template sensors that publish to MQTT:

```yaml
template:
  - sensor:
      - name: "Solar Power kW"
        unit_of_measurement: "kW"
        state: "{{ (states('sensor.solar_power_watts') | float / 1000) | round(3) }}"
        
      - name: "Grid Power kW"
        unit_of_measurement: "kW"
        state: "{{ (states('sensor.grid_power_watts') | float / 1000) | round(3) }}"

# Publish to MQTT every second
automation:
  - alias: "Update ESP32 Power Display"
    trigger:
      - platform: time_pattern
        seconds: "/1"  # Every second
    action:
      - service: mqtt.publish
        data:
          topic: "home/solar/power"
          payload: "{{ states('sensor.solar_power_kw') }}"
      - service: mqtt.publish
        data:
          topic: "home/grid/power"
          payload: '{"value": {{ states("sensor.grid_power_kw") }}}'
```

## Testing MQTT Topics

### Install Mosquitto Clients

```bash
# Debian/Ubuntu
sudo apt install mosquitto-clients

# macOS
brew install mosquitto
```

### Subscribe to Topics

Monitor what's being published:

```bash
# Subscribe to solar topic
mosquitto_sub -h 192.168.1.100 -t home/solar/power -v

# Subscribe to grid topic
mosquitto_sub -h 192.168.1.100 -t home/grid/power -v

# Subscribe to all topics (for debugging)
mosquitto_sub -h 192.168.1.100 -t '#' -v
```

### Publish Test Values

Manually publish test data:

```bash
# Publish solar power (2.5 kW)
mosquitto_pub -h 192.168.1.100 -t home/solar/power -m "2.5"

# Publish grid power (1.2 kW)
mosquitto_pub -h 192.168.1.100 -t home/grid/power -m '{"value": 1.2}'

# Publish grid export (-0.5 kW = sending to grid)
mosquitto_pub -h 192.168.1.100 -t home/grid/power -m '{"value": -0.5}'
```

**Expected Display:**
- Values should update within 1-2 seconds
- Colors change based on configured thresholds
- Bar charts update proportionally

## MQTT Broker Setup

### Mosquitto Broker (Recommended)

Install on Raspberry Pi or server:

```bash
# Install Mosquitto
sudo apt update
sudo apt install mosquitto mosquitto-clients

# Enable and start service
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

Configure authentication (optional but recommended):

```bash
# Create password file
sudo mosquitto_passwd -c /etc/mosquitto/passwd username

# Edit /etc/mosquitto/mosquitto.conf
allow_anonymous false
password_file /etc/mosquitto/passwd

# Restart service
sudo systemctl restart mosquitto
```

### Home Assistant Built-In Broker

1. Install Mosquitto broker add-on from Home Assistant
2. Configure in Add-on settings:
   - Set username and password
   - Enable anonymous: false
3. Start the add-on
4. Use `homeassistant.local` or HA IP as broker address

### Cloud MQTT Brokers (Not Recommended)

For testing only (adds latency):
- CloudMQTT: https://www.cloudmqtt.com/
- HiveMQ Cloud: https://www.hivemq.com/cloud/

> **Note:** Cloud brokers add 100-500ms latency. Use local broker for best performance.

## Configuring the Device

### Via Web Portal

1. Navigate to **Network** page
2. **MQTT Broker Settings**:
   - Broker: `192.168.1.100` (your broker IP)
   - Port: `1883`
   - Username: `your-username` (if auth enabled)
   - Password: `your-password` (if auth enabled)
3. Click **Save & Reboot**
4. Navigate to **Home** page
5. **MQTT Topics**:
   - Solar Power Topic: `home/solar/power`
   - Grid Power Topic: `home/grid/power`
6. Click **Save**

### Via REST API

```bash
# Update MQTT configuration
curl -X POST http://energy-monitor.local/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "mqtt_broker": "192.168.1.100",
    "mqtt_port": 1883,
    "mqtt_username": "esp32",
    "mqtt_password": "secret",
    "mqtt_topic_solar": "home/solar/power",
    "mqtt_topic_grid": "home/grid/power"
  }'
```

## Troubleshooting

### No Data on Display ("-- kW")

1. **Check MQTT connection**:
   - View serial monitor for "MQTT connected" message
   - Verify broker IP and credentials
   
2. **Verify topics are publishing**:
   ```bash
   mosquitto_sub -h 192.168.1.100 -t home/solar/power -v
   mosquitto_sub -h 192.168.1.100 -t home/grid/power -v
   ```

3. **Check data format**:
   - Solar: Plain number (not JSON)
   - Grid: JSON with `value` field
   - Both in kilowatts (not watts!)

### Incorrect Home Calculation

1. **Verify units**: Both topics must use **kilowatts (kW)**
   - Common error: One topic in watts, other in kW
   
2. **Check signs**: Grid power should be:
   - Positive when importing from grid
   - Negative when exporting to grid

### MQTT Disconnects Frequently

1. **Check WiFi signal**: Move device closer to router
2. **Broker capacity**: Ensure broker can handle connections
3. **Network stability**: Check for network interruptions
4. **Authentication**: Verify credentials are correct

### Delayed Updates

1. **Publishing frequency**: Ensure topics publish at ≥ 1 Hz
2. **Network latency**: Use local broker (not cloud)
3. **WiFi congestion**: Reduce interference on 2.4 GHz
4. **QoS settings**: Use QoS 0 for lowest latency

## Advanced Topics

### Multiple Devices

Each device should have unique:
- Device name (e.g., "Kitchen Monitor", "Office Monitor")
- Can subscribe to same or different MQTT topics
- All devices can share same MQTT broker

### Custom Topic Formats

The device expects specific formats, but you can adapt using:
- **Node-RED**: Transform messages between formats
- **Home Assistant templates**: Convert data before publishing
- **MQTT bridge**: Republish in expected format

### Security Best Practices

1. **Enable MQTT authentication** (username/password)
2. **Use TLS encryption** for sensitive networks (requires code modification)
3. **Firewall MQTT port** (1883) from external access
4. **Unique passwords** for each device
5. **Regular broker updates** for security patches

## Related Documentation

- [Configuration Guide](configuration.md) - Web portal settings
- [Troubleshooting](troubleshooting.md) - Common issues
- [GETTING-STARTED.md](../../GETTING-STARTED.md) - Initial setup

---

**Need help?** [Create an issue](https://github.com/jantielens/esp32-energymon-169lcd/issues) with your MQTT broker and topic configuration.
