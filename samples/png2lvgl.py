#!/usr/bin/env python3
"""
Convert PNG images to LVGL I1 (1-bit monochrome) format C arrays.

Usage:
    python3 png2lvgl.py <input.png> <output.c> [symbol_name]

Example:
    python3 png2lvgl.py ui-assets/empty.png src/assets/power_icons.c power_icons
"""

import sys
import os
from PIL import Image


def png_to_lvgl_i1(input_path, output_path, symbol_name="image"):
    """Convert PNG to LVGL I1 format C array."""
    
    # Read the PNG
    img = Image.open(input_path)
    print(f'Original image: {img.mode} {img.size}')
    
    # Convert to black and white (1-bit)
    img = img.convert('1')
    
    # Get dimensions
    width, height = img.size
    
    # Create output directory if needed
    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)
    
    # Generate C array
    with open(output_path, 'w') as f:
        f.write('#include "lvgl.h"\n\n')
        f.write(f'// {width}x{height} monochrome image\n')
        f.write(f'const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {symbol_name}_map[] = {{\n')
        
        # LVGL I1 format: 1 bit per pixel, packed into bytes
        # Row-major order, MSB first
        pixels = list(img.getdata())
        byte_array = []
        
        for y in range(height):
            for x in range(0, width, 8):
                byte_val = 0
                for bit in range(8):
                    if x + bit < width:
                        pixel_idx = y * width + x + bit
                        # In LVGL I1: 1 = white (background), 0 = black (foreground)
                        if pixels[pixel_idx] != 0:  # PIL: 0 = black, 255 = white
                            byte_val |= (1 << (7 - bit))
                byte_array.append(byte_val)
        
        # Write bytes in rows of 16
        for i in range(0, len(byte_array), 16):
            row = byte_array[i:i+16]
            f.write('  ' + ', '.join(f'0x{b:02x}' for b in row))
            if i + 16 < len(byte_array):
                f.write(',')
            f.write('\n')
        
        f.write('};\n\n')
        
        # Generate image descriptor for LVGL 9
        stride = (width + 7) // 8  # bytes per row for 1-bit format
        
        f.write(f'const lv_image_dsc_t {symbol_name} = {{\n')
        f.write('  .header = {\n')
        f.write('    .magic = LV_IMAGE_HEADER_MAGIC,\n')
        f.write('    .cf = LV_COLOR_FORMAT_I1,\n')
        f.write('    .flags = 0,\n')
        f.write(f'    .w = {width},\n')
        f.write(f'    .h = {height},\n')
        f.write(f'    .stride = {stride},\n')
        f.write('  },\n')
        f.write(f'  .data_size = sizeof({symbol_name}_map),\n')
        f.write(f'  .data = {symbol_name}_map,\n')
        f.write('};\n')
    
    print(f'✓ Generated: {output_path}')
    print(f'  Dimensions: {width}x{height}')
    print(f'  Data size: {len(byte_array)} bytes')
    print(f'  Symbol: {symbol_name}')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    symbol = sys.argv[3] if len(sys.argv) > 3 else 'image'
    
    if not os.path.exists(input_file):
        print(f'Error: Input file not found: {input_file}')
        sys.exit(1)
    
    png_to_lvgl_i1(input_file, output_file, symbol)
