/*
 * Power Screen Implementation
 * 
 * 3-column layout adapted from energy_screen.cpp sample
 */

#include "screen_power.h"
#include "config_manager.h"
#include "icons.h"
#include <math.h>
#include <stdio.h>

// External reference to global configuration
extern DeviceConfig device_config;

// Color definitions for power state visualization
// 
// Display hardware is BGR mode, and we swap RGB→BGR in flush callback to fix anti-aliasing
// Therefore, we define colors in BGR format here so they display correctly after the swap
// 
// BGR color mapping (what we define → what displays):
//   Define BLUE 0x0000FF   → displays as RED (after RGB→BGR swap)
//   Define GREEN 0x00FF00  → displays as GREEN (G channel unaffected)
//   Define RED 0xFF0000    → displays as BLUE (after RGB→BGR swap)
//   Define 0x00A5FF        → displays as ORANGE (BGR for orange)
//
const lv_color_t COLOR_BRIGHT_GREEN = lv_color_hex(0x00FF00);  // Green (unaffected by swap)
const lv_color_t COLOR_WHITE        = lv_color_hex(0xFFFFFF);  // White (unaffected)
const lv_color_t COLOR_ORANGE       = lv_color_hex(0x00A5FF);  // BGR for orange (displays as RGB 255,165,0)
const lv_color_t COLOR_RED          = lv_color_hex(0x0000FF);  // BGR for red (displays as RGB 255,0,0)

#define TEXT_COLOR lv_color_hex(0xFFFFFF)

// ============================================================================
// PowerStatistics Class - Rolling Window Statistics
// ============================================================================

class PowerStatistics {
public:
    PowerStatistics(size_t buffer_size = 600) : buffer_size(buffer_size) {
        buffer.reserve(buffer_size);
    }
    
    void addSample(float value) {
        if (isnan(value)) return;  // Ignore invalid samples
        
        if (buffer.size() < buffer_size) {
            // Buffer not full yet
            buffer.push_back(value);
            running_sum += value;
        } else {
            // Circular buffer - replace oldest
            running_sum -= buffer[head];
            buffer[head] = value;
            running_sum += value;
            head = (head + 1) % buffer_size;
        }
        
        // Recalculate min/max (could optimize with heap, but 120 samples is fast enough)
        recalculate();
    }
    
    float getMin() const { return cached_min; }
    float getMax() const { return cached_max; }
    float getAvg() const { return cached_avg; }
    
    bool hasData() const { return !buffer.empty(); }
    size_t getSampleCount() const { return buffer.size(); }
    
    void clear() {
        buffer.clear();
        head = 0;
        running_sum = 0.0f;
        cached_min = NAN;
        cached_max = NAN;
        cached_avg = NAN;
    }
    
private:
    std::vector<float> buffer;
    size_t buffer_size;
    size_t head = 0;
    float running_sum = 0.0f;
    
    // Cached statistics
    float cached_min = NAN;
    float cached_max = NAN;
    float cached_avg = NAN;
    
    void recalculate() {
        if (buffer.empty()) {
            cached_min = cached_max = cached_avg = NAN;
            return;
        }
        
        // Calculate min/max
        cached_min = buffer[0];
        cached_max = buffer[0];
        for (const float& val : buffer) {
            if (val < cached_min) cached_min = val;
            if (val > cached_max) cached_max = val;
        }
        
        // Calculate average from running sum
        cached_avg = running_sum / buffer.size();
    }
};

PowerScreen::PowerScreen() {
    // Initialize statistics trackers (10 minutes @ 1-second sampling = 600 samples)
    solar_stats = static_cast<void*>(new PowerStatistics(600));
    home_stats = static_cast<void*>(new PowerStatistics(600));
    grid_stats = static_cast<void*>(new PowerStatistics(600));
}

PowerScreen::~PowerScreen() {
    destroy();
    
    // Clean up statistics
    delete static_cast<PowerStatistics*>(solar_stats);
    delete static_cast<PowerStatistics*>(home_stats);
    delete static_cast<PowerStatistics*>(grid_stats);
    solar_stats = nullptr;
    home_stats = nullptr;
    grid_stats = nullptr;
}

// Helper: Convert RGB to BGR for display hardware compatibility
lv_color_t PowerScreen::rgb_to_bgr(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    return lv_color_hex((b << 16) | (g << 8) | r);
}

// Unified color determination based on configurable thresholds
lv_color_t PowerScreen::get_power_color(float value, float thresholds[3]) {
    if (isnan(value)) {
        // No data - use "ok" color (typically white)
        return rgb_to_bgr(device_config.color_ok);
    }
    
    uint32_t rgb;
    if (value < thresholds[0]) {
        rgb = device_config.color_good;
    } else if (value < thresholds[1]) {
        rgb = device_config.color_ok;
    } else if (value < thresholds[2]) {
        rgb = device_config.color_attention;
    } else {
        rgb = device_config.color_warning;
    }
    
    return rgb_to_bgr(rgb);
}

// Convenience wrappers for each power type
lv_color_t PowerScreen::get_grid_color(float kw) {
    return get_power_color(kw, device_config.grid_threshold);
}

lv_color_t PowerScreen::get_home_color(float kw) {
    return get_power_color(kw, device_config.home_threshold);
}

lv_color_t PowerScreen::get_solar_color(float kw) {
    return get_power_color(kw, device_config.solar_threshold);
}

void PowerScreen::update_power_colors() {
    // Calculate colors based on current values
    lv_color_t solar_color = get_solar_color(solar_kw);
    lv_color_t grid_color = get_grid_color(grid_kw);
    
    float home_kw = NAN;
    if (!isnan(solar_kw) && !isnan(grid_kw)) {
        home_kw = solar_kw + grid_kw;
    }
    lv_color_t home_color = get_home_color(home_kw);
    
    // Apply colors to solar widgets
    if (solar_icon) {
        lv_obj_set_style_img_recolor(solar_icon, solar_color, LV_PART_MAIN);
    }
    if (solar_value) {
        lv_obj_set_style_text_color(solar_value, solar_color, LV_PART_MAIN);
    }
    if (solar_unit) {
        lv_obj_set_style_text_color(solar_unit, solar_color, LV_PART_MAIN);
    }
    if (arrow1) {
        lv_obj_set_style_text_color(arrow1, solar_color, LV_PART_MAIN);
    }
    
    // Apply colors to home widgets
    if (home_icon) {
        lv_obj_set_style_img_recolor(home_icon, home_color, LV_PART_MAIN);
    }
    if (home_value) {
        lv_obj_set_style_text_color(home_value, home_color, LV_PART_MAIN);
    }
    if (home_unit) {
        lv_obj_set_style_text_color(home_unit, home_color, LV_PART_MAIN);
    }
    
    // Apply colors to grid widgets
    if (grid_icon) {
        lv_obj_set_style_img_recolor(grid_icon, grid_color, LV_PART_MAIN);
    }
    if (grid_value) {
        lv_obj_set_style_text_color(grid_value, grid_color, LV_PART_MAIN);
    }
    if (grid_unit) {
        lv_obj_set_style_text_color(grid_unit, grid_color, LV_PART_MAIN);
    }
    if (arrow2) {
        lv_obj_set_style_text_color(arrow2, grid_color, LV_PART_MAIN);
    }
    
    // Update bar charts with matching colors
    update_bar_charts();
}

void PowerScreen::create() {
    if (screen_obj) return;  // Already created
    
    // Create screen object
    screen_obj = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x000000), 0);  // Pure black
    
    // Create full-screen background (use LV_HOR_RES and LV_VER_RES for current dimensions)
    background = lv_obj_create(screen_obj);
    lv_obj_set_size(background, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(background, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(background, 0, 0);
    lv_obj_set_style_border_width(background, 0, 0);
    lv_obj_set_style_radius(background, 0, 0);
    lv_obj_set_style_bg_color(background, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    
    // Icons row at Y=25px
    // Solar icon at X=53px
    solar_icon = lv_img_create(background);
    lv_img_set_src(solar_icon, &sun);
    lv_obj_set_style_img_recolor(solar_icon, COLOR_WHITE, 0);
    lv_obj_set_style_img_recolor_opa(solar_icon, LV_OPA_COVER, 0);
    lv_obj_align(solar_icon, LV_ALIGN_TOP_MID, -107, 15);
    
    // Arrow 1 (>) at X=107px
    arrow1 = lv_label_create(background);
    lv_label_set_text(arrow1, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow1, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(arrow1, COLOR_WHITE, 0);
    lv_obj_align(arrow1, LV_ALIGN_TOP_MID, -53, 25);
    
    // Home icon at X=160px (center)
    home_icon = lv_img_create(background);
    lv_img_set_src(home_icon, &home);
    lv_obj_set_style_img_recolor(home_icon, COLOR_WHITE, 0);
    lv_obj_set_style_img_recolor_opa(home_icon, LV_OPA_COVER, 0);
    lv_obj_align(home_icon, LV_ALIGN_TOP_MID, 0, 15);
    
    // Arrow 2 (< or >) at X=213px
    arrow2 = lv_label_create(background);
    lv_label_set_text(arrow2, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow2, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(arrow2, COLOR_WHITE, 0);
    lv_obj_align(arrow2, LV_ALIGN_TOP_MID, 53, 25);
    
    // Grid icon at X=267px
    grid_icon = lv_img_create(background);
    lv_img_set_src(grid_icon, &grid);
    lv_obj_set_style_img_recolor(grid_icon, COLOR_WHITE, 0);
    lv_obj_set_style_img_recolor_opa(grid_icon, LV_OPA_COVER, 0);
    lv_obj_align(grid_icon, LV_ALIGN_TOP_MID, 107, 15);
    
    // Values row at Y=90px
    // Solar value
    solar_value = lv_label_create(background);
    lv_label_set_text(solar_value, "--");
    lv_obj_set_style_text_font(solar_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(solar_value, COLOR_WHITE, 0);
    lv_obj_align(solar_value, LV_ALIGN_TOP_MID, -107, 80);
    
    // Home value
    home_value = lv_label_create(background);
    lv_label_set_text(home_value, "--");
    lv_obj_set_style_text_font(home_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(home_value, COLOR_WHITE, 0);
    lv_obj_align(home_value, LV_ALIGN_TOP_MID, 0, 80);
    
    // Grid value
    grid_value = lv_label_create(background);
    lv_label_set_text(grid_value, "--");
    lv_obj_set_style_text_font(grid_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(grid_value, COLOR_WHITE, 0);
    lv_obj_align(grid_value, LV_ALIGN_TOP_MID, 107, 80);
    
    // Units row at Y=105px
    // Solar unit
    solar_unit = lv_label_create(background);
    lv_label_set_text(solar_unit, "kW");
    lv_obj_set_style_text_font(solar_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(solar_unit, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(solar_unit, LV_ALIGN_TOP_MID, -107, 115);
    
    // Home unit
    home_unit = lv_label_create(background);
    lv_label_set_text(home_unit, "kW");
    lv_obj_set_style_text_font(home_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(home_unit, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(home_unit, LV_ALIGN_TOP_MID, 0, 115);
    
    // Grid unit
    grid_unit = lv_label_create(background);
    lv_label_set_text(grid_unit, "kW");
    lv_obj_set_style_text_font(grid_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(grid_unit, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(grid_unit, LV_ALIGN_TOP_MID, 107, 115);
    
    // Bar charts at Y=140px, 100px tall
    // NOTE: Hardware has 20px Y offset (see lcd_driver.cpp lcd_set_window)
    // Logical: Y=140 to Y=240 (40px from 280px screen)
    // Actual on panel: Y=160 to Y=260 (20px from bottom edge - safe above rounded corners)
    // Solar bar
    solar_bar = lv_bar_create(background);
    lv_obj_set_size(solar_bar, 12, 100);
    lv_obj_align(solar_bar, LV_ALIGN_TOP_MID, -107, 140);
    lv_bar_set_range(solar_bar, 0, 3000);  // 0-3kW in watts
    lv_bar_set_value(solar_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(solar_bar, lv_color_hex(0x333333), LV_PART_MAIN);  // Dark gray background
    lv_obj_set_style_bg_opa(solar_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(solar_bar, COLOR_WHITE, LV_PART_INDICATOR);  // Bar color (will be updated)
    lv_obj_set_style_bg_opa(solar_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    
    // Home bar
    home_bar = lv_bar_create(background);
    lv_obj_set_size(home_bar, 12, 100);
    lv_obj_align(home_bar, LV_ALIGN_TOP_MID, 0, 140);
    lv_bar_set_range(home_bar, 0, 3000);
    lv_bar_set_value(home_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(home_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(home_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(home_bar, COLOR_WHITE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(home_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    
    // Grid bar
    grid_bar = lv_bar_create(background);
    lv_obj_set_size(grid_bar, 12, 100);
    lv_obj_align(grid_bar, LV_ALIGN_TOP_MID, 107, 140);
    lv_bar_set_range(grid_bar, 0, 3000);
    lv_bar_set_value(grid_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(grid_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(grid_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(grid_bar, COLOR_WHITE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(grid_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    
    // Box plot overlays (Q1-Q3 boxes + median/min/max lines)
    // White lines for min/max overlays
    lv_color_t line_color = lv_color_hex(0xFFFFFF);
    
    // Helper: create min/max line
    auto create_line = [&]() -> lv_obj_t* {
        lv_obj_t* line = lv_line_create(background);
        lv_obj_set_style_line_width(line, 1, 0);
        lv_obj_set_style_line_color(line, line_color, 0);
        lv_obj_set_style_line_opa(line, LV_OPA_70, 0);  // 70% opacity for subtle overlay
        lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
        return line;
    };
    
    // Solar min/max lines
    solar_line_min = create_line();
    solar_line_max = create_line();
    
    // Home min/max lines
    home_line_min = create_line();
    home_line_max = create_line();
    
    // Grid min/max lines
    grid_line_min = create_line();
    grid_line_max = create_line();
}

void PowerScreen::destroy() {
    if (screen_obj) {
        lv_obj_del(screen_obj);
        screen_obj = nullptr;
        background = nullptr;
        solar_icon = home_icon = grid_icon = nullptr;
        arrow1 = arrow2 = nullptr;
        solar_value = home_value = grid_value = nullptr;
        solar_unit = home_unit = grid_unit = nullptr;
        solar_bar = home_bar = grid_bar = nullptr;
        solar_line_min = solar_line_max = nullptr;
        home_line_min = home_line_max = nullptr;
        grid_line_min = grid_line_max = nullptr;
    }
    visible = false;
}

void PowerScreen::update() {
    // Static display, no animations
}

void PowerScreen::show() {
    if (screen_obj) {
        lv_scr_load(screen_obj);
        visible = true;
    }
}

void PowerScreen::hide() {
    visible = false;
}

void PowerScreen::set_solar_power(float kw) {
    solar_kw = kw;
    
    if (solar_value) {
        if (isnan(kw)) {
            lv_label_set_text(solar_value, "--");
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", kw);
            lv_label_set_text(solar_value, buf);
        }
    }
    
    // Hide/show arrow1 based on solar power
    if (arrow1) {
        if (!isnan(kw) && kw >= 0.01) {
            lv_obj_clear_flag(arrow1, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(arrow1, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    update_home_value();
    update_power_colors();
    update_statistics();
}

void PowerScreen::set_grid_power(float kw) {
    grid_kw = kw;
    
    if (grid_value) {
        if (isnan(kw)) {
            lv_label_set_text(grid_value, "--");
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", kw);
            lv_label_set_text(grid_value, buf);
        }
    }
    
    // Update arrow direction based on grid power
    if (arrow2) {
        if (!isnan(kw) && kw > 0) {
            lv_label_set_text(arrow2, LV_SYMBOL_LEFT);  // Drawing from grid (import)
        } else {
            lv_label_set_text(arrow2, LV_SYMBOL_RIGHT);  // Sending to grid (export)
        }
    }
    
    update_home_value();
    update_power_colors();
    update_statistics();
}

void PowerScreen::update_home_value() {
    if (!home_value) return;
    
    // Calculate home consumption: home = grid + solar
    if (!isnan(solar_kw) && !isnan(grid_kw)) {
        float home_kw = grid_kw + solar_kw;
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", home_kw);
        lv_label_set_text(home_value, buf);
    } else {
        lv_label_set_text(home_value, "--");
    }
    update_power_colors();
}

void PowerScreen::update_bar_charts() {
    // Update solar bar (0-3kW range, show absolute value)
    if (solar_bar) {
        if (!isnan(solar_kw)) {
            int32_t value = (int32_t)(fabs(solar_kw) * 1000.0f);  // Convert to watts
            if (value > 3000) value = 3000;  // Cap at max
            lv_bar_set_value(solar_bar, value, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(solar_bar, get_solar_color(solar_kw), LV_PART_INDICATOR);
        } else {
            lv_bar_set_value(solar_bar, 0, LV_ANIM_OFF);
        }
    }
    
    // Update home bar (0-3kW range, show absolute value)
    if (home_bar) {
        if (!isnan(solar_kw) && !isnan(grid_kw)) {
            float home_kw = grid_kw + solar_kw;
            int32_t value = (int32_t)(fabs(home_kw) * 1000.0f);
            if (value > 3000) value = 3000;
            lv_bar_set_value(home_bar, value, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(home_bar, get_home_color(home_kw), LV_PART_INDICATOR);
        } else {
            lv_bar_set_value(home_bar, 0, LV_ANIM_OFF);
        }
    }
    
    // Update grid bar (0-3kW range, show absolute value)
    if (grid_bar) {
        if (!isnan(grid_kw)) {
            int32_t value = (int32_t)(fabs(grid_kw) * 1000.0f);
            if (value > 3000) value = 3000;
            lv_bar_set_value(grid_bar, value, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(grid_bar, get_grid_color(grid_kw), LV_PART_INDICATOR);
        } else {
            lv_bar_set_value(grid_bar, 0, LV_ANIM_OFF);
        }
    }
}

void PowerScreen::update_statistics() {
    unsigned long current_time = millis();
    
    // Sample on every display update (1 second) to catch all MQTT updates
    // MQTT updates arrive at max 2× per second (500ms), display updates every 1000ms
    // This ensures we never miss significant min/max values
    
    // Add samples to statistics buffers
    if (solar_stats) {
        PowerStatistics* stats = static_cast<PowerStatistics*>(solar_stats);
        stats->addSample(solar_kw);
    }
    
    if (grid_stats) {
        PowerStatistics* stats = static_cast<PowerStatistics*>(grid_stats);
        stats->addSample(grid_kw);
    }
    
    // Calculate home consumption and add to home stats
    float home_kw = NAN;
    if (!isnan(solar_kw) && !isnan(grid_kw)) {
        home_kw = solar_kw + grid_kw;
    }
    if (home_stats) {
        PowerStatistics* stats = static_cast<PowerStatistics*>(home_stats);
        stats->addSample(home_kw);
    }
    
    // Update overlay lines
    update_stat_overlays();
}

void PowerScreen::update_stat_overlays() {
    // Bar positioning constants
    const float bar_max_kw = 3.0f;
    const int32_t bar_y = 140;
    const int32_t bar_height = 100;
    const int32_t bar_width = 12;
    const int32_t screen_center_x = 120;
    
    // Helper: Calculate Y position from kW value (inverted, 0=bottom)
    auto kw_to_y = [&](float kw) -> int32_t {
        float ratio = kw / bar_max_kw;
        if (ratio > 1.0f) ratio = 1.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        return bar_y + bar_height - (int32_t)(ratio * bar_height);
    };
    
    // Helper: Position a horizontal line across the top of the bar
    auto position_line = [&](lv_obj_t* line, lv_point_t* points, int32_t bar_x_offset, float value_kw, const char* label) {
        if (!line || isnan(value_kw)) {
            if (line) lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        
        int32_t y_pos = kw_to_y(value_kw);
        
        /* LVGL line positioning: Points are RELATIVE to line object position.
         * We must position the line object itself, then use local coordinates.
         * Match the bar positioning exactly: LV_ALIGN_TOP_MID with bar_x_offset.
         */
        
        // Position line object exactly like the bar (same alignment), shifted 1px right
        lv_obj_align(line, LV_ALIGN_TOP_MID, bar_x_offset + 1, y_pos);
        
        // Use local coordinates - span bar width from center
        // Extend: 2px left + 5px right for proper coverage
        points[0].x = -(bar_width/2 + 2);
        points[0].y = 0;
        points[1].x = bar_width/2 + 5;
        points[1].y = 0;
        
        lv_line_set_points(line, points, 2);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
    };
    
    // Update solar min/max (x=-107)
    if (solar_stats) {
        PowerStatistics* stats = static_cast<PowerStatistics*>(solar_stats);
        if (stats->hasData()) {
            position_line(solar_line_min, solar_min_points, -107, stats->getMin(), "Solar MIN");
            position_line(solar_line_max, solar_max_points, -107, stats->getMax(), "Solar MAX");
        }
    }
    
    // Update home min/max (x=0)
    if (home_stats) {
        PowerStatistics* stats = static_cast<PowerStatistics*>(home_stats);
        if (stats->hasData()) {
            position_line(home_line_min, home_min_points, 0, stats->getMin(), "Home MIN");
            position_line(home_line_max, home_max_points, 0, stats->getMax(), "Home MAX");
        }
    }
    
    // Update grid min/max (x=107)
    if (grid_stats) {
        PowerStatistics* stats = static_cast<PowerStatistics*>(grid_stats);
        if (stats->hasData()) {
            position_line(grid_line_min, grid_min_points, 107, stats->getMin(), "Grid MIN");
            position_line(grid_line_max, grid_max_points, 107, stats->getMax(), "Grid MAX");
        }
    }
}
