# Icon Generation System

## Overview
The project uses custom PNG icons that are automatically converted to LVGL image descriptors during the build process.

## Icon Sources
- **Location**: `assets/icons/*.png`
- **Format**: PNG images (any size, but 48×48px recommended for consistency)
- **Color**: Icons should use transparency (alpha channel) for proper recoloring

## Build Integration
Icons are automatically generated when running `./build.sh`:
1. `tools/png2icons.py` scans `assets/icons/` for PNG files
2. Converts each PNG to LVGL TRUE_COLOR_ALPHA format (RGB565 + Alpha)
3. Generates `src/app/icons.c` and `src/app/icons.h`
4. These files are automatically compiled into the firmware

## Generated Files
- **icons.c**: Contains pixel data arrays and LVGL image descriptors (auto-generated, not committed to git)
- **icons.h**: Header with extern declarations for each icon (auto-generated, not committed to git)

## Usage in Code
```cpp
#include "icons.h"

// Create image widget
lv_obj_t* icon = lv_img_create(parent);

// Set icon source (e.g., for sun.png → sun)
lv_img_set_src(icon, &sun);

// Recolor icon (white icons can be tinted any color)
lv_obj_set_style_img_recolor(icon, lv_color_hex(0xFF6B00), 0);
lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
```

## Current Icons
- **grid.png**: Power grid icon (48×48px, Google Fonts)
- **home.png**: Home/house icon (48×48px, Google Fonts)
- **sun.png**: Solar panel icon (48×48px, Google Fonts)

## Adding New Icons
1. Add PNG file to `assets/icons/`
2. Run `./build.sh` - icon will be automatically generated
3. Use in code via `&icon_name` (filename without .png extension)

## Dependencies
- **Python 3** with **Pillow** library
- Automatically installed during setup: `./setup.sh`
- Manual install: `pip install Pillow`

## Manual Generation
To regenerate icons without a full build:
```bash
python3 tools/png2icons.py assets/icons src/app/icons.c src/app/icons.h
```

## Technical Details
- **Format**: LVGL TRUE_COLOR_ALPHA (3 bytes per pixel: RGB565 + 8-bit alpha)
- **File Extension**: `.c` (not `.cpp`) for proper Arduino build system integration
- **Memory**: LV_ATTRIBUTE_MEM_ALIGN and LV_ATTRIBUTE_IMG_ attributes optimize placement
- **Recoloring**: Icons designed for white (#FFFFFF) base color can be recolored at runtime
