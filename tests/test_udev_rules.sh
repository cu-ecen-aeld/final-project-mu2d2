#!/bin/bash
# test_udev_rules.sh — CI tests for pagespeak-cam udev rules
# Runs in Docker on Apple Silicon without target hardware
#
# Tests:
#   1. Rules file exists
#   2. Rules file is not empty
#   3. Required udev attributes present (SUBSYSTEM, ATTR, DRIVERS, SYMLINK, MODE, GROUP)
#   4. udevadm verify validates syntax (real udev validation)
#   5. Symlink name doesn't conflict with kernel naming
#   6. Parent device matching uses SUBSYSTEMS for USB context

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RULES_FILE="$REPO_ROOT/meta-pagespeak/recipes-config/udev-rules/files/90-pagespeak-cam.rules"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

pass() { echo -e "${GREEN}PASS${NC}: $1"; ((PASS_COUNT++)); }
fail() { echo -e "${RED}FAIL${NC}: $1"; ((FAIL_COUNT++)); }
skip() { echo -e "${YELLOW}SKIP${NC}: $1"; ((SKIP_COUNT++)); }

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
echo ""
echo "Test 2: Rules file is not empty"
if [[ -s "$RULES_FILE" ]]; then
    pass "Rules file has content"
else
    fail "Rules file is empty"
fi

# Test 3: Required attributes present
echo ""
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

# Test 4: Real udevadm validation
echo ""
echo "Test 4: udevadm verify validation"
if command -v udevadm >/dev/null 2>&1; then
    # udevadm verify validates rules syntax (available in systemd 251+)
    if udevadm verify --help >/dev/null 2>&1; then
        # Run udevadm verify directly on the rules file
        VERIFY_OUTPUT=$(udevadm verify "$RULES_FILE" 2>&1)
        VERIFY_EXIT=$?

        if [[ $VERIFY_EXIT -eq 0 ]]; then
            pass "udevadm verify: rules syntax is valid"
        else
            echo "  udevadm output: $VERIFY_OUTPUT"
            fail "udevadm verify: rules syntax errors found"
        fi
    else
        # Fallback for older udevadm without verify subcommand
        skip "udevadm verify not available (requires systemd 251+), using fallback validation"

        echo "Test 4b: Fallback syntax check"
        # Check for common syntax errors with grep
        if grep -E '^[^#]*(MODE|GROUP|OWNER)=[^"0-9]' "$RULES_FILE" >/dev/null 2>&1; then
            fail "Found unquoted assignment value"
        elif grep -E '^[^#]*(SUBSYSTEM|KERNEL|DRIVERS|ATTR\{[^}]+\})=[^="]+[,\\]' "$RULES_FILE" >/dev/null 2>&1; then
            fail "Found comparison without quotes"
        else
            pass "Fallback syntax check: no obvious errors"
        fi
    fi
else
    skip "udevadm not available — skipping real validation"
    # Still do basic grep checks as fallback
    echo "Test 4b: Fallback syntax check (no udevadm)"
    if grep -E '^[^#]*(MODE|GROUP|OWNER)=[^"0-9]' "$RULES_FILE" >/dev/null 2>&1; then
        fail "Found unquoted assignment value"
    elif grep -E '^[^#]*(SUBSYSTEM|KERNEL|DRIVERS|ATTR\{[^}]+\})=[^="]+[,\\]' "$RULES_FILE" >/dev/null 2>&1; then
        fail "Found comparison without quotes"
    else
        pass "Fallback syntax check: no obvious errors"
    fi
fi

# Test 5: Symlink name doesn't conflict with kernel naming
echo ""
echo "Test 5: Symlink name safety"
if grep -q 'SYMLINK+="video' "$RULES_FILE"; then
    fail "Symlink name 'video*' conflicts with kernel naming"
else
    pass "Symlink name 'pagespeak-cam-raw' is safe"
fi

# Test 6: Rule uses SUBSYSTEMS for parent matching (not just SUBSYSTEM)
echo ""
echo "Test 6: Parent device matching"
if grep -q 'SUBSYSTEMS=="usb"' "$RULES_FILE"; then
    pass "Uses SUBSYSTEMS for USB parent matching"
else
    fail "Missing SUBSYSTEMS==\"usb\" for parent device context"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "=============================================="
echo " Test Summary"
echo "=============================================="
echo -e " ${GREEN}PASS${NC}: $PASS_COUNT"
echo -e " ${RED}FAIL${NC}: $FAIL_COUNT"
echo -e " ${YELLOW}SKIP${NC}: $SKIP_COUNT"
echo "=============================================="

if [[ "$FAIL_COUNT" -gt 0 ]]; then
    echo -e " ${RED}RESULT: FAILED${NC} ($FAIL_COUNT test(s) failed)"
    exit 1
else
    echo -e " ${GREEN}RESULT: ALL TESTS PASSED${NC}"
fi
