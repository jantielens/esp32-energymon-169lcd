#!/bin/bash
# Test script for image display API
# Usage: ./test-image-api.sh <device-ip> <image-file>
# Supports: .jpg, .jpeg (auto-converts to .sjpg), or .sjpg files

if [ $# -lt 2 ]; then
    echo "Error: Missing required parameters"
    echo "Usage: $0 <device-ip> <image-file>"
    echo ""
    echo "Supported formats:"
    echo "  - .jpg, .jpeg (will be auto-converted to SJPG)"
    echo "  - .sjpg (ready to upload)"
    echo ""
    echo "Example: $0 192.168.1.100 photo.jpg"
    exit 1
fi

DEVICE_IP="$1"
IMAGE_FILE="$2"

if [ ! -f "$IMAGE_FILE" ]; then
    echo "Error: Image file '$IMAGE_FILE' not found"
    echo "Usage: $0 <device-ip> <image-file>"
    exit 1
fi

# Get script directory for jpg2sjpg.sh
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== ESP32 Energy Monitor - Image Display API Test ==="
echo "Device IP: $DEVICE_IP"
echo "Original file: $IMAGE_FILE"
echo ""

# Detect file type and convert if needed
FILE_EXT="${IMAGE_FILE##*.}"
FILE_EXT_LOWER=$(echo "$FILE_EXT" | tr '[:upper:]' '[:lower:]')

UPLOAD_FILE="$IMAGE_FILE"
TEMP_SJPG=""

if [ "$FILE_EXT_LOWER" = "jpg" ] || [ "$FILE_EXT_LOWER" = "jpeg" ]; then
    echo "Detected JPEG format - converting to SJPG..."
    
    # Create temp file for SJPG
    TEMP_SJPG=$(mktemp /tmp/test-image-XXXXXX.sjpg)
    
    # Convert using jpg2sjpg.sh
    if ! "$SCRIPT_DIR/jpg2sjpg.sh" "$IMAGE_FILE" "$TEMP_SJPG"; then
        echo "Error: SJPG conversion failed"
        rm -f "$TEMP_SJPG"
        exit 1
    fi
    
    UPLOAD_FILE="$TEMP_SJPG"
    echo "✓ Converted to SJPG"
    echo ""
elif [ "$FILE_EXT_LOWER" != "sjpg" ]; then
    echo "Error: Unsupported file format '.$FILE_EXT'"
    echo "Supported: .jpg, .jpeg, .sjpg"
    exit 1
fi

# Check image size
IMAGE_SIZE=$(stat -f%z "$UPLOAD_FILE" 2>/dev/null || stat -c%s "$UPLOAD_FILE" 2>/dev/null)
echo "Upload size: $IMAGE_SIZE bytes"

if [ $IMAGE_SIZE -gt 102400 ]; then
    echo "Warning: Image larger than 100KB may be rejected"
fi

echo ""
echo "Uploading image to device..."
RESPONSE=$(curl -s -X POST \
    -F "image=@$UPLOAD_FILE" \
    http://$DEVICE_IP/api/display/image)

# Clean up temp file if created
if [ -n "$TEMP_SJPG" ]; then
    rm -f "$TEMP_SJPG"
fi

echo "Response: $RESPONSE"
echo ""

# Parse success status
if echo "$RESPONSE" | grep -q '"success":true'; then
    echo "✓ Image uploaded and displayed successfully!"
    echo "  Image will auto-dismiss after 10 seconds"
    echo ""
    echo "To manually dismiss before timeout, run:"
    echo "   curl -X DELETE http://$DEVICE_IP/api/display/image"
else
    echo "✗ Upload failed"
    exit 1
fi
