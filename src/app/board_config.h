#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ============================================================================
// Board Configuration - Two-Phase Include Pattern
// ============================================================================
// This file provides default configuration for all boards using a two-phase
// include pattern:
//
// Phase 1: Load board-specific overrides first (if they exist)
// Phase 2: Define defaults using #ifndef guards (only if not already defined)
//
// To customize for a specific board, create: src/boards/[board-name]/board_overrides.h
// The build system will automatically detect and include board-specific overrides.
//
// Example board-specific override:
//   src/boards/esp32c3/board_overrides.h

// ============================================================================
// Phase 1: Load Board-Specific Overrides
// ============================================================================
// build.sh defines BOARD_HAS_OVERRIDE when a board override directory exists
// and adds that directory to the include path. Board-specific settings are
// loaded first so they can override the defaults below.

#ifdef BOARD_HAS_OVERRIDE
#include "board_overrides.h"
#endif

// ============================================================================
// Phase 2: Default Hardware Capabilities
// ============================================================================
// These defaults are only applied if not already defined by board overrides.

// Built-in LED
#ifndef HAS_BUILTIN_LED
#define HAS_BUILTIN_LED false
#endif

#ifndef LED_PIN
#define LED_PIN 2  // Common GPIO for ESP32 boards
#endif

#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH true  // true = HIGH turns LED on, false = LOW turns LED on
#endif

// ============================================================================
// Default WiFi Configuration
// ============================================================================

#ifndef WIFI_MAX_ATTEMPTS
#define WIFI_MAX_ATTEMPTS 3
#endif

// ============================================================================
// Additional Default Configuration Settings
// ============================================================================
// Add new hardware features here using #ifndef guards to allow board-specific
// overrides.
//
// Usage Pattern in Application Code:
//   1. Define capabilities in board_overrides.h: #define HAS_BUTTON true
//   2. Use conditional compilation in app.ino:
//
//      #if HAS_BUTTON
//        pinMode(BUTTON_PIN, INPUT_PULLUP);
//        // Button-specific code only compiled when HAS_BUTTON is true
//      #endif
//
// Examples:
//
// Button:
// #ifndef HAS_BUTTON
// #define HAS_BUTTON false
// #endif
//
// #ifndef BUTTON_PIN
// #define BUTTON_PIN 0
// #endif
//
// Battery Monitor:
// #ifndef HAS_BATTERY_MONITOR
// #define HAS_BATTERY_MONITOR false
// #endif
//
// #ifndef BATTERY_ADC_PIN
// #define BATTERY_ADC_PIN 34
// #endif
//
// Display:
// Display: 1.69" LCD (ST7789V2)
#ifndef HAS_DISPLAY
#define HAS_DISPLAY true
#endif

#ifndef LCD_WIDTH
#define LCD_WIDTH 240
#endif

#ifndef LCD_HEIGHT
#define LCD_HEIGHT 280
#endif

// Display orientation
#ifndef LCD_ROTATION
#define LCD_ROTATION 0  // 0=portrait, 1=landscape (90°), 2=portrait (180°), 3=landscape (270°)
#endif

#ifndef LCD_CS_PIN
#define LCD_CS_PIN 5      // Chip Select
#endif

#ifndef LCD_DC_PIN
#define LCD_DC_PIN 16     // Data/Command
#endif

#ifndef LCD_RST_PIN
#define LCD_RST_PIN 17    // Reset
#endif

#ifndef LCD_BL_PIN
#define LCD_BL_PIN 4      // Backlight (PWM)
#endif

#ifndef LCD_MOSI_PIN
#define LCD_MOSI_PIN 23   // SPI MOSI
#endif

#ifndef LCD_SCK_PIN
#define LCD_SCK_PIN 18    // SPI Clock
#endif

#endif // BOARD_CONFIG_H

