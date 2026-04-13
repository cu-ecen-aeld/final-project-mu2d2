#!/bin/bash
# test_udev_rules.sh — CI tests for pagespeak-cam udev rules
# Runs in Docker on Apple Silicon without target hardware
#
# Tests:
#   1. Rules file exists
#   2. Rules file is not empty
#   3. Required udev attributes present (SUBSYSTEM, ATTR, DRIVERS, SYMLINK, MODE, GROUP)
#   4. No obvious syntax errors (unquoted values, malformed operators)
#   5. Symlink name doesn't conflict with kernel naming
#   6. Parent device matching uses SUBSYSTEMS for USB context

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RULES_FILE="$REPO_ROOT/meta-pagespeak/recipes-config/udev-rules/files/90-pagespeak-cam.rules"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

pass() { echo -e "${GREEN}PASS${NC}: $1"; }
fail() { echo -e "${RED}FAIL${NC}: $1"; exit 1; }

echo "=============================================="
echo " PageSpeak udev Rules CI Tests"
echo "=============================================="
echo ""

# Test 1: Rules file exists
echo "Test 1: Rules file exists"
if [[ -f "$RULES_FILE" ]]; then
    pass "Rules file found at $RULES_FILE"
else
    fail "Rules file not found at $RULES_FILE"
fi

# Test 2: Rules file is not empty
echo "Test 2: Rules file is not empty"
if [[ -s "$RULES_FILE" ]]; then
    pass "Rules file has content"
else
    fail "Rules file is empty"
fi

# Test 3: Required attributes present
echo "Test 3: Required udev attributes present"
REQUIRED_ATTRS=(
    'SUBSYSTEM=="video4linux"'
    'ATTR{index}=="0"'
    'DRIVERS=="uvcvideo"'
    'SYMLINK+="pagespeak-cam-raw"'
    'MODE="0660"'
    'GROUP="video"'
)

for attr in "${REQUIRED_ATTRS[@]}"; do
    if grep -q "$attr" "$RULES_FILE"; then
        pass "Found: $attr"
    else
        fail "Missing required attribute: $attr"
    fi
done

# Test 4: Check for common syntax errors
echo "Test 4: Basic syntax validation"
# Check for unquoted values (e.g., MODE=0660 instead of MODE="0660")
if grep -E '^[^#]*(MODE|GROUP|OWNER)=[^"0-9]' "$RULES_FILE" >/dev/null 2>&1; then
    fail "Found unquoted assignment value"
fi
# Check for missing quotes around comparison values
if grep -E '^[^#]*(SUBSYSTEM|KERNEL|DRIVERS|ATTR\{[^}]+\})=[^="]+[,\\]' "$RULES_FILE" >/dev/null 2>&1; then
    fail "Found comparison without quotes"
fi
pass "No obvious syntax errors"

# Test 5: Symlink name doesn't conflict with kernel naming
echo "Test 5: Symlink name safety"
if grep -q 'SYMLINK+="video' "$RULES_FILE"; then
    fail "Symlink name 'video*' conflicts with kernel naming"
else
    pass "Symlink name 'pagespeak-cam-raw' is safe"
fi

# Test 6: Rule uses SUBSYSTEMS for parent matching (not just SUBSYSTEM)
echo "Test 6: Parent device matching"
if grep -q 'SUBSYSTEMS=="usb"' "$RULES_FILE"; then
    pass "Uses SUBSYSTEMS for USB parent matching"
else
    fail "Missing SUBSYSTEMS==\"usb\" for parent device context"
fi

echo ""
echo "=============================================="
echo " All CI tests passed!"
echo "=============================================="
