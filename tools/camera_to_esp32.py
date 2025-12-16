#!/usr/bin/env python3
"""
Home Assistant AppDaemon App: Camera to ESP32 Image Display

Fetches camera snapshots from Home Assistant and sends them to ESP32 devices
as a baseline JPEG via the Image Display API.

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
import io
from PIL import Image
import requests

class CameraToESP32(hass.Hass):
    """AppDaemon app to send camera snapshots to ESP32 display"""
    
    def initialize(self):
        """Initialize the app and register event listener"""
        self.log("CameraToESP32 initialized")
        
        # Get optional default ESP32 IP from config
        self.default_esp32_ip = self.args.get("default_esp32_ip", None)
        
        # ESP32 display dimensions (resize images to fit)
        # Raw panel coordinates (default matches firmware expectations)
        self.display_width = self.args.get("display_width", 240)
        self.display_height = self.args.get("display_height", 280)

        # JPEG quality for re-encoding (baseline JPEG, TJpgDec-friendly)
        self.jpeg_quality = int(self.args.get("jpeg_quality", 80))
        
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
        """Fetch camera snapshot, resize/letterbox, and upload as baseline JPEG"""
        
        try:
            # Step 1: Fetch camera snapshot
            self.log(f"Fetching snapshot from {camera_entity}")
            jpeg_data = self.get_camera_snapshot(camera_entity)
            
            if not jpeg_data:
                self.error(f"Failed to fetch camera snapshot from {camera_entity}")
                return
            
            self.log(f"Fetched camera snapshot: {len(jpeg_data)} bytes")
            
            # Step 2: Resize/letterbox and (re)encode as baseline JPEG
            self.log(f"Resizing to {self.display_width}x{self.display_height} and encoding as baseline JPEG")
            out_jpeg = self.resize_and_encode_jpeg(
                jpeg_data,
                target_width=self.display_width,
                target_height=self.display_height,
                quality=self.jpeg_quality,
            )

            if not out_jpeg:
                self.error("Failed to resize/encode JPEG")
                return

            self.log(f"Prepared JPEG: {len(out_jpeg)} bytes")
            
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
            success = self.upload_to_esp32(out_jpeg, esp32_ip, timeout)
            
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
    
    def resize_and_encode_jpeg(self, jpeg_data, target_width=None, target_height=None, quality=80):
        """Resize/letterbox and encode as baseline JPEG (TJpgDec-friendly)."""
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

            # Save back to baseline JPEG bytes (no progressive/optimize)
            output = io.BytesIO()
            img.save(
                output,
                format='JPEG',
                quality=quality,
                subsampling=2,   # 4:2:0
                progressive=False,
                optimize=False,
            )
            return output.getvalue()
            
        except Exception as e:
            self.error(f"Failed to resize/encode JPEG: {e}")
            return None

    def upload_to_esp32(self, jpeg_data, esp32_ip, timeout=10):
        """Upload JPEG data to ESP32 via HTTP multipart POST."""
        try:
            url = f"http://{esp32_ip}/api/display/image"
            files = {'image': ('image.jpg', jpeg_data, 'image/jpeg')}
            params = {'timeout': timeout}

            response = requests.post(url, files=files, params=params, timeout=15)
            self.log(f"Upload response: HTTP {response.status_code} {response.text}")

            return response.status_code == 200 and '"success":true' in response.text
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
