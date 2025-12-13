#include "display_manager.h"
#include "lcd_driver.h"
#include "screen_splash.h"
#include "screen_power.h"
#include <math.h>

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LCD_WIDTH * 20];  // Larger buffer for faster rendering
static lv_color_t buf2[LCD_WIDTH * 20];  // Second buffer for double buffering
static lv_disp_drv_t disp_drv;

// Screen instances
static SplashScreen* splash_screen = nullptr;
static PowerScreen* power_screen = nullptr;
static ScreenBase* current_screen = nullptr;

// FPS counter
static lv_obj_t* fps_label = nullptr;
static unsigned long fps_last_time = 0;
static int fps_frame_count = 0;
static float fps_current = 0.0f;

// RGB565 to BGR565 color swap for anti-aliasing fix
// LVGL blends colors in RGB space, but ST7789V2 display expects BGR
// Swapping here fixes color artifacts on anti-aliased text edges
static inline void rgb565_to_bgr565(uint16_t *pixels, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint16_t color = pixels[i];
        // Extract RGB components from RGB565
        uint16_t r = (color >> 11) & 0x1F;  // 5 bits red
        uint16_t g = (color >> 5) & 0x3F;   // 6 bits green
        uint16_t b = color & 0x1F;          // 5 bits blue
        // Recombine as BGR565
        pixels[i] = (b << 11) | (g << 5) | r;
    }
}

static void display_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    const uint32_t pixel_count = w * h;

    // Swap RGB to BGR for display hardware
    rgb565_to_bgr565((uint16_t *)color_p, pixel_count);

    lcd_set_window(area->x1, area->y1, area->x2, area->y2);
    lcd_push_colors((uint16_t *)color_p, pixel_count);

    lv_disp_flush_ready(disp);
}

void display_init() {
    lcd_init();

    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_WIDTH * 20);  // Double buffering

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 0;
    
#if LCD_ROTATION == 1
    disp_drv.sw_rotate = 1;  // 90° clockwise
    disp_drv.rotated = LV_DISP_ROT_90;
#elif LCD_ROTATION == 2
    disp_drv.sw_rotate = 1;
    disp_drv.rotated = LV_DISP_ROT_180;
#elif LCD_ROTATION == 3
    disp_drv.sw_rotate = 1;
    disp_drv.rotated = LV_DISP_ROT_270;
#endif
    
    lv_disp_drv_register(&disp_drv);
    
    // Create and show splash screen
    splash_screen = new SplashScreen();
    splash_screen->create();
    splash_screen->show();
    current_screen = splash_screen;
    
    // Create power screen (but don't show yet)
    power_screen = new PowerScreen();
    power_screen->create();
    
    // Create FPS counter label on splash screen (will be recreated on screen changes)
    fps_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(fps_label, lv_color_hex(0x0000FF), 0);  // BGR for red
    lv_obj_set_style_text_font(fps_label, &lv_font_montserrat_20, 0);   // Larger font
    lv_label_set_text(fps_label, "FPS: --");
    lv_obj_align(fps_label, LV_ALIGN_BOTTOM_RIGHT, -15, -5);  // 10px from corner
    fps_last_time = millis();
}

void display_update() {
    lv_timer_handler();
    display_update_fps();  // Update FPS counter
    
    if (current_screen) {
        current_screen->update();
    }
}

void display_set_boot_progress(int percent, const char* status) {
    if (splash_screen) {
        splash_screen->set_progress(percent);
        splash_screen->set_status(status);
        
        // Force a few LVGL updates to render immediately
        for (int i = 0; i < 3; i++) {
            lv_timer_handler();
            display_update_fps();  // Count FPS during boot
            delay(5);
        }
    }
}

void display_show_power_screen() {
    if (power_screen && current_screen != power_screen) {
        power_screen->show();
        current_screen = power_screen;
        
        // Recreate FPS counter label on the new screen
        if (fps_label) {
            lv_obj_del(fps_label);  // Delete from previous screen
        }
        fps_label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_color(fps_label, lv_color_hex(0x0000FF), 0);  // BGR for red
        lv_obj_set_style_text_font(fps_label, &lv_font_montserrat_20, 0);   // Larger font
        lv_label_set_text(fps_label, "FPS: --");
        lv_obj_align(fps_label, LV_ALIGN_BOTTOM_RIGHT, -15, -5);  // 10px from corner
        
        // Force update
        lv_timer_handler();
    }
}

void display_update_energy(float solar_kw, float grid_kw) {
    if (power_screen) {
        power_screen->set_solar_power(solar_kw);
        power_screen->set_grid_power(grid_kw);
    }
}

void display_update_fps() {
    fps_frame_count++;
    
    unsigned long current_time = millis();
    unsigned long elapsed = current_time - fps_last_time;
    
    // Update FPS counter every 100ms (1/10th second)
    if (elapsed >= 100) {
        fps_current = (fps_frame_count * 1000.0f) / elapsed;
        
        char fps_text[16];
        snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", fps_current);
        
        // Update display label if it exists
        if (fps_label) {
            lv_label_set_text(fps_label, fps_text);
        }
        
        fps_frame_count = 0;
        fps_last_time = current_time;
    }
}
