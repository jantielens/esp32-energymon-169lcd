/*
 * MQTT Manager
 * 
 * Manages MQTT connection and subscriptions for energy monitoring.
 * Subscribes to solar power and grid power topics, parses incoming messages.
 * 
 * USAGE:
 *   mqtt_manager_init(&device_config);  // Initialize with config
 *   mqtt_manager_loop();                 // Call in main loop
 *   float solar = mqtt_manager_get_solar_power();
 *   float grid = mqtt_manager_get_grid_power();
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include "config_manager.h"

// API Functions
void mqtt_manager_init(const DeviceConfig *config);  // Initialize MQTT with config
void mqtt_manager_loop();                             // Process MQTT messages (call in loop)
float mqtt_manager_get_solar_power();                 // Get last solar power value (kW), NAN if not received
float mqtt_manager_get_grid_power();                  // Get last grid power value (kW), NAN if not received
bool mqtt_manager_is_connected();                     // Check if MQTT is connected

#endif // MQTT_MANAGER_H
