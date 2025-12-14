/*
 * Power Screen
 * 
 * Displays solar and grid power in 3-column layout.
 * Layout: Solar → Home ← Grid
 * Home consumption calculated as: home = grid + solar
 */

#ifndef SCREEN_POWER_H
#define SCREEN_POWER_H

#include "screen_base.h"
#include <math.h>

class PowerScreen : public ScreenBase {
public:
    PowerScreen();
    ~PowerScreen();
    
    void create() override;
    void destroy() override;
    void update() override;
    void show() override;
    void hide() override;
    
    // Update power values (in kW)
    void set_solar_power(float kw);
    void set_grid_power(float kw);

private:
    // UI elements
    lv_obj_t* background = nullptr;
    lv_obj_t* solar_icon = nullptr;
    lv_obj_t* home_icon = nullptr;
    lv_obj_t* grid_icon = nullptr;
    lv_obj_t* arrow1 = nullptr;
    lv_obj_t* arrow2 = nullptr;
    lv_obj_t* solar_value = nullptr;
    lv_obj_t* home_value = nullptr;
    lv_obj_t* grid_value = nullptr;
    lv_obj_t* solar_unit = nullptr;
    lv_obj_t* home_unit = nullptr;
    lv_obj_t* grid_unit = nullptr;
    lv_obj_t* solar_bar = nullptr;
    lv_obj_t* home_bar = nullptr;
    lv_obj_t* grid_bar = nullptr;
    
    // Statistics overlay lines (min/avg/max indicators)
    // Box plot overlay elements (per power type: box + median line + min/max lines)
    // Min/max overlay lines
    lv_obj_t* solar_line_min = nullptr;
    lv_obj_t* solar_line_max = nullptr;
    lv_obj_t* home_line_min = nullptr;
    lv_obj_t* home_line_max = nullptr;
    lv_obj_t* grid_line_min = nullptr;
    lv_obj_t* grid_line_max = nullptr;
    
    // Line points arrays (must persist for LVGL)
    lv_point_t solar_min_points[2];
    lv_point_t solar_max_points[2];
    lv_point_t home_min_points[2];
    lv_point_t home_max_points[2];
    lv_point_t grid_min_points[2];
    lv_point_t grid_max_points[2];
    
    // Current values
    float solar_kw = NAN;
    float grid_kw = NAN;
    
    // Rolling statistics (10 minutes @ 5-second sampling = 120 samples)
    // Using void* to avoid exposing implementation details in header
    void* solar_stats = nullptr;
    void* home_stats = nullptr;
    void* grid_stats = nullptr;
    
    // Helper to update calculated home value
    void update_home_value();
    
    // Color calculation helpers (unified algorithm with configurable thresholds)
    lv_color_t rgb_to_bgr(uint32_t rgb);  // Convert RGB to BGR for display
    lv_color_t get_power_color(float value, float thresholds[3]);  // Unified threshold logic
    lv_color_t get_grid_color(float kw);   // Wrapper for grid thresholds
    lv_color_t get_home_color(float kw);   // Wrapper for home thresholds
    lv_color_t get_solar_color(float kw);  // Wrapper for solar thresholds
    
    // Widget update helpers
    void update_power_colors();
    void update_bar_charts();
    void update_statistics();
    void update_stat_overlays();
};

#endif // SCREEN_POWER_H
