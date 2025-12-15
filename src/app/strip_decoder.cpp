/*
 * Strip Decoder Implementation
 * 
 * Decodes JPEG strips using TJpgDec and writes directly to LCD.
 * Performs RGB→BGR color swap during pixel write.
 */

#include "strip_decoder.h"
#include "lcd_driver.h"
#include "log_manager.h"
#include "board_config.h"
#include <lvgl.h>

// TJpgDec is compiled into LVGL's SJPG library
// Declare the functions and types we need from tjpgd
extern "C" {
    // TJpgDec types and constants
    typedef struct {
        uint16_t left, right, top, bottom;
    } JRECT;
    
    typedef struct JDEC JDEC;
    struct JDEC {
        size_t dctr;                // Number of bytes available in the input buffer
        uint8_t* dptr;              // Current data read ptr
        uint8_t* inbuf;             // Bit stream input buffer
        uint8_t dmsk;               // Current bit in the current read byte
        uint8_t scale;              // Output scaling ratio
        uint8_t msx, msy;           // MCU size in unit of block (width, height)
        uint8_t qtid[3];            // Quantization table ID of each component
        int16_t dcv[3];             // Previous DC element of each component
        uint16_t nrst;              // Restart interval
        uint16_t width, height;     // Size of the input image (pixel)
        uint8_t* huffbits[2][2];    // Huffman bit distribution tables [id][dcac]
        uint16_t* huffcode[2][2];   // Huffman code word tables [id][dcac]
        uint8_t* huffdata[2][2];    // Huffman decoded data tables [id][dcac]
        int32_t* qttbl[4];          // Dequantizer tables [id]
        void* workbuf;              // Working buffer for IDCT and RGB output
        uint8_t* mcubuf;            // Working buffer for the MCU
        void* pool;                 // Pointer to available memory pool
        size_t sz_pool;             // Size of momory pool (bytes available)
        size_t (*infunc)(JDEC*, uint8_t*, size_t);  // Pointer to jpeg stream input function
        void* device;               // Pointer to I/O device identifiler for the session
    };
    
    typedef enum {
        JDR_OK = 0, // 0: Succeeded
        JDR_INTR,   // 1: Interrupted by output function
        JDR_INP,    // 2: Device error or wrong termination of input stream
        JDR_MEM1,   // 3: Insufficient memory pool for the image
        JDR_MEM2,   // 4: Insufficient stream input buffer
        JDR_PAR,    // 5: Parameter error
        JDR_FMT1,   // 6: Data format error (may be damaged data)
        JDR_FMT2,   // 7: Right format but not supported
        JDR_FMT3    // 8: Not supported JPEG standard
    } JRESULT;
    
    // TJpgDec functions
    JRESULT jd_prepare(JDEC* jd, size_t (*infunc)(JDEC*, uint8_t*, size_t), void* pool, size_t sz_pool, void* dev);
    JRESULT jd_decomp(JDEC* jd, int (*outfunc)(JDEC*, void*, JRECT*), uint8_t scale);
}

// Input buffer context for TJpgDec
struct JpegInputContext {
    const uint8_t* data;
    size_t size;
    size_t pos;
};

// Output context for TJpgDec
struct JpegOutputContext {
    StripDecoder* decoder;
    int strip_y_offset;
    uint16_t* line_buffer;  // Buffer for one line of pixels
    int buffer_width;
};

// TJpgDec uses a single opaque device pointer for the entire decode session.
// Both the input function and output function must be able to access their
// respective state through the same pointer.
struct JpegSessionContext {
    JpegInputContext input;
    JpegOutputContext output;
};

// TJpgDec input function - read from memory buffer
// Signature must match jd_prepare: size_t (*)(JDEC*, uint8_t*, size_t)
static size_t jpeg_input_func(JDEC* jd, uint8_t* buff, size_t nbyte) {
    JpegSessionContext* session = (JpegSessionContext*)jd->device;
    if (!session) return 0;

    JpegInputContext* ctx = &session->input;
    if (!ctx->data) return 0;
    if (ctx->pos >= ctx->size) return 0;

    const size_t remaining = ctx->size - ctx->pos;
    const size_t to_read = (nbyte < remaining) ? nbyte : remaining;

    if (buff && to_read > 0) {
        memcpy(buff, ctx->data + ctx->pos, to_read);
    }

    ctx->pos += to_read;
    return to_read;
}

// TJpgDec output function - convert RGB888→BGR565 and write to LCD
static int jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegSessionContext* session = (JpegSessionContext*)jd->device;
    JpegOutputContext* ctx = session ? &session->output : nullptr;
    uint8_t* src = (uint8_t*)bitmap;
    
    if (!ctx || !ctx->line_buffer) {
        Logger.logMessage("StripDecoder", "ERROR: Invalid context or line_buffer");
        return 0;
    }
    
    // Process each line in the MCU block
    for (int y = rect->top; y <= rect->bottom; y++) {
        int line_width = rect->right - rect->left + 1;
        
        // Bounds check
        if (line_width > ctx->buffer_width) {
            Logger.logMessagef("StripDecoder", "ERROR: line_width %d > buffer_width %d", line_width, ctx->buffer_width);
            return 0;
        }
        
        // Convert RGB888 to BGR565 for this line
        for (int x = 0; x < line_width; x++) {
            uint8_t r = *src++;
            uint8_t g = *src++;
            uint8_t b = *src++;
            
            // RGB888 → BGR565 conversion
            // BGR565: BBBB BGGG GGGR RRRR
            ctx->line_buffer[x] = ((b & 0xF8) << 8) |   // Blue in high bits
                                  ((g & 0xFC) << 3) |   // Green in middle
                                  (r >> 3);              // Red in low bits
        }
        
        // Write line to LCD at correct position
        int lcd_x = rect->left;
        int lcd_y = ctx->strip_y_offset + y;
        
        // Bounds check for LCD coordinates
        if (lcd_x < 0 || lcd_y < 0 || lcd_x + line_width > LCD_WIDTH || lcd_y >= LCD_HEIGHT) {
            Logger.logMessagef("StripDecoder", "ERROR: Invalid LCD coords: x=%d y=%d w=%d (LCD: %dx%d)", 
                              lcd_x, lcd_y, line_width, LCD_WIDTH, LCD_HEIGHT);
            return 0;
        }
        
        lcd_push_pixels_at(lcd_x, lcd_y, line_width, 1, ctx->line_buffer);
    }
    
    return 1;  // Continue decoding
}

StripDecoder::StripDecoder() : width(0), height(0), current_y(0) {
}

StripDecoder::~StripDecoder() {
    end();
}

void StripDecoder::begin(int image_width, int image_height) {
    width = image_width;
    height = image_height;
    current_y = 0;
    
    Logger.logMessagef("StripDecoder", "Begin decode: %dx%d image", width, height);
}

bool StripDecoder::decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index) {
    Logger.logBegin("Strip");
    Logger.logLinef("Strip %d: Y=%d, Size=%u, Heap=%u", 
                   strip_index, current_y, jpeg_size, ESP.getFreeHeap());
    
    // Allocate TJpgDec work area
    // TJpgDec requires: 3100 + (width * height * 2 / MCU_size) bytes
    // For 240x16: minimum ~3220 bytes, using 4096 for safety
    const size_t work_size = 4096;
    void* work = malloc(work_size);
    if (!work) {
        Logger.logEnd("ERROR: Failed to allocate work buffer");
        return false;
    }
    Logger.logLinef("Work buffer allocated: %p (%d bytes)", work, work_size);
    
    // Allocate line buffer for pixel conversion
    uint16_t* line_buffer = (uint16_t*)malloc(width * sizeof(uint16_t));
    if (!line_buffer) {
        free(work);
        Logger.logEnd("ERROR: Failed to allocate line buffer");
        return false;
    }
    Logger.logLinef("Line buffer allocated: %p (%d bytes)", line_buffer, width * sizeof(uint16_t));
    
    JDEC jdec;
    JRESULT res;
    
    // Setup session context (shared between input and output callbacks)
    JpegSessionContext session_ctx;
    session_ctx.input.data = jpeg_data;
    session_ctx.input.size = jpeg_size;
    session_ctx.input.pos = 0;
    Logger.logLinef("Input context: data=%p size=%u", jpeg_data, jpeg_size);

    session_ctx.output.decoder = this;
    session_ctx.output.strip_y_offset = current_y;
    session_ctx.output.line_buffer = line_buffer;
    session_ctx.output.buffer_width = width;
    Logger.logLinef("Output context: y_offset=%d buffer=%p width=%d",
                   current_y, line_buffer, width);
    
    // Prepare decoder
    Logger.logMessage("StripDecoder", "Calling jd_prepare...");
    res = jd_prepare(&jdec, jpeg_input_func, work, work_size, &session_ctx);
    
    if (res != JDR_OK) {
        Logger.logLinef("ERROR: jd_prepare failed: %d", res);
        Logger.logEnd();
        free(line_buffer);
        free(work);
        return false;
    }
    
    Logger.logLinef("JPEG: %dx%d", jdec.width, jdec.height);
    
    // Decompress and output to LCD
    Logger.logMessage("StripDecoder", "Calling jd_decomp...");
    res = jd_decomp(&jdec, jpeg_output_func, 0);  // 0 = 1:1 scale
    
    if (res != JDR_OK) {
        Logger.logLinef("ERROR: jd_decomp failed: %d", res);
        Logger.logEnd();
        free(line_buffer);
        free(work);
        return false;
    }
    
    // Move Y position for next strip
    current_y += jdec.height;
    
    Logger.logLinef("✓ Decoded, new Y: %d", current_y);
    Logger.logEnd();
    
    // Cleanup
    free(line_buffer);
    free(work);
    
    return true;
}

void StripDecoder::end() {
    Logger.logMessagef("StripDecoder", "Complete at Y=%d", current_y);
    current_y = 0;
    width = 0;
    height = 0;
}
