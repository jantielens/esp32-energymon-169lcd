/*
 * Image Screen
 * 
 * Displays uploaded JPEG images with configurable timeout before returning to power screen.
 * Default timeout: 10 seconds (configurable via query parameter)
 * Images are stored in RAM only and lost on reboot.
 */

#ifndef SCREEN_IMAGE_H
#define SCREEN_IMAGE_H

#include "screen_base.h"

class ImageScreen : public ScreenBase {
public:
    ImageScreen();
    ~ImageScreen();
    
    void create() override;
    void destroy() override;
    void update() override;
    void show() override;
    void hide() override;
    
    // Load JPEG image from memory buffer
    bool load_image(const uint8_t* jpeg_data, size_t jpeg_size);
    
    // Clear current image
    void clear_image();
    
    // Set display timeout in milliseconds (default: 10 seconds)
    void set_timeout(unsigned long timeout_ms);
    
    // Set start time (for accurate timeout when decoding takes time)
    void set_start_time(unsigned long start_time);
    
    // Check if timeout expired
    bool is_timeout_expired();
    
private:
    lv_obj_t* img_obj = nullptr;
    
    // Image data buffer (allocated on heap)
    uint8_t* image_buffer = nullptr;
    size_t buffer_size = 0;
    
    // Timeout tracking
    unsigned long display_start_time = 0;
    unsigned long display_timeout_ms = 10000;  // Default 10 seconds, configurable
};

#endif // SCREEN_IMAGE_H
