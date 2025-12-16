#!/usr/bin/env python3
"""
Home Assistant AppDaemon App: Camera to ESP32 Image Display

Fetches camera snapshots from Home Assistant and sends them to ESP32 devices
as baseline JPEG via the Image Display API.

How it works (high-level):
    1) Fetch camera snapshot from Home Assistant (camera_proxy)
    2) Optional rotation (auto/no/explicit)
    3) Resize with letterboxing to panel size (default 240x280)
    4) Encode as baseline JPEG (TJpgDec-friendly)
    5) Upload via either:
         - single: POST /api/display/image (multipart)
         - strip:  POST /api/display/image/chunks (multiple requests)

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
            # Optional knobs:
            # mode: single|strip        (default: single)
                        # rotate_degrees: null|0|90|180|270 (default: null = auto)
                        #   - null / omitted: auto-rotate landscape->portrait when targeting a portrait panel
                        #   - 0: no rotation
                        #   - 90/180/270: explicit rotation
            # strip_height: 16|32|...   (default: 32)
            # timeout: seconds          (default: 10; 0 = permanent)
            # jpeg_quality: 60-95       (default: apps.yaml setting or 80)
            # dismiss: true             (optional: just dismiss current image)

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

        # Default upload mode: 'single' (multipart) or 'strip' (chunked fragments)
        self.default_mode = str(self.args.get("mode", "single")).strip().lower()

        # Default strip height for chunked uploads
        self.default_strip_height = int(self.args.get("strip_height", 32))

        # Rotation control (unified):
        #   - None: auto-rotate landscape->portrait when targeting a portrait panel
        #   - 0: no rotation
        #   - 90/180/270: explicit rotation
        raw_rotate_degrees = self.args.get("rotate_degrees", None)
        self.default_rotate_degrees = None if raw_rotate_degrees is None else int(raw_rotate_degrees)

        raw_rotate = self.args.get("rotate", None)
        if raw_rotate is not None and raw_rotate_degrees is None:
            if not bool(raw_rotate):
                self.default_rotate_degrees = 0
        
        # Listen for camera_to_esp32 events
        self.listen_event(self.handle_event, "camera_to_esp32")
    
    def handle_event(self, event_name, data, kwargs):
        """Handle camera_to_esp32 event"""
        # Debug: log all event data
        self.log(f"Event data received: {data}")
        
        camera_entity = data.get("camera_entity")
        esp32_ip = data.get("esp32_ip", self.default_esp32_ip)
        timeout = data.get("timeout", 10)  # Default 10 seconds

        # Optional behavior
        mode = str(data.get("mode", self.default_mode)).strip().lower()
        rotate_degrees = data.get("rotate_degrees", self.default_rotate_degrees)
        strip_height = int(data.get("strip_height", self.default_strip_height))
        jpeg_quality = int(data.get("jpeg_quality", self.jpeg_quality))
        dismiss = bool(data.get("dismiss", False))

        if "rotate_degrees" not in data and "rotate" in data:
            if not bool(data.get("rotate")):
                rotate_degrees = 0
            else:
                rotate_degrees = None
        
        self.log(f"Parsed timeout value: {timeout} (type: {type(timeout).__name__})")
        
        if dismiss:
            if not esp32_ip:
                self.error("Missing required parameter: esp32_ip (or set default_esp32_ip in apps.yaml)")
                return
            self.log(f"Dismissing image on {esp32_ip}")
            ok = self.dismiss_image(esp32_ip)
            if ok:
                self.log("[OK] Dismiss successful")
            else:
                self.error("Dismiss failed")
            return

        if not camera_entity:
            self.error("Missing required parameter: camera_entity")
            return
        
        if not esp32_ip:
            self.error("Missing required parameter: esp32_ip (or set default_esp32_ip in apps.yaml)")
            return

        if mode not in ("single", "strip"):
            self.error(f"Invalid mode '{mode}'. Expected 'single' or 'strip'.")
            return

        if rotate_degrees is not None:
            try:
                rotate_degrees = int(rotate_degrees)
            except Exception:
                self.error(f"Invalid rotate_degrees '{rotate_degrees}'. Expected 0/90/180/270")
                return
            if rotate_degrees not in (0, 90, 180, 270):
                self.error(f"Invalid rotate_degrees '{rotate_degrees}'. Expected 0/90/180/270")
                return

        rotate_desc = "auto" if rotate_degrees is None else str(rotate_degrees)

        self.log(
            f"Processing camera snapshot: {camera_entity} -> {esp32_ip} "
            f"(mode={mode}, rotate_degrees={rotate_desc}, strip_height={strip_height}, timeout={timeout}s)"
        )
        
        # Process and send snapshot
        self.process_camera_snapshot(
            camera_entity,
            esp32_ip,
            timeout=timeout,
            mode=mode,
            rotate_degrees=rotate_degrees,
            strip_height=strip_height,
            jpeg_quality=jpeg_quality,
        )
    
    def process_camera_snapshot(
        self,
        camera_entity,
        esp32_ip,
        timeout=10,
        mode="single",
        rotate_degrees=None,
        strip_height=32,
        jpeg_quality=80,
    ):
        """Fetch camera snapshot, preprocess, and upload using the selected API mode."""
        
        try:
            # Step 1: Fetch camera snapshot
            self.log(f"Fetching snapshot from {camera_entity}")
            jpeg_data = self.get_camera_snapshot(camera_entity)
            
            if not jpeg_data:
                self.error(f"Failed to fetch camera snapshot from {camera_entity}")
                return
            
            self.log(f"Fetched camera snapshot: {len(jpeg_data)} bytes")
            
            # Step 2: Preprocess (rotation + letterbox resize)
            self.log(f"Preparing image for {self.display_width}x{self.display_height} panel")
            img = self.prepare_image(
                jpeg_data,
                target_width=self.display_width,
                target_height=self.display_height,
                rotate_degrees=rotate_degrees,
            )
            if img is None:
                self.error("Failed to prepare image")
                return
            
            # Step 3: Test ESP32 connectivity (cheap API endpoint)
            self.log(f"Testing ESP32 connectivity at {esp32_ip}")
            try:
                test_response = requests.get(f"http://{esp32_ip}/api/health", timeout=5)
                self.log(f"ESP32 reachable: HTTP {test_response.status_code}")
            except Exception as e:
                self.error(f"ESP32 connectivity test failed: {e}")
                return
            
            # Step 4: Upload to ESP32
            if mode == "single":
                out_jpeg = self.encode_baseline_jpeg(img, quality=jpeg_quality)
                self.log(f"Uploading (single) to {esp32_ip} with {timeout}s timeout ({len(out_jpeg)} bytes)")
                success = self.upload_single(out_jpeg, esp32_ip, timeout)
            else:
                strips = self.image_to_jpeg_strips(img, strip_height=strip_height, quality=jpeg_quality)
                self.log(
                    f"Uploading (strip) to {esp32_ip} with {timeout}s timeout "
                    f"({len(strips)} strips @ {strip_height}px)"
                )
                success = self.upload_strips(strips, esp32_ip, timeout)
            
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
    
    def prepare_image(self, jpeg_data, target_width=None, target_height=None, rotate_degrees=None):
        """Decode bytes into an RGB PIL image, rotate (auto/no/explicit), then letterbox-resize to target."""
        try:
            # Load JPEG from bytes
            img = Image.open(io.BytesIO(jpeg_data))
            original_size = img.size

            # Rotation
            if rotate_degrees is None:
                # Auto-rotate landscape -> portrait when targeting a portrait panel.
                if target_width and target_height:
                    if original_size[0] > original_size[1] and target_height > target_width:
                        self.log(
                            f"Auto-rotating landscape snapshot to portrait (source {original_size[0]}x{original_size[1]})"
                        )
                        img = img.transpose(Image.ROTATE_90)
                        original_size = img.size
            elif rotate_degrees != 0:
                # PIL rotates counter-clockwise for positive degrees.
                self.log(f"Rotating by {rotate_degrees} degrees")
                img = img.rotate(rotate_degrees, expand=True)
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

            return img
            
        except Exception as e:
            self.error(f"Failed to prepare image: {e}")
            return None

    def encode_baseline_jpeg(self, img, quality=80):
        """Encode a PIL image as baseline JPEG (TJpgDec-friendly)."""
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

    def image_to_jpeg_strips(self, img, strip_height=32, quality=80):
        """Split a prepared PIL image into baseline-JPEG fragments for /api/display/image/chunks."""
        width, height = img.size
        strips = []
        for y in range(0, height, strip_height):
            strip = img.crop((0, y, width, min(y + strip_height, height)))
            strips.append(self.encode_baseline_jpeg(strip, quality=quality))
        return strips

    def upload_single(self, jpeg_data, esp32_ip, timeout=10):
        """Upload JPEG data to ESP32 via multipart POST /api/display/image."""
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

    def upload_strips(self, strips, esp32_ip, timeout=10):
        """Upload JPEG fragments via POST /api/display/image/chunks."""
        try:
            total = len(strips)
            width = self.display_width
            height = self.display_height

            for idx, strip_data in enumerate(strips):
                url = f"http://{esp32_ip}/api/display/image/chunks"
                params = {
                    'index': idx,
                    'total': total,
                    'width': width,
                    'height': height,
                    'timeout': timeout,
                }
                response = requests.post(
                    url,
                    params=params,
                    data=strip_data,
                    headers={'Content-Type': 'application/octet-stream'},
                    timeout=10,
                )
                if response.status_code != 200 or '"success":true' not in response.text:
                    self.error(f"Strip {idx}/{total-1} failed: HTTP {response.status_code} {response.text}")
                    return False

            return True
        except Exception as e:
            self.error(f"Strip upload error: {e}")
            import traceback
            self.error(traceback.format_exc())
            return False

    def dismiss_image(self, esp32_ip):
        """Dismiss the current image via DELETE /api/display/image."""
        try:
            url = f"http://{esp32_ip}/api/display/image"
            response = requests.delete(url, timeout=10)
            self.log(f"Dismiss response: HTTP {response.status_code} {response.text}")
            return response.status_code == 200 and '"success":true' in response.text
        except Exception as e:
            self.error(f"Dismiss error: {e}")
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
