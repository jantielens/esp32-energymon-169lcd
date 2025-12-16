# Image API Module (Copy/Paste Guide)

This project’s Image Display API is intentionally implemented as a small, **copyable module**.

The goal is to make it easy to port the endpoints into another ESP32 + Arduino project without pulling in the full web portal.

## What You Get

- `POST /api/display/image?timeout=<seconds>`
  - Accepts a **single full-frame JPEG** via `multipart/form-data` field name `image`
  - Performs JPEG “preflight” checks (baseline-only, supported chroma sampling, expected dimensions)
  - **Queues** the decode/display work for the main loop (`image_api_process_pending`) so the upload handler can return quickly

- `POST /api/display/image/strips?...` (memory-efficient)
  - Accepts a **single JPEG strip** per request via `application/octet-stream`
  - Decodes **synchronously** inside the request handler (by design)

- `DELETE /api/display/image`
  - Dismisses the current image

## Files To Copy

Copy these files as-is:

- `src/app/image_api.h`
- `src/app/image_api.cpp`
- `src/app/jpeg_preflight.h`
- `src/app/jpeg_preflight.cpp`

## Dependencies

Required:

- `ESPAsyncWebServer` (for `AsyncWebServer`, handlers, uploads)
- Arduino core (`Arduino.h`)

The module is designed to avoid dependencies on this repo’s web portal state/config.

## Integration Checklist

### 1) Provide a backend adapter

The module calls three project-specific hooks via `ImageApiBackend`:

- `hide_current_image()`
- `start_strip_session(width, height, timeout_ms, start_time_ms)`
- `decode_strip(jpeg_data, jpeg_size, strip_index, output_bgr565)`

These are intentionally minimal so you can connect them to your own display stack.

### 2) Configure the module

You must provide `ImageApiConfig`:

- `lcd_width`, `lcd_height` (expected JPEG dimensions for `/api/display/image`)
- `max_image_size_bytes` (upload limit)
- `decode_headroom_bytes` (heap headroom required before accepting an upload)
- `default_timeout_ms`, `max_timeout_ms`

### 3) Initialize and register routes

Call this once after your `AsyncWebServer` exists:

```cpp
ImageApiBackend backend;
backend.hide_current_image = []() {
    // Hide any currently shown image (implementation-specific)
};
backend.start_strip_session = [](int width, int height, unsigned long timeout_ms, unsigned long start_time_ms) -> bool {
    // Prepare display for strip rendering
    return true;
};
backend.decode_strip = [](const uint8_t* jpeg_data, size_t jpeg_size, uint8_t strip_index, bool output_bgr565) -> bool {
    // Decode a strip and blit it to the correct destination
    return true;
};

ImageApiConfig cfg;
cfg.lcd_width = 240;
cfg.lcd_height = 280;
cfg.max_image_size_bytes = 100 * 1024;
cfg.decode_headroom_bytes = 50 * 1024;
cfg.default_timeout_ms = 10 * 1000;
cfg.max_timeout_ms = 24UL * 60UL * 60UL * 1000UL;

image_api_init(cfg, backend);
image_api_register_routes(server);
```

### 4) Call `image_api_process_pending(...)` from your main loop

The `/api/display/image` endpoint queues work that must be processed later:

```cpp
void loop() {
    // ... your normal loop ...

    // If your project has an OTA flow, pass `true` while OTA is active to pause image decoding.
    image_api_process_pending(false);
}
```

## Notes / Porting Tips

- `/api/display/image` expects **exact panel dimensions** (whatever you configured via `lcd_width/lcd_height`).
- The JPEG preflight rejects progressive JPEGs and uncommon sampling factors to match TJpgDec/tjpgd constraints.
- `/api/display/image/strips` decodes synchronously by design (good for deterministic client pipelines).
- The module does not implement authentication or rate limiting.
