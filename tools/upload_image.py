#!/usr/bin/env python3
"""
ESP32 Image Upload Tool - Unified strip-based and single-file upload

Handles both preprocessing (JPG → SJPG with BGR swap) and uploading.
Supports configurable strip height.

Usage:
    # Strip-based upload (memory efficient, required for large images)
    python3 upload_image.py 192.168.1.111 photo.jpg --mode strip --strip-height 32

    # Single-file upload (faster, for small images)
    python3 upload_image.py 192.168.1.111 photo.jpg --mode single
    
    # Upload pre-converted SJPG
    python3 upload_image.py 192.168.1.111 photo.sjpg --mode strip
    
    # Debug: upload specific strip range
    python3 upload_image.py 192.168.1.111 photo.sjpg --mode strip --start 2 --end 2

Requirements:
    - Python 3.6+
    - Pillow (PIL): pip3 install Pillow
    - requests: pip3 install requests
"""

import argparse
import struct
import sys
import os
import tempfile
import math
import requests
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow not installed. Run: pip3 install Pillow")
    sys.exit(1)


# ============================================================================
# SJPG Conversion (with BGR color swap)
# ============================================================================

def jpg_to_sjpg(input_jpg, strip_height=32, swap_bgr=True):
    """
    Convert JPG to SJPG format with optional BGR color swap.
    
    Args:
        input_jpg: Path to input JPG file
        strip_height: Height of each strip in pixels (8, 16, 32, 64...)
        swap_bgr: Whether to swap R↔B channels for BGR displays
        
    Returns:
        bytes: SJPG file data
    """
    print(f"Converting {input_jpg} to SJPG (strip_height={strip_height}, bgr={swap_bgr})")
    
    # Load and prepare image
    img = Image.open(input_jpg)
    if img.mode != 'RGB':
        img = img.convert('RGB')
    
    width, height = img.size
    print(f"  Image: {width}×{height} pixels")
    
    # BGR color swap if needed
    if swap_bgr:
        r, g, b = img.split()
        img = Image.merge('RGB', (b, g, r))
        print(f"  Swapped R↔B for BGR display")
    
    # Split into strips and encode as JPEG
    num_strips = math.ceil(height / strip_height)
    print(f"  Splitting into {num_strips} strips ({strip_height}px each)")
    
    strip_data = []
    strip_lengths = []
    
    for i in range(num_strips):
        y_start = i * strip_height
        y_end = min((i + 1) * strip_height, height)
        
        # Crop strip
        strip_img = img.crop((0, y_start, width, y_end))
        
        # Save as JPEG to bytes
        import io
        buffer = io.BytesIO()
        strip_img.save(buffer, format='JPEG', quality=90)
        jpeg_data = buffer.getvalue()
        
        strip_data.append(jpeg_data)
        strip_lengths.append(len(jpeg_data))
        
        print(f"    Strip {i}: {len(jpeg_data)} bytes ({y_end - y_start}px high)")
    
    # Build SJPG header
    header = bytearray()
    
    # Magic bytes: "_SJPG__" (7 bytes)
    header += b'_SJPG__'
    
    # Version: "\x00V1.00\x00" (7 bytes)
    header += b'\x00V1.00\x00'
    
    # Width (2 bytes, little-endian)
    header += struct.pack('<H', width)
    
    # Height (2 bytes, little-endian)
    header += struct.pack('<H', height)
    
    # Number of strips (2 bytes, little-endian)
    header += struct.pack('<H', num_strips)
    
    # Strip height (2 bytes, little-endian)
    header += struct.pack('<H', strip_height)
    
    # Strip length table (2 bytes each, little-endian)
    # Note: This matches the existing conversion logic used elsewhere in this repo
    # (e.g. tools/jpg_to_sjpg.py and tools/camera_to_esp32.py) and the LVGL SJPG decoder.
    for length in strip_lengths:
        if length > 0xFFFF:
            raise ValueError(f"Strip too large for SJPG u16 length field: {length} bytes")
        header += struct.pack('<H', length)
    
    # Combine header + all strip data
    sjpg_data = header + b''.join(strip_data)
    
    print(f"  SJPG created: {len(sjpg_data)} bytes (header: {len(header)})")
    
    return sjpg_data


# ============================================================================
# SJPG Parsing
# ============================================================================

def parse_sjpg(sjpg_data):
    """
    Parse SJPG file format and extract strips.
    
    Args:
        sjpg_data: SJPG file data (bytes)
        
    Returns:
        tuple: (width, height, block_height, strips)
    """
    # Validate magic bytes
    if sjpg_data[0:4] != b'_SJP':
        raise ValueError("Invalid SJPG file (missing magic bytes)")
    
    # Parse header (little-endian)
    width = struct.unpack('<H', sjpg_data[14:16])[0]
    height = struct.unpack('<H', sjpg_data[16:18])[0]
    num_strips = struct.unpack('<H', sjpg_data[18:20])[0]
    block_height = struct.unpack('<H', sjpg_data[20:22])[0]
    
    # Parse strip table (2 bytes per entry, starting at byte 22)
    # Historically, the tools in this repo generate a *length table* (u16 per strip).
    # Some documentation (and other tooling) may refer to an *offset table*.
    # We support both by using a simple heuristic.
    table = []
    pos = 22
    for _ in range(num_strips):
        value = struct.unpack('<H', sjpg_data[pos:pos+2])[0]
        table.append(value)
        pos += 2
    
    header_size = pos
    data_size = len(sjpg_data) - header_size
    if data_size < 0:
        raise ValueError("Invalid SJPG: header larger than file")

    # Heuristic: treat as offsets if it looks like a monotonic offset table starting at 0.
    looks_like_offsets = (
        num_strips > 0 and
        table[0] == 0 and
        all(table[i] <= table[i + 1] for i in range(num_strips - 1)) and
        table[-1] <= data_size
    )

    strips = []
    if looks_like_offsets:
        # Offset table: slice between successive offsets
        for i in range(num_strips):
            start = header_size + table[i]
            if i + 1 < num_strips:
                end = header_size + table[i + 1]
            else:
                end = len(sjpg_data)
            if start > end or start < header_size or end > len(sjpg_data):
                raise ValueError("Invalid SJPG offset table")
            strips.append(sjpg_data[start:end])
    else:
        # Length table: slice using cumulative sizes
        offset = 0
        for length in table:
            start = header_size + offset
            end = start + length
            if start > end or start < header_size or end > len(sjpg_data):
                raise ValueError("Invalid SJPG length table")
            strips.append(sjpg_data[start:end])
            offset += length
    
    return width, height, block_height, strips


# ============================================================================
# Upload Functions
# ============================================================================

def upload_single_file(esp32_ip, sjpg_data, timeout=10):
    """
    Upload entire SJPG file in one request (original API).
    
    Args:
        esp32_ip: ESP32 IP address
        sjpg_data: SJPG file data (bytes)
        timeout: Display timeout in seconds
        
    Returns:
        bool: Success
    """
    url = f"http://{esp32_ip}/api/display/image"
    
    print(f"\n=== Single-File Upload ===")
    print(f"URL: {url}")
    print(f"Size: {len(sjpg_data)} bytes")
    print(f"Timeout: {timeout}s")
    
    try:
        files = {'image': ('image.sjpg', sjpg_data, 'application/octet-stream')}
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


def upload_strips(esp32_ip, sjpg_data, timeout=10, start_strip=None, end_strip=None):
    """
    Upload SJPG as individual strips (memory-efficient API).
    
    Args:
        esp32_ip: ESP32 IP address
        sjpg_data: SJPG file data (bytes)
        timeout: Display timeout in seconds
        start_strip: First strip to upload (None = 0)
        end_strip: Last strip to upload (None = all)
        
    Returns:
        bool: Success
    """
    # Parse SJPG
    width, height, block_height, strips = parse_sjpg(sjpg_data)
    
    # Determine strip range
    first = start_strip if start_strip is not None else 0
    last = end_strip if end_strip is not None else len(strips) - 1
    
    print(f"\n=== Strip-Based Upload ===")
    print(f"Image: {width}×{height} pixels")
    print(f"Strips: {len(strips)} total ({block_height}px each)")
    print(f"Range: {first} to {last}")
    print(f"Timeout: {timeout}s")
    
    # Upload each strip
    for i in range(first, last + 1):
        strip_data = strips[i]
        
        # Build URL with metadata
        url = f"http://{esp32_ip}/api/display/strip"
        params = {
            'index': i,
            'total': len(strips),
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
  # Strip-based upload with 32px strips (memory efficient)
  python3 upload_image.py 192.168.1.111 photo.jpg --mode strip --strip-height 32
  
  # Single-file upload (faster for small images)
  python3 upload_image.py 192.168.1.111 photo.jpg --mode single
  
  # Upload pre-converted SJPG
  python3 upload_image.py 192.168.1.111 photo.sjpg --mode strip
  
  # Debug: upload only strip 2
  python3 upload_image.py 192.168.1.111 photo.sjpg --mode strip --start 2 --end 2
  
  # Custom timeout (0 = permanent, default = 10s)
  python3 upload_image.py 192.168.1.111 photo.jpg --timeout 30
        """
    )
    
    parser.add_argument('esp32_ip', help='ESP32 IP address')
    parser.add_argument('image', help='Image file (.jpg, .jpeg, or .sjpg)')
    parser.add_argument('--mode', choices=['single', 'strip'], required=True,
                       help='Upload mode (required)')
    parser.add_argument('--strip-height', type=int, default=None,
                       help='Strip height in pixels for SJPG conversion (default: 16 for --mode single, 32 for --mode strip)')
    parser.add_argument('--timeout', type=int, default=10,
                       help='Display timeout in seconds (0=permanent, default: 10)')
    parser.add_argument('--start', type=int, default=None,
                       help='First strip to upload (debug mode)')
    parser.add_argument('--end', type=int, default=None,
                       help='Last strip to upload (debug mode)')
    parser.add_argument('--no-bgr-swap', action='store_true',
                       help='Disable BGR color swap (for RGB displays)')
    
    args = parser.parse_args()
    
    # Validate inputs
    if not os.path.exists(args.image):
        print(f"Error: Image file not found: {args.image}")
        sys.exit(1)
    
    # Determine file type
    file_ext = Path(args.image).suffix.lower()
    
    # Choose default strip height (if converting from JPG)
    # NOTE: LVGL's SJPG decoder is effectively 16px-strip oriented; using 32 can result
    # in only the first strip being displayed when using the single-file LVGL path.
    strip_height = args.strip_height
    if strip_height is None:
        strip_height = 16 if args.mode == 'single' else 32

    # Convert JPG to SJPG if needed
    if file_ext in ['.jpg', '.jpeg']:
        print(f"Input: {args.image} (JPG)")
        sjpg_data = jpg_to_sjpg(
            args.image, 
            strip_height=strip_height,
            swap_bgr=not args.no_bgr_swap
        )
    elif file_ext == '.sjpg':
        print(f"Input: {args.image} (SJPG)")
        with open(args.image, 'rb') as f:
            sjpg_data = f.read()
        
        # Parse to show info
        width, height, block_height, strips = parse_sjpg(sjpg_data)
        print(f"  Image: {width}×{height} pixels")
        print(f"  Strips: {len(strips)} ({block_height}px each)")
    else:
        print(f"Error: Unsupported file format: {file_ext}")
        print("Supported: .jpg, .jpeg, .sjpg")
        sys.exit(1)
    
    # Upload
    if args.mode == 'single':
        success = upload_single_file(args.esp32_ip, sjpg_data, args.timeout)
    else:  # strip
        success = upload_strips(
            args.esp32_ip, 
            sjpg_data, 
            args.timeout,
            args.start,
            args.end
        )
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
