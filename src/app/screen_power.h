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
    
    // Current values
    float solar_kw = NAN;
    float grid_kw = NAN;
    
    // Helper to update calculated home value
    void update_home_value();
    
    // NEW: Color calculation helpers
    lv_color_t get_grid_color(float kw);
    lv_color_t get_home_color(float kw);
    lv_color_t get_solar_color(float kw);
    
    // NEW: Widget update helpers
    void update_power_colors();
    void update_bar_charts();
};

#endif // SCREEN_POWER_H
