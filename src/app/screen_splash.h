/*
 * Splash Screen
 * 
 * Displays boot progress with spinner and status messages.
 * Shows PROJECT_DISPLAY_NAME and current boot stage.
 */

#ifndef SCREEN_SPLASH_H
#define SCREEN_SPLASH_H

#include "screen_base.h"

class SplashScreen : public ScreenBase {
public:
    SplashScreen();
    ~SplashScreen();
    
    void create() override;
    void destroy() override;
    void update() override;
    void show() override;
    void hide() override;
    
    // Update boot progress (0-100%)
    void set_progress(int percent);
    
    // Update status message
    void set_status(const char* message);

private:
    lv_obj_t* spinner = nullptr;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* progress_label = nullptr;
    
    int current_progress = 0;
};

#endif // SCREEN_SPLASH_H
