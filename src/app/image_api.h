#pragma once

#include <stddef.h>
#include <stdint.h>

class AsyncWebServer;

// Small adapter layer for porting this feature to other projects:
// provide these hooks to connect the HTTP upload endpoints to your display pipeline.
struct ImageApiBackend {
    void (*hide_current_image)();
    bool (*start_strip_session)(int width, int height, unsigned long timeout_ms, unsigned long start_time);
    bool (*decode_strip)(const uint8_t* jpeg_data, size_t jpeg_size, uint8_t strip_index, bool output_bgr565);
};

struct ImageApiConfig {
    int lcd_width = 0;
    int lcd_height = 0;
    size_t max_image_size_bytes = 100 * 1024;
    size_t decode_headroom_bytes = 50 * 1024;  // extra heap headroom beyond upload size
    unsigned long default_timeout_ms = 10000;
    unsigned long max_timeout_ms = 86400UL * 1000UL;
};

void image_api_init(const ImageApiConfig& cfg, const ImageApiBackend& backend);
void image_api_register_routes(AsyncWebServer* server);

// Call from the main loop. Keeps /api/display/image deferred behavior.
void image_api_process_pending(bool ota_in_progress);
