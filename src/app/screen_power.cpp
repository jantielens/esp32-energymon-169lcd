/*
 * Power Screen Implementation
 * 
 * 3-column layout adapted from energy_screen.cpp sample
 */

#include "screen_power.h"
#include <math.h>
#include <stdio.h>

#define TEXT_COLOR lv_color_hex(0xFFFFFF)

PowerScreen::PowerScreen() {
}

PowerScreen::~PowerScreen() {
    destroy();
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
    // Solar icon (refresh) at X=53px
    solar_icon = lv_label_create(background);
    lv_label_set_text(solar_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(solar_icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(solar_icon, TEXT_COLOR, 0);
    lv_obj_align(solar_icon, LV_ALIGN_TOP_MID, -107, 15);
    
    // Arrow 1 (>) at X=107px
    arrow1 = lv_label_create(background);
    lv_label_set_text(arrow1, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow1, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(arrow1, TEXT_COLOR, 0);
    lv_obj_align(arrow1, LV_ALIGN_TOP_MID, -53, 15);
    
    // Home icon at X=160px (center)
    home_icon = lv_label_create(background);
    lv_label_set_text(home_icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(home_icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(home_icon, TEXT_COLOR, 0);
    lv_obj_align(home_icon, LV_ALIGN_TOP_MID, 0, 15);
    
    // Arrow 2 (< or >) at X=213px
    arrow2 = lv_label_create(background);
    lv_label_set_text(arrow2, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow2, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(arrow2, TEXT_COLOR, 0);
    lv_obj_align(arrow2, LV_ALIGN_TOP_MID, 53, 15);
    
    // Grid icon (charge) at X=267px
    grid_icon = lv_label_create(background);
    lv_label_set_text(grid_icon, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_font(grid_icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(grid_icon, TEXT_COLOR, 0);
    lv_obj_align(grid_icon, LV_ALIGN_TOP_MID, 107, 15);
    
    // Values row at Y=90px
    // Solar value
    solar_value = lv_label_create(background);
    lv_label_set_text(solar_value, "--");
    lv_obj_set_style_text_font(solar_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(solar_value, TEXT_COLOR, 0);
    lv_obj_align(solar_value, LV_ALIGN_TOP_MID, -107, 80);
    
    // Home value
    home_value = lv_label_create(background);
    lv_label_set_text(home_value, "--");
    lv_obj_set_style_text_font(home_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(home_value, TEXT_COLOR, 0);
    lv_obj_align(home_value, LV_ALIGN_TOP_MID, 0, 80);
    
    // Grid value
    grid_value = lv_label_create(background);
    lv_label_set_text(grid_value, "--");
    lv_obj_set_style_text_font(grid_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(grid_value, TEXT_COLOR, 0);
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
}
