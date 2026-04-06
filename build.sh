#!/bin/bash
# build.sh — Multi-target build script for PageSpeak
#
# Usage:
#   ./build.sh                    # Build for default machine (rpi3)
#   ./build.sh rpi3               # Build for Raspberry Pi 3
#   ./build.sh rpi4               # Build for Raspberry Pi 4 (32-bit)
#   ./build.sh rpi4-64            # Build for Raspberry Pi 4 (64-bit)
#   ./build.sh rpi5               # Build for Raspberry Pi 5
#   ./build.sh all                # Build for all supported targets
#   ./build.sh <machine> <recipe> # Build specific recipe (e.g., ./build.sh rpi3 pagespeak-cam-driver)
#
# Environment variables:
#   DOCKER=0                      # Set to 0 to build natively (Linux only)
#   BB_NUMBER_THREADS=8           # Override parallel BitBake tasks
#   PARALLEL_MAKE="-j 8"          # Override parallel make jobs

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Help message
usage() {
    cat <<EOF
PageSpeak Multi-Target Build Script

Usage: ./build.sh [target] [recipe]

Targets:
  rpi3      Raspberry Pi 3B/3B+ (32-bit ARM, kernel 5.15)
  rpi4      Raspberry Pi 4B (32-bit ARM, kernel 5.15)
  rpi4-64   Raspberry Pi 4B (64-bit ARM, kernel 5.15)
  rpi5      Raspberry Pi 5 (64-bit ARM, kernel 6.1)
  all       Build for rpi3, rpi4, and rpi5

Recipes (optional, defaults to pagespeak-image):
  pagespeak-image         Full bootable image with all modules
  pagespeak-cam-driver    Camera capture kernel module only
  pagespeak-btn           GPIO button kernel module only

Examples:
  ./build.sh                      # Build image for rpi3 (default)
  ./build.sh rpi5                 # Build image for Raspberry Pi 5
  ./build.sh rpi3 pagespeak-cam-driver  # Build only the camera driver for rpi3
  ./build.sh all                  # Build images for all targets

Environment:
  DOCKER=0              Build natively instead of in Docker (Linux x86_64 only)
  BB_NUMBER_THREADS=N   Number of parallel BitBake tasks (default: 8)

Output images are placed in:
  build/tmp/deploy/images/<machine>/pagespeak-image-<machine>.rpi-sdimg
EOF
    exit 0
}

# Check for help flag early (before any other processing)
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
fi

# Default settings
DOCKER="${DOCKER:-1}"
BB_NUMBER_THREADS="${BB_NUMBER_THREADS:-8}"
PARALLEL_MAKE="${PARALLEL_MAKE:--j 8}"
DEFAULT_RECIPE="pagespeak-image"

# Machine name mapping (function to avoid bash 4 associative array requirement)
get_machine() {
    case "$1" in
        rpi3)    echo "raspberrypi3" ;;
        rpi4)    echo "raspberrypi4" ;;
        rpi4-64) echo "raspberrypi4-64" ;;
        rpi5)    echo "raspberrypi5" ;;
        *)       echo "" ;;
    esac
}

# All supported targets for 'all' build
ALL_TARGETS="rpi3 rpi4 rpi5"

# Parse arguments
TARGET="${1:-rpi3}"
RECIPE="${2:-$DEFAULT_RECIPE}"

# Validate target
if [[ "$TARGET" != "all" ]]; then
    MACHINE=$(get_machine "$TARGET")
    if [[ -z "$MACHINE" ]]; then
        echo "Error: Unknown target '$TARGET'"
        echo "Valid targets: rpi3 rpi4 rpi4-64 rpi5 all"
        exit 1
    fi
fi

# Build function for a single target
build_target() {
    local target="$1"
    local recipe="$2"
    local machine
    machine=$(get_machine "$target")

    echo ""
    echo "=============================================="
    echo " Building $recipe for $target ($machine)"
    echo "=============================================="
    echo ""

    if [[ "$DOCKER" == "1" ]]; then
        # Docker build
        docker run --rm \
            -v "$SCRIPT_DIR":/workdir \
            -v yocto-downloads:/yocto-cache/downloads \
            -v yocto-sstate:/yocto-cache/sstate-cache \
            -v yocto-tmp:/yocto-cache/tmp \
            -e MACHINE="$machine" \
            yocto-arm64:latest \
            bash -c "
                cd /workdir

                set +u
                source sources/poky/oe-init-build-env build >/dev/null 2>&1
                set -u

                # Copy base config
                cp /workdir/conf/local.conf conf/local.conf

                # Append Docker-specific settings
                cat >> conf/local.conf << CONF

# Docker cache volumes
DL_DIR = \"/yocto-cache/downloads\"
SSTATE_DIR = \"/yocto-cache/sstate-cache\"
TMPDIR = \"/yocto-cache/tmp\"

# Parallel build settings
BB_NUMBER_THREADS = \"$BB_NUMBER_THREADS\"
PARALLEL_MAKE = \"$PARALLEL_MAKE\"

# Target machine (from build.sh)
MACHINE = \"$machine\"
CONF

                # Generate bblayers.conf
                sed \\
                    -e 's|##POKY_DIR##|/workdir/sources/poky|g' \\
                    -e 's|##OE_DIR##|/workdir/sources/meta-openembedded|g' \\
                    -e 's|##META_RPI_DIR##|/workdir/sources/meta-raspberrypi|g' \\
                    -e 's|##PROJECT_DIR##|/workdir|g' \\
                    /workdir/conf/bblayers.conf.sample > conf/bblayers.conf

                echo \"Building $recipe for MACHINE=$machine\"
                bitbake $recipe
            "
    else
        # Native build (Linux only)
        (
            set +u
            # shellcheck disable=SC1091
            source sources/poky/oe-init-build-env build >/dev/null 2>&1
            set -u

            cp "$SCRIPT_DIR/conf/local.conf" conf/local.conf

            # Append machine override
            echo "" >> conf/local.conf
            echo "MACHINE = \"$machine\"" >> conf/local.conf

            # Generate bblayers.conf
            sed \
                -e "s|##POKY_DIR##|$SCRIPT_DIR/sources/poky|g" \
                -e "s|##OE_DIR##|$SCRIPT_DIR/sources/meta-openembedded|g" \
                -e "s|##META_RPI_DIR##|$SCRIPT_DIR/sources/meta-raspberrypi|g" \
                -e "s|##PROJECT_DIR##|$SCRIPT_DIR|g" \
                "$SCRIPT_DIR/conf/bblayers.conf.sample" > conf/bblayers.conf

            MACHINE="$machine" bitbake "$recipe"
        )
    fi

    echo ""
    echo "Build complete: $target ($machine) - $recipe"
}

# Main execution
if [[ "$TARGET" == "all" ]]; then
    echo "Building for all targets: $ALL_TARGETS"
    for t in $ALL_TARGETS; do
        build_target "$t" "$RECIPE"
    done
    echo ""
    echo "=============================================="
    echo " All builds complete!"
    echo "=============================================="
    echo ""
    echo "Output images:"
    for t in $ALL_TARGETS; do
        m=$(get_machine "$t")
        echo "  $t: build/tmp/deploy/images/$m/pagespeak-image-$m.rpi-sdimg"
    done
else
    build_target "$TARGET" "$RECIPE"
    MACHINE=$(get_machine "$TARGET")
    echo ""
    echo "Output: build/tmp/deploy/images/$MACHINE/$RECIPE-$MACHINE.rpi-sdimg"
fi
