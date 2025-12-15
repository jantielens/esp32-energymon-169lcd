/*
 * Strip Decoder for Memory-Efficient Image Display
 * 
 * Decodes individual JPEG strips and writes directly to LCD hardware.
 * Uses TJpgDec from LVGL (same decoder used by SJPG format).
 * 
 * Memory usage: ~20KB constant regardless of image size
 *   - Strip buffer: ~2KB (temporary per strip)
 *   - Decode buffer: ~13KB for 280px width × 16px height
 *   - TJpgDec work area: ~3KB
 */

#ifndef STRIP_DECODER_H
#define STRIP_DECODER_H

#include <Arduino.h>

class StripDecoder {
public:
    StripDecoder();
    ~StripDecoder();
    
    // Initialize decoder for new image session
    // image_width: total image width in pixels
    // image_height: total image height in pixels
    void begin(int image_width, int image_height);
    
    // Decode and display a single JPEG strip
    // jpeg_data: pointer to JPEG data for this strip
    // jpeg_size: size of JPEG data in bytes
    // strip_index: index of this strip (0-based)
    // Returns: true on success, false on failure
    bool decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index);
    
    // Complete image session and cleanup
    void end();
    
    // Get current Y position (for progress tracking)
    int get_current_y() const { return current_y; }
    
private:
    int width;       // Image width
    int height;      // Image height
    int current_y;   // Current Y position in image
    
    // Note: Strip height is auto-detected from JPEG during decode (not hardcoded)
    // Typical values: 8, 16, 32, or 64 pixels (configurable in encoder)
    static const int STRIP_HEIGHT = 16;  // Default SJPG strip height (for reference only)
};

#endif // STRIP_DECODER_H
