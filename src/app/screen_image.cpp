/*
 * Image Screen Implementation
 */

#include "screen_image.h"
#include "log_manager.h"
#include <Arduino.h>
#include <stdlib.h>

// Virtual file system for SJPG decoder - allows loading from memory
static const uint8_t* vfs_jpeg_data = nullptr;
static size_t vfs_jpeg_size = 0;
static size_t vfs_jpeg_pos = 0;
static volatile bool vfs_busy = false;  // Atomic flag to prevent concurrent access

// LVGL file system callbacks for memory-based JPEG
static void* fs_open_cb(lv_fs_drv_t* drv, const char* path, lv_fs_mode_t mode) {
    if (vfs_jpeg_data && (strcmp(path, "mem.jpg") == 0 || strcmp(path, "mem.sjpg") == 0)) {
        vfs_jpeg_pos = 0;
        return (void*)1;  // Non-null pointer to indicate success
    }
    return nullptr;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t* drv, void* file_p) {
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t* drv, void* file_p, void* buf, uint32_t btr, uint32_t* br) {
    if (!vfs_jpeg_data || vfs_jpeg_pos >= vfs_jpeg_size) {
        *br = 0;
        return LV_FS_RES_OK;
    }
    
    uint32_t remaining = vfs_jpeg_size - vfs_jpeg_pos;
    uint32_t to_read = (btr < remaining) ? btr : remaining;
    
    memcpy(buf, vfs_jpeg_data + vfs_jpeg_pos, to_read);
    vfs_jpeg_pos += to_read;
    *br = to_read;
    
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t* drv, void* file_p, uint32_t pos, lv_fs_whence_t whence) {
    if (whence == LV_FS_SEEK_SET) {
        vfs_jpeg_pos = pos;
    } else if (whence == LV_FS_SEEK_CUR) {
        vfs_jpeg_pos += pos;
    } else if (whence == LV_FS_SEEK_END) {
        vfs_jpeg_pos = vfs_jpeg_size + pos;
    }
    
    if (vfs_jpeg_pos > vfs_jpeg_size) {
        vfs_jpeg_pos = vfs_jpeg_size;
    }
    
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t* drv, void* file_p, uint32_t* pos_p) {
    *pos_p = vfs_jpeg_pos;
    return LV_FS_RES_OK;
}

static bool fs_driver_registered = false;

static void register_fs_driver() {
    if (fs_driver_registered) return;
    
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    drv.letter = 'M';  // Memory driver
    drv.open_cb = fs_open_cb;
    drv.close_cb = fs_close_cb;
    drv.read_cb = fs_read_cb;
    drv.seek_cb = fs_seek_cb;
    drv.tell_cb = fs_tell_cb;
    
    lv_fs_drv_register(&drv);
    fs_driver_registered = true;
}

ImageScreen::ImageScreen() {
    // Constructor - member variables initialized in header
}

ImageScreen::~ImageScreen() {
    destroy();
}

void ImageScreen::create() {
    if (screen_obj) return;  // Already created
    
    Logger.logBegin("ImageScreen");
    
    // Create screen object with solid black background
    screen_obj = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x000000), 0);  // Black background
    lv_obj_set_style_bg_opa(screen_obj, LV_OPA_COVER, 0);  // Fully opaque
    
    // Create image object - no scaling, clip if too large
    img_obj = lv_img_create(screen_obj);
    
    // Disable scrollbars - expect correct resolution images
    lv_obj_clear_flag(screen_obj, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, 0);
    
    Logger.logEnd();
}

void ImageScreen::destroy() {
    Logger.logBegin("ImageScreen Destroy");
    
    // Free image buffer
    clear_image();
    
    // Delete screen
    if (screen_obj) {
        lv_obj_del(screen_obj);
        screen_obj = nullptr;
        img_obj = nullptr;
    }
    
    visible = false;
    
    Logger.logEnd();
}

void ImageScreen::update() {
    // Nothing to update - static image display
}

void ImageScreen::show() {
    if (screen_obj) {
        lv_scr_load(screen_obj);
        visible = true;
        display_start_time = millis();
        Logger.logMessage("ImageScreen", "Displayed (10s timeout started)");
    }
}

void ImageScreen::hide() {
    visible = false;
}

bool ImageScreen::load_image(const uint8_t* jpeg_data, size_t jpeg_size) {
    if (!jpeg_data || jpeg_size == 0) {
        Logger.logMessage("ImageScreen", "ERROR: Invalid image data");
        return false;
    }
    
    // Check if VFS is busy (another operation in progress)
    if (vfs_busy) {
        Logger.logMessage("ImageScreen", "ERROR: VFS busy, cannot load image");
        return false;
    }
    
    Logger.logBegin("Load Image");
    Logger.logLinef("Size: %u bytes", jpeg_size);
    Logger.logLinef("Free heap before: %u bytes", ESP.getFreeHeap());
    
    // Register virtual filesystem driver on first use
    register_fs_driver();
    
    // Clear any existing image
    clear_image();
    
    // Mark VFS as busy
    vfs_busy = true;
    
    // Allocate buffer for image data
    image_buffer = (uint8_t*)malloc(jpeg_size);
    if (!image_buffer) {
        vfs_busy = false;
        Logger.logEnd("ERROR: Out of memory");
        return false;
    }
    
    // Copy image data
    memcpy(image_buffer, jpeg_data, jpeg_size);
    buffer_size = jpeg_size;
    
    // Set up virtual file system pointers
    vfs_jpeg_data = image_buffer;
    vfs_jpeg_size = jpeg_size;
    vfs_jpeg_pos = 0;
    
    // Load image from virtual "file"
    if (img_obj) {
        lv_img_set_src(img_obj, "M:mem.sjpg");
        
        // Get decoded image dimensions for logging
        const void* src = lv_img_get_src(img_obj);
        if (src) {
            lv_img_header_t header;
            lv_res_t res = lv_img_decoder_get_info(src, &header);
            if (res == LV_RES_OK) {
                Logger.logLinef("Image dimensions: %dx%d", header.w, header.h);
            }
        }
        
        // Center the image
        lv_obj_center(img_obj);
    }
    
    Logger.logLinef("Free heap after: %u bytes", ESP.getFreeHeap());
    Logger.logEnd("Image loaded successfully");
    
    // Keep vfs_busy = true while image is displayed
    // Will be cleared in clear_image()
    
    return true;
}

void ImageScreen::clear_image() {
    // Clear virtual filesystem pointers
    vfs_jpeg_data = nullptr;
    vfs_jpeg_size = 0;
    vfs_jpeg_pos = 0;
    vfs_busy = false;  // Release VFS for next operation
    
    if (image_buffer) {
        Logger.logMessagef("ImageScreen", "Freeing image buffer (%u bytes)", buffer_size);
        free(image_buffer);
        image_buffer = nullptr;
        buffer_size = 0;
    }
    
    if (img_obj) {
        lv_img_set_src(img_obj, NULL);
    }
}

bool ImageScreen::is_timeout_expired() {
    if (!visible) return false;
    
    unsigned long elapsed = millis() - display_start_time;
    return elapsed >= DISPLAY_TIMEOUT_MS;
}
