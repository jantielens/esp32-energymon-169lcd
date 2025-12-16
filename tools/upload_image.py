#!/usr/bin/env python3
"""ESP32 Image Upload Tool

Uploads a plain baseline JPEG to the device.

Endpoints:
  - POST /api/display/image          (single upload via multipart)
    - POST /api/display/image/strips   (strip uploads via raw body)

Notes:
  - Firmware expects normal RGB JPEG input; no client-side BGR swapping.
  - Firmware may reject unsupported JPEG encodings; this tool re-encodes strips/single
    as baseline JPEG with 4:2:0 chroma subsampling by default.

Usage:
    # Strip upload (memory efficient)
    python3 upload_image.py 192.168.1.111 photo.jpg --mode strip --strip-height 32

    # Single-file upload
    python3 upload_image.py 192.168.1.111 photo.jpg --mode single

Requirements:
    - Python 3.6+
    - Pillow (PIL): pip3 install Pillow
    - requests: pip3 install requests
"""

import argparse
import sys
import os
import math
import requests
from typing import List, Tuple

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow not installed. Run: pip3 install Pillow")
    sys.exit(1)


# ============================================================================
# JPEG helpers
# ============================================================================

def _load_rgb_image(path: str) -> Image.Image:
    img = Image.open(path)
    if img.mode != 'RGB':
        img = img.convert('RGB')
    return img


def _encode_baseline_jpeg(img: Image.Image, quality: int = 90) -> bytes:
    """Encode as baseline JPEG with 4:2:0 subsampling (TJpgDec-friendly)."""
    import io
    buffer = io.BytesIO()
    img.save(
        buffer,
        format='JPEG',
        quality=quality,
        subsampling=2,      # 4:2:0
        progressive=False,
        optimize=False,
    )
    return buffer.getvalue()


def image_to_jpeg_bytes(input_path: str, quality: int = 90) -> Tuple[int, int, bytes]:
    img = _load_rgb_image(input_path)
    width, height = img.size
    jpeg_data = _encode_baseline_jpeg(img, quality=quality)
    return width, height, jpeg_data


def image_to_jpeg_strips(input_path: str, strip_height: int, quality: int = 90) -> Tuple[int, int, int, List[bytes]]:
    img = _load_rgb_image(input_path)
    width, height = img.size
    num_strips = math.ceil(height / strip_height)
    strips: List[bytes] = []

    for i in range(num_strips):
        y_start = i * strip_height
        y_end = min((i + 1) * strip_height, height)
        strip_img = img.crop((0, y_start, width, y_end))
        strips.append(_encode_baseline_jpeg(strip_img, quality=quality))

    return width, height, strip_height, strips


# ============================================================================
# Upload Functions
# ============================================================================

def upload_single_file(esp32_ip, jpeg_data, timeout=10):
    """
    Upload a single baseline JPEG via multipart.
    
    Args:
        esp32_ip: ESP32 IP address
        jpeg_data: JPEG file data (bytes)
        timeout: Display timeout in seconds
        
    Returns:
        bool: Success
    """
    url = f"http://{esp32_ip}/api/display/image"
    
    print(f"\n=== Single-File Upload (JPEG) ===")
    print(f"URL: {url}")
    print(f"Size: {len(jpeg_data)} bytes")
    print(f"Timeout: {timeout}s")
    
    try:
        files = {'image': ('image.jpg', jpeg_data, 'image/jpeg')}
        params = {'timeout': timeout}
        
        response = requests.post(url, files=files, params=params, timeout=30)
        
        if response.status_code == 200:
            print(f"✓ Upload successful")
            return True
        else:
            print(f"✗ Upload failed: HTTP {response.status_code}")
            print(f"  Response: {response.text}")
            return False
            
    except Exception as e:
        print(f"✗ Upload failed: {e}")
        return False


def upload_strips(esp32_ip, width, height, strips, timeout=10, start_strip=None, end_strip=None):
    """
    Upload the image as individual JPEG fragments (memory-efficient API).
    
    Args:
        esp32_ip: ESP32 IP address
        timeout: Display timeout in seconds
        start_strip: First strip to upload (None = 0)
        end_strip: Last strip to upload (None = all)
        
    Returns:
        bool: Success
    """
    # Determine strip range
    first = start_strip if start_strip is not None else 0
    last = end_strip if end_strip is not None else len(strips) - 1
    
    print(f"\n=== Strip Upload (JPEG fragments) ===")
    print(f"Image: {width}×{height} pixels")
    print(f"Strips: {len(strips)} total")
    print(f"Range: {first} to {last}")
    print(f"Timeout: {timeout}s")
    
    # Upload each strip
    for i in range(first, last + 1):
        strip_data = strips[i]
        
        # Build URL with metadata
        url = f"http://{esp32_ip}/api/display/image/strips"
        params = {
            'strip_index': i,
            'strip_count': len(strips),
            'width': width,
            'height': height,
            'timeout': timeout
        }
        
        try:
            response = requests.post(
                url,
                params=params,
                data=strip_data,
                headers={'Content-Type': 'application/octet-stream'},
                timeout=10
            )
            
            if response.status_code == 200:
                print(f"  Strip {i}/{len(strips)-1}: ✓ ({len(strip_data)} bytes)")
            else:
                print(f"  Strip {i}: ✗ HTTP {response.status_code}")
                print(f"    Response: {response.text}")
                return False
                
        except Exception as e:
            print(f"  Strip {i}: ✗ {e}")
            return False
    
    print(f"\n✓ All strips uploaded successfully!")
    return True


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='ESP32 Image Upload Tool - Strip-based and single-file upload',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Strip upload with 32px strips (memory efficient)
  python3 upload_image.py 192.168.1.111 photo.jpg --mode strip --strip-height 32
  
  # Single-file upload (faster for small images)
  python3 upload_image.py 192.168.1.111 photo.jpg --mode single
  
  # Custom timeout (0 = permanent, default = 10s)
  python3 upload_image.py 192.168.1.111 photo.jpg --timeout 30
        """
    )
    
    parser.add_argument('esp32_ip', help='ESP32 IP address')
    parser.add_argument('image', help='Image file (any Pillow-supported format; will be encoded as JPEG)')
    parser.add_argument('--mode', choices=['single', 'strip'], required=True,
                       help='Upload mode (required)')
    parser.add_argument('--strip-height', type=int, default=None,
                       help='Strip height in pixels for strip upload (default: 32)')
    parser.add_argument('--timeout', type=int, default=10,
                       help='Display timeout in seconds (0=permanent, default: 10)')
    parser.add_argument('--start', type=int, default=None,
                       help='First strip to upload (debug mode)')
    parser.add_argument('--end', type=int, default=None,
                       help='Last strip to upload (debug mode)')
    parser.add_argument('--jpeg-quality', type=int, default=90,
                       help='JPEG quality for (re)encoding (default: 90)')
    
    args = parser.parse_args()
    
    # Validate inputs
    if not os.path.exists(args.image):
        print(f"Error: Image file not found: {args.image}")
        sys.exit(1)
    
    print(f"Input: {args.image}")
    strip_height = args.strip_height
    if strip_height is None:
        strip_height = 32
    
    # Upload
    if args.mode == 'single':
        width, height, jpeg_data = image_to_jpeg_bytes(args.image, quality=args.jpeg_quality)
        print(f"  Image: {width}×{height} pixels")
        success = upload_single_file(args.esp32_ip, jpeg_data, args.timeout)
    else:  # strip
        width, height, _, strips = image_to_jpeg_strips(args.image, strip_height=strip_height, quality=args.jpeg_quality)
        print(f"  Image: {width}×{height} pixels")
        print(f"  Strips: {len(strips)} total ({strip_height}px target)")
        success = upload_strips(
            args.esp32_ip,
            width,
            height,
            strips,
            args.timeout,
            args.start,
            args.end,
        )
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
