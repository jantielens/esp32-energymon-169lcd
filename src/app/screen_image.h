/*
 * Image Screen
 * 
 * Displays uploaded JPEG images for 10 seconds before returning to power screen.
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
    
    // Check if timeout expired (10 seconds)
    bool is_timeout_expired();
    
private:
    lv_obj_t* img_obj = nullptr;
    
    // Image data buffer (allocated on heap)
    uint8_t* image_buffer = nullptr;
    size_t buffer_size = 0;
    
    // Timeout tracking
    unsigned long display_start_time = 0;
    static const unsigned long DISPLAY_TIMEOUT_MS = 10000;  // 10 seconds
};

#endif // SCREEN_IMAGE_H
