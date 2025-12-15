#!/usr/bin/env python3
"""
Test script to debug upload to ESP32
Can run locally without Home Assistant
"""

import requests
import sys

if len(sys.argv) < 3:
    print("Usage: python3 test_upload.py <esp32_ip> <sjpg_file>")
    sys.exit(1)

esp32_ip = sys.argv[1]
sjpg_file = sys.argv[2]

print(f"Testing upload to {esp32_ip}")
print(f"File: {sjpg_file}")

# Read file
with open(sjpg_file, 'rb') as f:
    data = f.read()

print(f"File size: {len(data)} bytes")

# Test different upload methods
print("\n=== Test 1: files={'image': ...} ===")
try:
    files = {'image': ('image.sjpg', data, 'application/octet-stream')}
    response = requests.post(f"http://{esp32_ip}/api/display/image", files=files, timeout=10)
    print(f"Status: {response.status_code}")
    print(f"Response: {response.text}")
except Exception as e:
    print(f"Error: {e}")

print("\n=== Test 2: data=... (raw body) ===")
try:
    headers = {'Content-Type': 'application/octet-stream'}
    response = requests.post(f"http://{esp32_ip}/api/display/image", data=data, headers=headers, timeout=10)
    print(f"Status: {response.status_code}")
    print(f"Response: {response.text}")
except Exception as e:
    print(f"Error: {e}")

print("\n=== Test 3: Simulating curl command ===")
import subprocess
result = subprocess.run(
    ['curl', '-v', '-X', 'POST', '-F', f'image=@{sjpg_file}', f'http://{esp32_ip}/api/display/image'],
    capture_output=True,
    text=True
)
print(f"curl stdout: {result.stdout}")
print(f"curl stderr: {result.stderr}")
