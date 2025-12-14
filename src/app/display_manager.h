#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>
#include "board_config.h"

void display_init();
void display_update();

// FPS counter for performance testing
void display_update_fps();

// Boot progress tracking (0-100%)
void display_set_boot_progress(int percent, const char* status);

// Switch to power screen (after boot complete)
void display_show_power_screen();

// Update power values on power screen
void display_update_energy(float solar_kw, float grid_kw);

// Image display API (10-second timeout, auto-return to power screen)
bool display_show_image(const uint8_t* jpeg_data, size_t jpeg_size);
void display_hide_image();  // Manual dismiss (also called automatically after timeout)

#endif // DISPLAY_MANAGER_H
