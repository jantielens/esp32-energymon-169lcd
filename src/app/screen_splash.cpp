/*
 * Splash Screen Implementation
 */

#include "screen_splash.h"
#include "web_assets.h"  // For PROJECT_DISPLAY_NAME
#include <stdio.h>

SplashScreen::SplashScreen() {
}

SplashScreen::~SplashScreen() {
    destroy();
}

void SplashScreen::create() {
    if (screen_obj) return;  // Already created
    
    // Create screen object
    screen_obj = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x000000), 0);  // Pure black
    
    // Title (PROJECT_DISPLAY_NAME) at top
    title_label = lv_label_create(screen_obj);
    lv_label_set_text(title_label, PROJECT_DISPLAY_NAME);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);
    
    // Spinner in center
    spinner = lv_spinner_create(screen_obj, 1000, 60);
    lv_obj_set_size(spinner, 60, 60);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x00ADB5), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 6, LV_PART_MAIN);
    
    // Progress percentage below spinner
    progress_label = lv_label_create(screen_obj);
    lv_label_set_text(progress_label, "0%");
    lv_obj_set_style_text_color(progress_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_16, 0);
    lv_obj_align(progress_label, LV_ALIGN_CENTER, 0, 40);
    
    // Status message at bottom
    status_label = lv_label_create(screen_obj);
    lv_label_set_text(status_label, "Initializing...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void SplashScreen::destroy() {
    if (screen_obj) {
        lv_obj_del(screen_obj);
        screen_obj = nullptr;
        spinner = nullptr;
        title_label = nullptr;
        status_label = nullptr;
        progress_label = nullptr;
    }
    visible = false;
}

void SplashScreen::update() {
    // Spinner animates automatically
}

void SplashScreen::show() {
    if (screen_obj) {
        lv_scr_load(screen_obj);
        visible = true;
    }
}

void SplashScreen::hide() {
    visible = false;
}

void SplashScreen::set_progress(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    current_progress = percent;
    
    if (progress_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(progress_label, buf);
    }
}

void SplashScreen::set_status(const char* message) {
    if (status_label && message) {
        lv_label_set_text(status_label, message);
    }
}
