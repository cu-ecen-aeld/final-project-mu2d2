#!/bin/bash
# test_pagespeak_daemon.sh — CI tests for pagespeak-daemon (TDD)
# Runs in Docker on Apple Silicon without target hardware
#
# Tests:
#   1.  Recipe file exists
#   2.  All required source files exist
#   3.  capture.h has required interface
#   4.  preprocess.h has required interface
#   5.  ocr.h has required interface
#   6.  tts.h has required interface
#   7.  btn_event struct in main.c has count and timestamp_ns fields
#   8.  Source files compile (syntax check with gcc -fsyntax-only)
#   9.  systemd service file has required sections and ExecStart=
#   9b. systemd service dependencies configuration
#   9c. systemd service restart policy
#   9d. systemd service install target
#   9e. systemd service logging configuration
#   10. Recipe SRC_URI references all required files
#   11. pagespeak-daemon added to pagespeak-image.bb
#   12. Recipe systemd integration (inherit, SYSTEMD_SERVICE, SYSTEMD_AUTO_ENABLE)

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RECIPE_FILE="$REPO_ROOT/meta-pagespeak/recipes-app/pagespeak-daemon/pagespeak-daemon.bb"
SRC_DIR="$REPO_ROOT/meta-pagespeak/recipes-app/pagespeak-daemon/files"
IMAGE_FILE="$REPO_ROOT/meta-pagespeak/recipes-core/images/pagespeak-image.bb"

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
echo " PageSpeak Daemon CI Tests"
echo "=============================================="
echo ""

# ---------------------------------------------------------------------------
# Test 1: Recipe file exists
# ---------------------------------------------------------------------------
echo "Test 1: Recipe file exists"
if [[ -f "$RECIPE_FILE" ]]; then
    pass "Recipe found at $RECIPE_FILE"
else
    fail "Recipe not found at $RECIPE_FILE"
fi

# ---------------------------------------------------------------------------
# Test 2: All required source files exist
# ---------------------------------------------------------------------------
echo ""
echo "Test 2: Required source files exist"
REQUIRED_SOURCES=(
    "main.c"
    "capture.c"
    "capture.h"
    "preprocess.h"
    "preprocess_stub.c"
    "ocr.h"
    "ocr_stub.c"
    "tts.h"
    "tts_stub.c"
    "pagespeak-daemon.service"
)

ALL_SOURCES_PRESENT=true
for src in "${REQUIRED_SOURCES[@]}"; do
    if [[ -f "$SRC_DIR/$src" ]]; then
        pass "Found: $src"
    else
        fail "Missing: $SRC_DIR/$src"
        ALL_SOURCES_PRESENT=false
    fi
done

# ---------------------------------------------------------------------------
# Test 3: capture.h required interface
# ---------------------------------------------------------------------------
echo ""
echo "Test 3: capture.h has required interface"
CAPTURE_H="$SRC_DIR/capture.h"
if [[ -f "$CAPTURE_H" ]]; then
    CAPTURE_FUNCS=("capture_open" "capture_frame" "capture_free" "capture_close")
    for fn in "${CAPTURE_FUNCS[@]}"; do
        if grep -q "$fn" "$CAPTURE_H"; then
            pass "capture.h declares: $fn"
        else
            fail "capture.h missing declaration: $fn"
        fi
    done
else
    skip "capture.h not found — skipping interface checks"
fi

# ---------------------------------------------------------------------------
# Test 4: preprocess.h required interface
# ---------------------------------------------------------------------------
echo ""
echo "Test 4: preprocess.h has required interface"
PREPROCESS_H="$SRC_DIR/preprocess.h"
if [[ -f "$PREPROCESS_H" ]]; then
    PREPROCESS_FUNCS=("preprocess_image" "preprocess_free")
    for fn in "${PREPROCESS_FUNCS[@]}"; do
        if grep -q "$fn" "$PREPROCESS_H"; then
            pass "preprocess.h declares: $fn"
        else
            fail "preprocess.h missing declaration: $fn"
        fi
    done
else
    skip "preprocess.h not found — skipping interface checks"
fi

# ---------------------------------------------------------------------------
# Test 5: ocr.h required interface
# ---------------------------------------------------------------------------
echo ""
echo "Test 5: ocr.h has required interface"
OCR_H="$SRC_DIR/ocr.h"
if [[ -f "$OCR_H" ]]; then
    OCR_FUNCS=("ocr_init" "ocr_extract" "ocr_free_text" "ocr_cleanup")
    for fn in "${OCR_FUNCS[@]}"; do
        if grep -q "$fn" "$OCR_H"; then
            pass "ocr.h declares: $fn"
        else
            fail "ocr.h missing declaration: $fn"
        fi
    done
else
    skip "ocr.h not found — skipping interface checks"
fi

# ---------------------------------------------------------------------------
# Test 6: tts.h required interface
# ---------------------------------------------------------------------------
echo ""
echo "Test 6: tts.h has required interface"
TTS_H="$SRC_DIR/tts.h"
if [[ -f "$TTS_H" ]]; then
    TTS_FUNCS=("tts_init" "tts_speak" "tts_cleanup")
    for fn in "${TTS_FUNCS[@]}"; do
        if grep -q "$fn" "$TTS_H"; then
            pass "tts.h declares: $fn"
        else
            fail "tts.h missing declaration: $fn"
        fi
    done
else
    skip "tts.h not found — skipping interface checks"
fi

# ---------------------------------------------------------------------------
# Test 7: btn_event struct in main.c has count and timestamp_ns fields
# ---------------------------------------------------------------------------
echo ""
echo "Test 7: btn_event struct matches kernel driver ABI"
MAIN_C="$SRC_DIR/main.c"
if [[ -f "$MAIN_C" ]]; then
    BTN_FIELDS=("count" "timestamp_ns")
    for field in "${BTN_FIELDS[@]}"; do
        if grep -q "$field" "$MAIN_C"; then
            pass "main.c references btn_event field: $field"
        else
            fail "main.c missing btn_event field: $field"
        fi
    done
else
    skip "main.c not found — skipping btn_event struct check"
fi

# ---------------------------------------------------------------------------
# Test 8: Source files compile (gcc -fsyntax-only)
# ---------------------------------------------------------------------------
echo ""
echo "Test 8: Source files pass gcc -fsyntax-only"
if command -v gcc >/dev/null 2>&1; then
    if [[ "$ALL_SOURCES_PRESENT" == "true" ]]; then
        SYNTAX_FAILED=false
        C_SOURCES=("main.c" "capture.c" "preprocess_stub.c" "ocr_stub.c" "tts_stub.c")
        for src in "${C_SOURCES[@]}"; do
            if gcc -fsyntax-only -I"$SRC_DIR" "$SRC_DIR/$src" 2>/dev/null; then
                pass "Syntax OK: $src"
            else
                # Print errors to help diagnose failures
                gcc -fsyntax-only -I"$SRC_DIR" "$SRC_DIR/$src" 2>&1 | head -20 || true
                fail "Syntax error in: $src"
                SYNTAX_FAILED=true
            fi
        done
    else
        skip "Source files missing — skipping syntax check"
    fi
else
    skip "gcc not available — skipping syntax check"
fi

# ---------------------------------------------------------------------------
# Test 9: systemd service file has required sections and ExecStart=
# ---------------------------------------------------------------------------
echo ""
echo "Test 9: systemd service file structure"
SERVICE_FILE="$SRC_DIR/pagespeak-daemon.service"
if [[ -f "$SERVICE_FILE" ]]; then
    SERVICE_SECTIONS=("[Unit]" "[Service]" "[Install]")
    for section in "${SERVICE_SECTIONS[@]}"; do
        if grep -qF "$section" "$SERVICE_FILE"; then
            pass "Service file has section: $section"
        else
            fail "Service file missing section: $section"
        fi
    done
    if grep -q "^ExecStart=" "$SERVICE_FILE"; then
        pass "Service file has ExecStart="
    else
        fail "Service file missing ExecStart="
    fi
else
    skip "pagespeak-daemon.service not found — skipping service checks"
fi

# ---------------------------------------------------------------------------
# Test 9b: systemd service dependencies configuration
# ---------------------------------------------------------------------------
echo ""
echo "Test 9b: systemd service dependencies"
SERVICE_FILE="$SRC_DIR/pagespeak-daemon.service"
if [[ -f "$SERVICE_FILE" ]]; then
    # After= must include local-fs.target
    if grep -q "^After=.*local-fs.target" "$SERVICE_FILE"; then
        pass "Service waits for local-fs.target"
    else
        fail "Service missing After=local-fs.target"
    fi

    # Check for device dependencies (button and camera)
    if grep -q "dev-pagespeak" "$SERVICE_FILE"; then
        pass "Service has device node dependencies"
    else
        fail "Service missing device node dependencies (dev-pagespeak-*)"
    fi
else
    skip "pagespeak-daemon.service not found"
fi

# ---------------------------------------------------------------------------
# Test 9c: systemd service restart policy
# ---------------------------------------------------------------------------
echo ""
echo "Test 9c: systemd service restart policy"
SERVICE_FILE="$SRC_DIR/pagespeak-daemon.service"
if [[ -f "$SERVICE_FILE" ]]; then
    if grep -q "^Restart=on-failure" "$SERVICE_FILE"; then
        pass "Service has Restart=on-failure"
    else
        fail "Service missing Restart=on-failure"
    fi

    if grep -q "^RestartSec=" "$SERVICE_FILE"; then
        pass "Service has RestartSec= configured"
    else
        fail "Service missing RestartSec= (restart delay)"
    fi
else
    skip "pagespeak-daemon.service not found"
fi

# ---------------------------------------------------------------------------
# Test 9d: systemd service install target
# ---------------------------------------------------------------------------
echo ""
echo "Test 9d: systemd service install target"
SERVICE_FILE="$SRC_DIR/pagespeak-daemon.service"
if [[ -f "$SERVICE_FILE" ]]; then
    if grep -q "^WantedBy=multi-user.target" "$SERVICE_FILE"; then
        pass "Service has WantedBy=multi-user.target"
    else
        fail "Service missing WantedBy=multi-user.target (required for boot start)"
    fi
else
    skip "pagespeak-daemon.service not found"
fi

# ---------------------------------------------------------------------------
# Test 9e: systemd service logging configuration
# ---------------------------------------------------------------------------
echo ""
echo "Test 9e: systemd service logging configuration"
SERVICE_FILE="$SRC_DIR/pagespeak-daemon.service"
if [[ -f "$SERVICE_FILE" ]]; then
    if grep -q "^StandardOutput=journal" "$SERVICE_FILE"; then
        pass "Service stdout goes to journal"
    else
        fail "Service missing StandardOutput=journal"
    fi

    if grep -q "^StandardError=journal" "$SERVICE_FILE"; then
        pass "Service stderr goes to journal"
    else
        fail "Service missing StandardError=journal"
    fi
else
    skip "pagespeak-daemon.service not found"
fi

# ---------------------------------------------------------------------------
# Test 10: Recipe SRC_URI references all required files
# ---------------------------------------------------------------------------
echo ""
echo "Test 10: Recipe SRC_URI references all required files"
if [[ -f "$RECIPE_FILE" ]]; then
    RECIPE_SOURCES=(
        "main.c"
        "capture.c"
        "capture.h"
        "preprocess.h"
        "preprocess_stub.c"
        "ocr.h"
        "ocr_stub.c"
        "tts.h"
        "tts_stub.c"
        "pagespeak-daemon.service"
    )
    for src in "${RECIPE_SOURCES[@]}"; do
        if grep -q "$src" "$RECIPE_FILE"; then
            pass "SRC_URI references: $src"
        else
            fail "SRC_URI missing reference to: $src"
        fi
    done
else
    skip "Recipe file not found — skipping SRC_URI checks"
fi

# ---------------------------------------------------------------------------
# Test 11: pagespeak-daemon added to pagespeak-image.bb
# ---------------------------------------------------------------------------
echo ""
echo "Test 11: pagespeak-daemon included in image recipe"
if [[ -f "$IMAGE_FILE" ]]; then
    if grep -q "pagespeak-daemon" "$IMAGE_FILE"; then
        pass "pagespeak-daemon found in pagespeak-image.bb"
    else
        fail "pagespeak-daemon not found in $IMAGE_FILE"
    fi
else
    fail "Image recipe not found at $IMAGE_FILE"
fi

# ---------------------------------------------------------------------------
# Test 12: Recipe systemd integration
# ---------------------------------------------------------------------------
echo ""
echo "Test 12: Recipe systemd integration"
if [[ -f "$RECIPE_FILE" ]]; then
    if grep -q "^inherit systemd" "$RECIPE_FILE"; then
        pass "Recipe inherits systemd class"
    else
        fail "Recipe missing 'inherit systemd'"
    fi

    if grep -q 'SYSTEMD_SERVICE:\${PN}.*=.*pagespeak-daemon.service' "$RECIPE_FILE"; then
        pass "Recipe defines SYSTEMD_SERVICE"
    else
        fail "Recipe missing SYSTEMD_SERVICE definition"
    fi

    if grep -q 'SYSTEMD_AUTO_ENABLE:\${PN}.*=.*"enable"' "$RECIPE_FILE"; then
        pass "Recipe enables service at boot (SYSTEMD_AUTO_ENABLE)"
    else
        fail "Recipe missing SYSTEMD_AUTO_ENABLE = enable"
    fi
else
    skip "Recipe file not found"
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
