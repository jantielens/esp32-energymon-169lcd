#pragma once

#include <Arduino.h>
#include "config_manager.h"

struct WebPortalState {
    bool ap_mode_active = false;
    DeviceConfig* current_config = nullptr;

    // OTA
    bool ota_in_progress = false;
    size_t ota_progress = 0;
    size_t ota_total = 0;

    // LCD brightness (current runtime value, may differ from saved)
    uint8_t current_brightness = 100;

    // CPU usage tracking (health endpoint)
    uint32_t last_idle_runtime = 0;
    uint32_t last_total_runtime = 0;
    unsigned long last_cpu_check = 0;
};

extern WebPortalState g_web_portal_state;
