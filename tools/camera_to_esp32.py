#!/usr/bin/env python3
"""
Home Assistant AppDaemon App: Camera to ESP32 Image Display

Fetches camera snapshots from Home Assistant and sends them to ESP32 devices
with automatic BGR color conversion and SJPG format conversion.

Installation:
  1. Copy this file to: /addon_configs/a0d7b954_appdaemon/apps/
  2. Add to apps.yaml:
     camera_to_esp32:
       module: camera_to_esp32
       class: CameraToESP32

Usage (from automation):
  - event: camera_to_esp32
    event_data:
      camera_entity: camera.front_door
      esp32_ip: "192.168.1.111"

Requirements:
  - Pillow (add to AppDaemon python_packages)
"""

import appdaemon.plugins.hass.hassapi as hass
import os
import tempfile
import subprocess
import math
from PIL import Image
import io
import requests

class CameraToESP32(hass.Hass):
    """AppDaemon app to send camera snapshots to ESP32 display"""
    
    def initialize(self):
        """Initialize the app and register event listener"""
        self.log("CameraToESP32 initialized")
        
        # Get optional default ESP32 IP from config
        self.default_esp32_ip = self.args.get("default_esp32_ip", None)
        
        # ESP32 display dimensions (resize images to fit)
        # ST7789V2 is 280 wide x 240 tall in landscape orientation
        self.display_width = self.args.get("display_width", 280)
        self.display_height = self.args.get("display_height", 240)
        
        # Listen for camera_to_esp32 events
        self.listen_event(self.handle_event, "camera_to_esp32")
    
    def handle_event(self, event_name, data, kwargs):
        """Handle camera_to_esp32 event"""
        # Debug: log all event data
        self.log(f"Event data received: {data}")
        
        camera_entity = data.get("camera_entity")
        esp32_ip = data.get("esp32_ip", self.default_esp32_ip)
        timeout = data.get("timeout", 10)  # Default 10 seconds
        
        self.log(f"Parsed timeout value: {timeout} (type: {type(timeout).__name__})")
        
        if not camera_entity:
            self.error("Missing required parameter: camera_entity")
            return
        
        if not esp32_ip:
            self.error("Missing required parameter: esp32_ip (or set default_esp32_ip in apps.yaml)")
            return
        
        self.log(f"Processing camera snapshot: {camera_entity} -> {esp32_ip} (timeout: {timeout}s)")
        
        # Process and send snapshot
        self.process_camera_snapshot(camera_entity, esp32_ip, timeout)
    
    def process_camera_snapshot(self, camera_entity, esp32_ip, timeout=10):
        """Fetch camera snapshot, convert to SJPG, and upload to ESP32"""
        
        try:
            # Step 1: Fetch camera snapshot
            self.log(f"Fetching snapshot from {camera_entity}")
            jpeg_data = self.get_camera_snapshot(camera_entity)
            
            if not jpeg_data:
                self.error(f"Failed to fetch camera snapshot from {camera_entity}")
                return
            
            self.log(f"Fetched camera snapshot: {len(jpeg_data)} bytes")
            
            # Step 2: Resize and convert RGB → BGR
            self.log(f"Resizing to {self.display_width}x{self.display_height} and converting RGB to BGR")
            bgr_jpeg_data = self.convert_rgb_to_bgr(jpeg_data, self.display_width, self.display_height)
            
            if not bgr_jpeg_data:
                self.error("Failed to convert to BGR")
                return
            
            self.log(f"Resized and converted to BGR JPEG: {len(bgr_jpeg_data)} bytes")
            
            # Step 3: Convert to SJPG format
            self.log("Converting to SJPG format")
            sjpg_data = self.convert_to_sjpg(bgr_jpeg_data)
            
            if not sjpg_data:
                self.error("Failed to convert to SJPG")
                return
            
            self.log(f"Converted to SJPG: {len(sjpg_data)} bytes")
            
            # Step 4: Test ESP32 connectivity
            self.log(f"Testing ESP32 connectivity at {esp32_ip}")
            try:
                test_response = requests.get(f"http://{esp32_ip}/", timeout=5)
                self.log(f"ESP32 reachable: HTTP {test_response.status_code}")
            except Exception as e:
                self.error(f"ESP32 connectivity test failed: {e}")
                return
            
            # Step 5: Upload to ESP32
            self.log(f"Uploading to ESP32 at {esp32_ip} with {timeout}s timeout")
            success = self.upload_to_esp32(sjpg_data, esp32_ip, timeout)
            
            if success:
                self.log(f"[OK] Upload successful!")
            else:
                self.error(f"Upload failed!")
                
        except Exception as e:
            self.error(f"Error processing camera snapshot: {e}", level="ERROR")
    
    def get_camera_snapshot(self, camera_entity):
        """Fetch camera snapshot from Home Assistant"""
        try:
            # Use HA camera proxy to get snapshot
            url = f"{self.get_ha_url()}/api/camera_proxy/{camera_entity}"
            headers = {
                "Authorization": f"Bearer {self.get_ha_token()}",
                "Content-Type": "application/json"
            }
            
            response = requests.get(url, headers=headers, timeout=10)
            
            if response.status_code == 200:
                return response.content
            else:
                self.error(f"Camera snapshot failed: HTTP {response.status_code}")
                return None
                
        except Exception as e:
            self.error(f"Failed to fetch camera snapshot: {e}")
            return None
    
    def convert_rgb_to_bgr(self, jpeg_data, target_width=None, target_height=None):
        """Convert RGB JPEG to BGR JPEG using PIL, with optional resizing that maintains aspect ratio"""
        try:
            # Load JPEG from bytes
            img = Image.open(io.BytesIO(jpeg_data))
            original_size = img.size
            
            # Resize if target dimensions provided, maintaining aspect ratio
            if target_width and target_height:
                self.log(f"Resizing from {original_size[0]}x{original_size[1]} to fit {target_width}x{target_height}")
                
                # Calculate scaling to fit within target dimensions
                width_ratio = target_width / original_size[0]
                height_ratio = target_height / original_size[1]
                scale_ratio = min(width_ratio, height_ratio)
                
                # Calculate new size maintaining aspect ratio
                new_width = int(original_size[0] * scale_ratio)
                new_height = int(original_size[1] * scale_ratio)
                
                self.log(f"Scaled to {new_width}x{new_height} (scale: {scale_ratio:.3f})")
                
                # Resize image
                img = img.resize((new_width, new_height), Image.Resampling.LANCZOS)
                
                # Create black background at target size
                background = Image.new('RGB', (target_width, target_height), (0, 0, 0))
                
                # Calculate position to center the image
                offset_x = (target_width - new_width) // 2
                offset_y = (target_height - new_height) // 2
                
                # Paste resized image onto black background
                background.paste(img, (offset_x, offset_y))
                img = background
                
                self.log(f"Centered with offset ({offset_x}, {offset_y})")
            
            # Convert to RGB if needed (handles RGBA, grayscale, etc.)
            if img.mode != 'RGB':
                img = img.convert('RGB')
            
            # Split into R, G, B channels
            r, g, b = img.split()
            
            # Merge back as B, G, R (swapping R and B)
            img_bgr = Image.merge('RGB', (b, g, r))
            
            # Save back to JPEG bytes with lower quality to reduce size
            output = io.BytesIO()
            img_bgr.save(output, format='JPEG', quality=70, optimize=True)
            bgr_jpeg_data = output.getvalue()
            
            return bgr_jpeg_data
            
        except Exception as e:
            self.error(f"Failed to convert RGB to BGR: {e}")
            return None
    
    def convert_to_sjpg(self, jpeg_data):
        """Convert JPEG to SJPG format using inline conversion (no subprocess)"""
        try:
            # Load JPEG from bytes
            im = Image.open(io.BytesIO(jpeg_data))
            width, height = im.size
            
            self.log(f"Image size: {width}x{height}")
            
            # SJPG parameters
            JPEG_SPLIT_HEIGHT = 16
            SJPG_FILE_FORMAT_VERSION = "V1.00"
            
            splits = math.ceil(height / JPEG_SPLIT_HEIGHT)
            self.log(f"Splitting into {splits} strips of {JPEG_SPLIT_HEIGHT}px height")
            
            # Create JPEG strips
            lenbuf = []
            sjpeg_data = bytearray()
            
            row_remaining = height
            for i in range(splits):
                if row_remaining < JPEG_SPLIT_HEIGHT:
                    crop = im.crop((0, i * JPEG_SPLIT_HEIGHT, width, row_remaining + i * JPEG_SPLIT_HEIGHT))
                else:
                    crop = im.crop((0, i * JPEG_SPLIT_HEIGHT, width, JPEG_SPLIT_HEIGHT + i * JPEG_SPLIT_HEIGHT))
                
                row_remaining = row_remaining - JPEG_SPLIT_HEIGHT
                
                # Save crop as JPEG to bytes with lower quality
                crop_bytes = io.BytesIO()
                crop.save(crop_bytes, format='JPEG', quality=60, optimize=True)
                crop_data = crop_bytes.getvalue()
                
                sjpeg_data += crop_data
                lenbuf.append(len(crop_data))
            
            self.log(f"Created {len(lenbuf)} JPEG strips, total {len(sjpeg_data)} bytes")
            
            # Build SJPG header
            header = bytearray()
            header += bytearray("_SJPG__".encode("UTF-8"))
            header += bytearray(("\x00" + SJPG_FILE_FORMAT_VERSION + "\x00").encode("UTF-8"))
            header += width.to_bytes(2, byteorder='little')
            header += height.to_bytes(2, byteorder='little')
            header += splits.to_bytes(2, byteorder='little')
            header += int(JPEG_SPLIT_HEIGHT).to_bytes(2, byteorder='little')
            
            for item_len in lenbuf:
                header += item_len.to_bytes(2, byteorder='little')
            
            # Combine header + data
            sjpeg = header + sjpeg_data
            
            self.log(f"SJPG total size: {len(sjpeg)} bytes (header: {len(header)}, data: {len(sjpeg_data)})")
            
            return bytes(sjpeg)
            
        except Exception as e:
            self.error(f"Failed to convert to SJPG: {e}")
            import traceback
            self.error(traceback.format_exc())
            return None
    
    def upload_to_esp32(self, sjpg_data, esp32_ip, timeout=10):
        """Upload SJPG data to ESP32 via HTTP POST using curl"""
        try:
            url = f"http://{esp32_ip}/api/display/image?timeout={timeout}"
            
            # Save SJPG to temporary file for curl
            with tempfile.NamedTemporaryFile(delete=False, suffix='.sjpg') as tmp:
                tmp.write(sjpg_data)
                tmp_path = tmp.name
            
            try:
                # Use curl for upload (reliable multipart/form-data)
                result = subprocess.run(
                    ['curl', '-X', 'POST', '-F', f'image=@{tmp_path}', url],
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                
                self.log(f"Upload response: {result.stdout}")
                
                if result.returncode == 0 and '"success":true' in result.stdout:
                    return True
                else:
                    self.error(f"Upload failed: {result.stdout}")
                    return False
                    
            finally:
                # Clean up temp file
                os.unlink(tmp_path)
                
        except subprocess.TimeoutExpired:
            self.error("Upload timeout")
            return False
        except Exception as e:
            self.error(f"Upload error: {e}")
            import traceback
            self.error(traceback.format_exc())
            return False
    
    def get_ha_url(self):
        """Get Home Assistant URL"""
        # AppDaemon is typically on same host
        return "http://supervisor/core"
    
    def get_ha_token(self):
        """Get Home Assistant long-lived access token"""
        # AppDaemon has access to HA tokens via supervisor
        # This is automatically handled by the Hass API
        # For direct API calls, we can use the config
        return os.environ.get('SUPERVISOR_TOKEN', '')
