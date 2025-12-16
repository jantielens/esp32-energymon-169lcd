/*
 * Direct Image Screen
 * 
 * Blank black screen for strip-by-strip image display.
 * Strips are decoded and written directly to LCD hardware (bypassing LVGL).
 * Uses visibility pattern to prevent PowerScreen from rendering during display.
 * 
 * Default timeout: 10 seconds (configurable via query parameter)
 */

#ifndef SCREEN_DIRECT_IMAGE_H
#define SCREEN_DIRECT_IMAGE_H

#include "screen_base.h"
#include "strip_decoder.h"

class DirectImageScreen : public ScreenBase {
public:
    DirectImageScreen();
    ~DirectImageScreen();
    
    void create() override;
    void destroy() override;
    void update() override;
    void show() override;
    void hide() override;
    
    // Start strip upload session
    // width: image width in pixels
    // height: image height in pixels
    void begin_strip_session(int width, int height);
    
    // Decode and display a single strip
    // Returns: true on success, false on failure
    bool decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index, bool output_bgr565 = true);
    
    // End strip upload session
    void end_strip_session();
    
    // Set display timeout in milliseconds (default: 10 seconds)
    void set_timeout(unsigned long timeout_ms);
    
    // Set start time (for accurate timeout when upload completes)
    void set_start_time(unsigned long start_time);
    
    // Check if timeout expired
    bool is_timeout_expired();
    
    // Get strip decoder for progress tracking
    StripDecoder* get_decoder() { return &decoder; }
    
private:
    StripDecoder decoder;
    
    // Timeout tracking
    unsigned long display_start_time = 0;
    unsigned long display_timeout_ms = 10000;  // Default 10 seconds, configurable
    
    // Session state
    bool session_active = false;
};

#endif // SCREEN_DIRECT_IMAGE_H
