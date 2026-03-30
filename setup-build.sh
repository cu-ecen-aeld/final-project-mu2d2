#!/bin/bash
# setup-build.sh — Initialize the PageSpeak Yocto build environment
# Safe to re-run (idempotent). Clones dependencies, configures build dir.
#
# Usage:
#   ./setup-build.sh              # Use default build directory
#   ./setup-build.sh /path/to/build  # Use custom build directory

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YOCTO_RELEASE="kirkstone"

# Build directory — default to 'build' alongside the project
BUILD_DIR="${1:-${SCRIPT_DIR}/build}"

# Dependency directories — siblings of the project directory
DEPS_DIR="${SCRIPT_DIR}/sources"
POKY_DIR="${DEPS_DIR}/poky"
META_RPI_DIR="${DEPS_DIR}/meta-raspberrypi"
OE_DIR="${DEPS_DIR}/meta-openembedded"

echo "=== PageSpeak Yocto Build Setup ==="
echo "Project dir:  ${SCRIPT_DIR}"
echo "Sources dir:  ${DEPS_DIR}"
echo "Build dir:    ${BUILD_DIR}"
echo "Yocto release: ${YOCTO_RELEASE}"
echo ""

# --- Clone / update dependencies ---
mkdir -p "${DEPS_DIR}"

clone_or_update() {
    local repo_url="$1"
    local target_dir="$2"
    local branch="$3"
    local name
    name="$(basename "$target_dir")"

    if [ -d "${target_dir}" ]; then
        echo "[OK] ${name} already cloned (${target_dir})"
    else
        echo "[CLONE] Cloning ${name} (branch: ${branch})..."
        git clone -b "${branch}" --single-branch "${repo_url}" "${target_dir}"
        echo "[OK] ${name} cloned."
    fi
}

clone_or_update "https://git.yoctoproject.org/poky" "${POKY_DIR}" "${YOCTO_RELEASE}"
clone_or_update "https://github.com/agherzan/meta-raspberrypi.git" "${META_RPI_DIR}" "${YOCTO_RELEASE}"
clone_or_update "https://github.com/openembedded/meta-openembedded.git" "${OE_DIR}" "${YOCTO_RELEASE}"

# --- Source the Yocto build environment ---
# Note: oe-init-build-env changes the working directory to BUILD_DIR.
# All paths above use absolute paths so this is safe.
echo ""
echo "[SETUP] Sourcing oe-init-build-env..."
# shellcheck disable=SC1091
source "${POKY_DIR}/oe-init-build-env" "${BUILD_DIR}"

# --- Copy local.conf ---
if [ ! -f "${BUILD_DIR}/conf/local.conf.orig" ] && [ -f "${BUILD_DIR}/conf/local.conf" ]; then
    # Back up the auto-generated local.conf on first run
    cp "${BUILD_DIR}/conf/local.conf" "${BUILD_DIR}/conf/local.conf.orig"
fi
echo "[CONF] Copying local.conf..."
cp "${SCRIPT_DIR}/conf/local.conf" "${BUILD_DIR}/conf/local.conf"

# --- Generate bblayers.conf from template ---
echo "[CONF] Generating bblayers.conf..."
sed \
    -e "s|##POKY_DIR##|${POKY_DIR}|g" \
    -e "s|##OE_DIR##|${OE_DIR}|g" \
    -e "s|##META_RPI_DIR##|${META_RPI_DIR}|g" \
    -e "s|##PROJECT_DIR##|${SCRIPT_DIR}|g" \
    "${SCRIPT_DIR}/conf/bblayers.conf.sample" \
    > "${BUILD_DIR}/conf/bblayers.conf"

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Build directory: ${BUILD_DIR}"
echo "Current MACHINE: $(grep '^MACHINE' "${BUILD_DIR}/conf/local.conf" | head -1)"
echo ""
echo "To build the image:"
echo "  bitbake pagespeak-image"
echo ""
echo "To switch target machine, edit conf/local.conf in the repo root"
echo "and re-run this script, or edit ${BUILD_DIR}/conf/local.conf directly."
echo ""
echo "NOTE: First build will take several hours and ~50GB of disk space."
