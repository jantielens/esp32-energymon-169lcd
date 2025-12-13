/*
 * MQTT Manager Implementation
 * 
 * Uses PubSubClient library for MQTT connectivity.
 * Handles connection, subscription, and message parsing.
 */

#include "mqtt_manager.h"
#include "log_manager.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Increase MQTT buffer size for large JSON messages
#define MQTT_MAX_PACKET_SIZE 512

// MQTT client instances
static WiFiClient wifi_client;
static PubSubClient mqtt_client(wifi_client);

// Configuration
static char mqtt_broker[CONFIG_MQTT_BROKER_MAX_LEN] = "";
static uint16_t mqtt_port = 1883;
static char mqtt_username[CONFIG_MQTT_USERNAME_MAX_LEN] = "";
static char mqtt_password[CONFIG_MQTT_PASSWORD_MAX_LEN] = "";
static char topic_solar[CONFIG_MQTT_TOPIC_MAX_LEN] = "";
static char topic_grid[CONFIG_MQTT_TOPIC_MAX_LEN] = "";

// Power values (NAN = not received yet)
static float solar_power_kw = NAN;
static float grid_power_kw = NAN;

// Connection retry settings
static unsigned long last_reconnect_attempt = 0;
static const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds

// MQTT callback for incoming messages
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    // Create null-terminated string from payload
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Logger.logMessagef("MQTT", "Received on %s: %s", topic, message);
    
    // Check which topic this message is from
    if (strcmp(topic, topic_solar) == 0) {
        // Solar topic: Direct float value
        solar_power_kw = atof(message);
        Logger.logMessagef("MQTT", "Solar power updated: %.3f kW", solar_power_kw);
    } 
    else if (strcmp(topic, topic_grid) == 0) {
        // Grid topic: JSON with "value" field
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (error) {
            Logger.logMessagef("MQTT", "Failed to parse grid JSON: %s", error.c_str());
            return;
        }
        
        if (doc.containsKey("value")) {
            grid_power_kw = doc["value"];
            Logger.logMessagef("MQTT", "Grid power updated: %.3f kW", grid_power_kw);
        } else {
            Logger.logMessage("MQTT", "Grid JSON missing 'value' field");
        }
    }
}

// Attempt to connect to MQTT broker
bool mqtt_reconnect() {
    if (strlen(mqtt_broker) == 0) {
        Logger.logMessage("MQTT", "No broker configured, skipping connection");
        return false;
    }
    
    Logger.logMessagef("MQTT", "Connecting to %s:%d", mqtt_broker, mqtt_port);
    
    // Generate unique client ID
    String client_id = "ESP32-" + WiFi.macAddress();
    client_id.replace(":", "");
    
    bool connected = false;
    if (strlen(mqtt_username) > 0) {
        connected = mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password);
    } else {
        connected = mqtt_client.connect(client_id.c_str());
    }
    
    if (connected) {
        Logger.logMessage("MQTT", "Connected successfully");
        
        // Subscribe to topics
        if (strlen(topic_solar) > 0) {
            if (mqtt_client.subscribe(topic_solar)) {
                Logger.logMessagef("MQTT", "Subscribed to solar: %s", topic_solar);
            } else {
                Logger.logMessagef("MQTT", "Failed to subscribe to solar: %s", topic_solar);
            }
        }
        
        if (strlen(topic_grid) > 0) {
            if (mqtt_client.subscribe(topic_grid)) {
                Logger.logMessagef("MQTT", "Subscribed to grid: %s", topic_grid);
            } else {
                Logger.logMessagef("MQTT", "Failed to subscribe to grid: %s", topic_grid);
            }
        }
        
        return true;
    } else {
        Logger.logMessagef("MQTT", "Connection failed, state=%d", mqtt_client.state());
        return false;
    }
}

// Initialize MQTT manager
void mqtt_manager_init(const DeviceConfig *config) {
    if (!config) {
        Logger.logMessage("MQTT", "Config is NULL, skipping init");
        return;
    }
    
    // Copy configuration
    strlcpy(mqtt_broker, config->mqtt_broker, CONFIG_MQTT_BROKER_MAX_LEN);
    mqtt_port = config->mqtt_port;
    strlcpy(mqtt_username, config->mqtt_username, CONFIG_MQTT_USERNAME_MAX_LEN);
    strlcpy(mqtt_password, config->mqtt_password, CONFIG_MQTT_PASSWORD_MAX_LEN);
    strlcpy(topic_solar, config->mqtt_topic_solar, CONFIG_MQTT_TOPIC_MAX_LEN);
    strlcpy(topic_grid, config->mqtt_topic_grid, CONFIG_MQTT_TOPIC_MAX_LEN);
    
    // Skip if no broker configured
    if (strlen(mqtt_broker) == 0) {
        Logger.logMessage("MQTT", "No broker configured");
        return;
    }
    
    Logger.logBegin("MQTT Init");
    Logger.logLinef("Broker: %s:%d", mqtt_broker, mqtt_port);
    Logger.logLinef("Username: %s", strlen(mqtt_username) > 0 ? mqtt_username : "(none)");
    Logger.logLinef("Solar topic: %s", strlen(topic_solar) > 0 ? topic_solar : "(none)");
    Logger.logLinef("Grid topic: %s", strlen(topic_grid) > 0 ? topic_grid : "(none)");
    Logger.logEnd();
    
    // Configure MQTT client
    mqtt_client.setServer(mqtt_broker, mqtt_port);
    mqtt_client.setBufferSize(MQTT_MAX_PACKET_SIZE);
    mqtt_client.setCallback(mqtt_callback);
    
    // Initial connection attempt
    mqtt_reconnect();
}

// Process MQTT (call in main loop)
void mqtt_manager_loop() {
    if (!mqtt_client.connected()) {
        // Attempt reconnection with backoff
        unsigned long now = millis();
        if (now - last_reconnect_attempt > RECONNECT_INTERVAL) {
            last_reconnect_attempt = now;
            mqtt_reconnect();
        }
    } else {
        // Process incoming messages
        mqtt_client.loop();
    }
}

// Get solar power value
float mqtt_manager_get_solar_power() {
    return solar_power_kw;
}

// Get grid power value
float mqtt_manager_get_grid_power() {
    return grid_power_kw;
}

// Check connection status
bool mqtt_manager_is_connected() {
    return mqtt_client.connected();
}
