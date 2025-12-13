/*
 * Base Screen Class
 * 
 * Abstract base class for all display screens.
 * Provides lifecycle management and common interface.
 */

#ifndef SCREEN_BASE_H
#define SCREEN_BASE_H

#include <lvgl.h>

class ScreenBase {
public:
    virtual ~ScreenBase() {}
    
    // Lifecycle methods
    virtual void create() = 0;      // Build UI elements
    virtual void destroy() = 0;     // Clean up UI elements
    virtual void update() = 0;      // Refresh data/animations
    virtual void show() = 0;        // Make screen visible
    virtual void hide() = 0;        // Hide screen
    
    // Check if screen is currently visible
    virtual bool is_visible() const { return visible; }

protected:
    lv_obj_t* screen_obj = nullptr;  // Root screen object
    bool visible = false;             // Visibility state
};

#endif // SCREEN_BASE_H
