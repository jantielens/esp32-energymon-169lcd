#!/usr/bin/env bash
set -euo pipefail

# Interactive Image API test runner
#
# Runs a small suite of end-to-end tests against the Image Display API.
# After each step it pauses so you can visually confirm what the LCD shows.
#
# Usage:
#   ./tools/upload_image_tests.sh 192.168.1.111 ./tools/test240x280.jpg
#
# Notes:
# - Uses tools/upload_image.py for most tests (it re-encodes to baseline JPEG).
# - For the curl single-upload test, this script generates a baseline JPEG temp file
#   so the curl test is not dependent on the input image's JPEG encoding.

ESP32_IP="${1:-}"
INPUT_IMAGE="${2:-}"

if [[ -z "$ESP32_IP" || -z "$INPUT_IMAGE" ]]; then
  echo "Usage: $0 <esp32_ip> <image_path>"
  echo "Example: $0 192.168.1.111 ./tools/test240x280.jpg"
  exit 2
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="$ROOT_DIR/tools"
UPLOAD_PY="$TOOLS_DIR/upload_image.py"

if [[ ! -f "$UPLOAD_PY" ]]; then
  echo "ERROR: Missing $UPLOAD_PY"
  exit 2
fi

if [[ ! -f "$INPUT_IMAGE" ]]; then
  echo "ERROR: Image not found: $INPUT_IMAGE"
  exit 2
fi

BASE_URL="http://$ESP32_IP"

pause() {
  echo
  read -r -p "Press ENTER to continue..." _
}

step() {
  local title="$1"
  echo
  echo "============================================================"
  echo "$title"
  echo "============================================================"
}

require_ok() {
  local exit_code=$?
  if [[ $exit_code -ne 0 ]]; then
    echo "ERROR: Command failed (exit=$exit_code). Stopping."
    exit $exit_code
  fi
}

# Generate a baseline JPEG temp file to make curl testing reliable.
TMP_CURL_JPEG=""
cleanup() {
  if [[ -n "${TMP_CURL_JPEG:-}" && -f "$TMP_CURL_JPEG" ]]; then
    rm -f "$TMP_CURL_JPEG"
  fi
}
trap cleanup EXIT

step "0) Preflight: device health"
set +e
curl -sS "$BASE_URL/api/health" | cat
HEALTH_RC=$?
set -e
if [[ $HEALTH_RC -ne 0 ]]; then
  echo "WARN: /api/health did not respond cleanly. Continuing anyway."
fi

echo
echo "What to check: device reachable on $BASE_URL"
pause

step "1) Single image upload via tools/upload_image.py (--mode single)"
python3 "$UPLOAD_PY" "$ESP32_IP" "$INPUT_IMAGE" --mode single --timeout 4
require_ok

echo
echo "What to check on screen: full image displayed (orientation is exactly as uploaded)."
echo "It should auto-dismiss after ~4 seconds."
pause

step "2) Single image upload via curl (multipart POST /api/display/image)"
# Create baseline JPEG for curl upload (avoids progressive/unsupported variants).
TMP_CURL_JPEG="$TOOLS_DIR/.tmp_curl_baseline.jpg"
python3 - "$INPUT_IMAGE" "$TMP_CURL_JPEG" <<'PY'
import sys
from PIL import Image
src, dst = sys.argv[1], sys.argv[2]
img = Image.open(src)
if img.mode != "RGB":
    img = img.convert("RGB")
img.save(dst, format="JPEG", quality=90, subsampling=2, progressive=False, optimize=False)
print(dst)
PY

curl -sS -f \
  -F "image=@${TMP_CURL_JPEG};type=image/jpeg" \
  "$BASE_URL/api/display/image?timeout=4" \
  | cat
require_ok

echo
echo "What to check on screen: same image as step 1, same orientation."
echo "It should auto-dismiss after ~4 seconds."
pause

step "3) Strip upload via tools/upload_image.py (--mode strip, default strip height)"
python3 "$UPLOAD_PY" "$ESP32_IP" "$INPUT_IMAGE" --mode strip --timeout 4
require_ok

echo
echo "What to check on screen: image draws in strips (top-to-bottom) and ends as a full image."
echo "It should auto-dismiss after ~4 seconds."
pause

step "4) Strip upload via tools/upload_image.py with start/end (partial upload)"
echo "Uploading only strips 0..1 (partial image)"
python3 "$UPLOAD_PY" "$ESP32_IP" "$INPUT_IMAGE" --mode strip --timeout 4 --start 0 --end 1
require_ok

echo
echo "What to check on screen: only the TOP part of the image is drawn (partial image)."
echo "This confirms the chunk endpoint and strip ordering behavior."
pause

step "5) Strip upload with custom strip height (stress request count)"
python3 "$UPLOAD_PY" "$ESP32_IP" "$INPUT_IMAGE" --mode strip --timeout 4 --strip-height 16
require_ok

echo
echo "What to check on screen: image draws with more (smaller) strips; should still complete."
echo "This increases request count and is useful for spotting timing/race issues."
pause

step "6) Permanent display (timeout=0) + manual dismiss (DELETE /api/display/image)"
python3 "$UPLOAD_PY" "$ESP32_IP" "$INPUT_IMAGE" --mode single --timeout 0
require_ok

echo
echo "What to check on screen: image stays displayed (no auto-dismiss)."
pause

echo "Now dismissing via HTTP DELETE..."
curl -sS -f -X DELETE "$BASE_URL/api/display/image" | cat
require_ok

echo
echo "What to check on screen: returns to the power screen (or previous UI) immediately."
pause

echo
echo "All tests completed successfully."
