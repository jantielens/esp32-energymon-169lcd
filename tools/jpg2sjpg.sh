#!/bin/bash
# Convert JPG to SJPG format for LVGL with BGR color swap
# Swaps R↔B channels to match ST7789V2 BGR display
# Usage: ./jpg2sjpg.sh <input.jpg> [output.sjpg]

if [ $# -lt 1 ]; then
    echo "Usage: $0 <input.jpg> [output.sjpg]"
    echo ""
    echo "Example: $0 photo.jpg photo.sjpg"
    echo ""
    echo "Note: Automatically swaps R↔B for BGR displays (ST7789V2)"
    exit 1
fi

INPUT="$1"
OUTPUT="${2:-${INPUT%.jpg}.sjpg}"

# Find LVGL conversion script (check local tools/ first, then Arduino libraries)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LVGL_SCRIPT="$SCRIPT_DIR/jpg_to_sjpg.py"

if [ ! -f "$LVGL_SCRIPT" ]; then
    LVGL_SCRIPT="$HOME/Arduino/libraries/lvgl/scripts/jpg_to_sjpg.py"
    if [ ! -f "$LVGL_SCRIPT" ]; then
        echo "Error: LVGL conversion script not found at:"
        echo "  $SCRIPT_DIR/jpg_to_sjpg.py"
        echo "  $HOME/Arduino/libraries/lvgl/scripts/jpg_to_sjpg.py"
        echo ""
        echo "The script should be included in the tools/ directory"
        exit 1
    fi
fi

if [ ! -f "$INPUT" ]; then
    echo "Error: Input file not found: $INPUT"
    exit 1
fi

echo "Converting JPG to SJPG format (with BGR color swap)..."
echo "Input:  $INPUT"
echo "Output: $OUTPUT"
echo ""

# Create temp file with R↔B swapped
TEMP_JPG=$(mktemp /tmp/jpg2sjpg-XXXXXX.jpg)

# Swap R and B channels using Python (without numpy)
python3 << EOF
from PIL import Image

# Load image
img = Image.open('$INPUT')

# Convert to RGB if needed
if img.mode != 'RGB':
    img = img.convert('RGB')

# Split into R, G, B channels
r, g, b = img.split()

# Merge back as B, G, R (swapping R and B)
img_bgr = Image.merge('RGB', (b, g, r))

# Save as temporary JPG
img_bgr.save('$TEMP_JPG', 'JPEG', quality=95)
print("✓ R↔B channels swapped")
EOF

if [ $? -ne 0 ]; then
    echo "Error: Color swap failed"
    rm -f "$TEMP_JPG"
    exit 1
fi

# Run the SJPG conversion on the BGR image
python3 "$LVGL_SCRIPT" "$TEMP_JPG"

# The script creates files with fixed names, rename if needed
TEMP_BASENAME=$(basename "$TEMP_JPG" .jpg)
GENERATED_SJPG="${TEMP_BASENAME}.sjpg"

if [ -f "$GENERATED_SJPG" ]; then
    mv "$GENERATED_SJPG" "$OUTPUT"
    echo "✓ Conversion successful!"
    echo "  Created: $OUTPUT"
    
    # Show file sizes
    ORIG_SIZE=$(stat -f%z "$INPUT" 2>/dev/null || stat -c%s "$INPUT" 2>/dev/null)
    SJPG_SIZE=$(stat -f%z "$OUTPUT" 2>/dev/null || stat -c%s "$OUTPUT" 2>/dev/null)
    
    echo ""
    echo "File sizes:"
    echo "  Original JPG:  $ORIG_SIZE bytes"
    echo "  SJPG format:   $SJPG_SIZE bytes"
    
    # Clean up C array file if generated
    C_FILE="${TEMP_BASENAME}.c"
    if [ -f "$C_FILE" ]; then
        rm "$C_FILE"
    fi
    
    # Clean up temp files
    rm -f "$TEMP_JPG"
else
    echo "Error: Conversion failed"
    rm -f "$TEMP_JPG"
    exit 1
fi
