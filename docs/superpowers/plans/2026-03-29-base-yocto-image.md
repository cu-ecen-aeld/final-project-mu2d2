# PageSpeak Base Yocto Image — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a bootable Yocto Linux image for Raspberry Pi 3B/5 with V4L2 camera, ALSA audio, and GPIO support — the foundation for the PageSpeak OCR-to-speech device.

**Architecture:** Custom Yocto layer (`meta-pagespeak`) on top of poky + meta-raspberrypi + meta-openembedded (Kirkstone LTS). A single image recipe (`pagespeak-image.bb`) extends `core-image-base` with the required packages. A kernel config fragment adds any missing V4L2/GPIO options. A setup script automates cloning dependencies and configuring the build.

**Tech Stack:** Yocto Project Kirkstone (4.0 LTS), BitBake, meta-raspberrypi BSP, meta-openembedded, Bash

---

## Important Context

- **Target repo:** `cu-ecen-aeld/final-project-mu2d2` — already cloned to `/Users/jsnapoli1/Documents/sp26/ecen5713/final-project-mu2d2/`
- **Current state:** Repo has only `readme.md`. Everything below is new.
- **RPi 5 in Kirkstone:** meta-raspberrypi Kirkstone branch does include `raspberrypi5` MACHINE. However, RPi 5 boot support may be limited (U-Boot issues). The plan defaults to `raspberrypi3` and documents RPi 5 as secondary target.
- **Kirkstone branch names:** `kirkstone` for poky, meta-raspberrypi, and meta-openembedded.
- **Default IMAGE_FSTYPES:** meta-raspberrypi defaults to `tar.bz2 ext3 rpi-sdimg`. We use `rpi-sdimg` for the flashable SD card image.
- **macOS builds:** Yocto cannot build natively on macOS. Must use CROPS Docker container (`crops/poky`).
- **What NOT to build:** No OpenCV, Tesseract, espeak-ng, no custom V4L2 module, no application code, no systemd services.

## File Structure

All paths relative to repo root (`final-project-mu2d2/`):

| File | Purpose |
|------|---------|
| `meta-pagespeak/conf/layer.conf` | Layer configuration — BBPATH, BBFILES, collection name, priority, compat |
| `meta-pagespeak/recipes-core/images/pagespeak-image.bb` | Image recipe — extends core-image-base, adds all required packages |
| `meta-pagespeak/recipes-kernel/linux/linux-raspberrypi_%.bbappend` | Kernel bbappend — applies config fragment |
| `meta-pagespeak/recipes-kernel/linux/files/pagespeak.cfg` | Kernel config fragment — V4L2 + GPIO options not in defconfig |
| `meta-pagespeak/recipes-kernel/linux/files/README.md` | Documents what the config fragment enables and why |
| `conf/local.conf` | Build configuration — MACHINE, DISTRO, caching dirs, RPi-specific vars |
| `conf/bblayers.conf.sample` | Template bblayers.conf — setup script copies this with correct paths |
| `setup-build.sh` | Build environment setup — clones deps, sources oe-init-build-env, configures |
| `README.md` | Full documentation — prerequisites, quick start, flashing, verification |

Placeholder directories (empty with `.gitkeep`):
- `meta-pagespeak/recipes-app/`
- `meta-pagespeak/recipes-config/udev-rules/`
- `meta-pagespeak/recipes-config/systemd-services/`
- `meta-pagespeak/recipes-support/`

---

## Task 1: Create the `meta-pagespeak` layer skeleton

**Files:**
- Create: `meta-pagespeak/conf/layer.conf`
- Create: `meta-pagespeak/recipes-app/.gitkeep`
- Create: `meta-pagespeak/recipes-config/udev-rules/.gitkeep`
- Create: `meta-pagespeak/recipes-config/systemd-services/.gitkeep`
- Create: `meta-pagespeak/recipes-support/.gitkeep`
- Create: `meta-pagespeak/recipes-kernel/.gitkeep`
- Create: `meta-pagespeak/recipes-core/.gitkeep`

- [ ] **Step 1: Create directory structure**

```bash
cd /Users/jsnapoli1/Documents/sp26/ecen5713/final-project-mu2d2
mkdir -p meta-pagespeak/conf
mkdir -p meta-pagespeak/recipes-app
mkdir -p meta-pagespeak/recipes-config/udev-rules
mkdir -p meta-pagespeak/recipes-config/systemd-services
mkdir -p meta-pagespeak/recipes-support
mkdir -p meta-pagespeak/recipes-kernel
mkdir -p meta-pagespeak/recipes-core/images
```

- [ ] **Step 2: Create `meta-pagespeak/conf/layer.conf`**

```bitbake
# Layer configuration for meta-pagespeak
# PageSpeak: portable OCR-to-speech device for visually impaired users

BBPATH .= ":${LAYERDIR}"

BBFILES += " \
    ${LAYERDIR}/recipes-*/*/*.bb \
    ${LAYERDIR}/recipes-*/*/*.bbappend \
"

BBFILE_COLLECTIONS += "meta-pagespeak"
BBFILE_PATTERN_meta-pagespeak = "^${LAYERDIR}/"
BBFILE_PRIORITY_meta-pagespeak = "10"

LAYERDEPENDS_meta-pagespeak = "core raspberrypi openembedded-layer"
LAYERSERIES_COMPAT_meta-pagespeak = "kirkstone"
```

- [ ] **Step 3: Create `.gitkeep` files for empty placeholder directories**

```bash
touch meta-pagespeak/recipes-app/.gitkeep
touch meta-pagespeak/recipes-config/udev-rules/.gitkeep
touch meta-pagespeak/recipes-config/systemd-services/.gitkeep
touch meta-pagespeak/recipes-support/.gitkeep
```

- [ ] **Step 4: Verify directory structure looks correct**

Run: `find meta-pagespeak -type f | sort`

Expected output:
```
meta-pagespeak/conf/layer.conf
meta-pagespeak/recipes-app/.gitkeep
meta-pagespeak/recipes-config/systemd-services/.gitkeep
meta-pagespeak/recipes-config/udev-rules/.gitkeep
meta-pagespeak/recipes-support/.gitkeep
```

- [ ] **Step 5: Commit**

```bash
git add meta-pagespeak/
git commit -m "feat: add meta-pagespeak layer skeleton with layer.conf

Custom Yocto layer for the PageSpeak OCR-to-speech device.
Includes placeholder directories for future recipes and
layer configuration targeting Kirkstone."
```

---

## Task 2: Create the image recipe

**Files:**
- Create: `meta-pagespeak/recipes-core/images/pagespeak-image.bb`

- [ ] **Step 1: Create `meta-pagespeak/recipes-core/images/pagespeak-image.bb`**

```bitbake
SUMMARY = "PageSpeak base image"
DESCRIPTION = "Bootable image for the PageSpeak OCR-to-speech device. \
Includes V4L2 camera support, ALSA audio, GPIO tools, and basic debug utilities."
LICENSE = "MIT"

inherit core-image

# V4L2 camera debugging
IMAGE_INSTALL:append = " v4l-utils"

# ALSA audio utilities
IMAGE_INSTALL:append = " alsa-utils"

# GPIO userspace tools
IMAGE_INSTALL:append = " libgpiod libgpiod-tools"

# Networking and debug
IMAGE_INSTALL:append = " openssh-sftp-server nano"

# Ensure all built kernel modules are installed
IMAGE_INSTALL:append = " kernel-modules"

# Development extras
EXTRA_IMAGE_FEATURES += "debug-tweaks ssh-server-dropbear"
```

Note: We use `inherit core-image` which pulls in `core-image-base` functionality. The `debug-tweaks` feature enables root login without password. `ssh-server-dropbear` gives us a lightweight SSH server for headless access.

- [ ] **Step 2: Verify the recipe file exists and has correct syntax**

Run: `cat meta-pagespeak/recipes-core/images/pagespeak-image.bb`

Expected: File contents as written above.

- [ ] **Step 3: Commit**

```bash
git add meta-pagespeak/recipes-core/images/pagespeak-image.bb
git commit -m "feat: add pagespeak-image recipe

Extends core-image with V4L2 utils, ALSA utils, libgpiod tools,
SSH server, and kernel modules for the PageSpeak device."
```

---

## Task 3: Create the kernel config fragment

**Files:**
- Create: `meta-pagespeak/recipes-kernel/linux/linux-raspberrypi_%.bbappend`
- Create: `meta-pagespeak/recipes-kernel/linux/files/pagespeak.cfg`

**Context:** The meta-raspberrypi defconfig already enables most ALSA and basic V4L2 support. However, `CONFIG_GPIO_CDEV` (GPIO character device) and some UVC-related options may need explicit enabling. The config fragment approach adds only what's missing without forking the full defconfig.

- [ ] **Step 1: Create the kernel config fragment file**

Create `meta-pagespeak/recipes-kernel/linux/files/pagespeak.cfg`:

```cfg
# PageSpeak kernel config fragment
# Adds V4L2 UVC camera, GPIO character device, and USB audio support
# on top of the meta-raspberrypi defconfig.

# V4L2 UVC webcam support
CONFIG_MEDIA_SUPPORT=y
CONFIG_MEDIA_USB_SUPPORT=y
CONFIG_USB_VIDEO_CLASS=m
CONFIG_VIDEO_V4L2=y

# GPIO character device interface (for libgpiod)
CONFIG_GPIO_CDEV=y
CONFIG_GPIO_CDEV_V1=y

# USB audio (for USB speakers/microphones)
CONFIG_SND_USB_AUDIO=m
```

- [ ] **Step 2: Create the kernel bbappend**

Create `meta-pagespeak/recipes-kernel/linux/linux-raspberrypi_%.bbappend`:

```bitbake
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://pagespeak.cfg"

# Autoload UVC and USB audio modules at boot.
# uvcvideo transitively loads videobuf2-core, videobuf2-v4l2,
# videobuf2-vmalloc, and videodev — no need to list them separately.
KERNEL_MODULE_AUTOLOAD += "uvcvideo snd-usb-audio"
```

- [ ] **Step 3: Create the directory structure**

```bash
mkdir -p meta-pagespeak/recipes-kernel/linux/files
```

Then write both files.

- [ ] **Step 4: Verify files**

Run: `find meta-pagespeak/recipes-kernel -type f | sort`

Expected:
```
meta-pagespeak/recipes-kernel/linux/files/pagespeak.cfg
meta-pagespeak/recipes-kernel/linux/linux-raspberrypi_%.bbappend
```

- [ ] **Step 5: Commit**

```bash
git add meta-pagespeak/recipes-kernel/
git commit -m "feat: add kernel config fragment for V4L2, GPIO, USB audio

Kernel .cfg fragment enables UVC webcam, GPIO character device
interface, and USB audio modules. bbappend autoloads uvcvideo
and snd-usb-audio at boot."
```

---

## Task 4: Create `conf/local.conf`

**Files:**
- Create: `conf/local.conf`

- [ ] **Step 1: Create `conf/local.conf`**

```bash
mkdir -p conf
```

Create `conf/local.conf`:

```bitbake
# PageSpeak Yocto Build Configuration
# ====================================
# This file is copied into the build directory by setup-build.sh.
# Edit this file (not the copy in build/conf/) for persistent changes.

# --- Machine Selection ---
# Default: Raspberry Pi 3B
# To build for Raspberry Pi 5, change to: MACHINE = "raspberrypi5"
MACHINE = "raspberrypi3"

# --- Distribution ---
DISTRO = "poky"

# --- Package format ---
PACKAGE_CLASSES = "package_rpm"

# --- Raspberry Pi specific settings ---
# Enable UART serial console (required for RPi 3+ headless debugging)
ENABLE_UART = "1"

# GPU memory split — 128MB for camera/video use
GPU_MEM = "128"

# Enable V4L2 camera support in meta-raspberrypi
VIDEO_CAMERA = "1"

# --- Image output format ---
# rpi-sdimg produces a flashable SD card image
IMAGE_FSTYPES:append = " rpi-sdimg"

# --- Development features ---
# debug-tweaks: root login without password
EXTRA_IMAGE_FEATURES += "debug-tweaks"

# --- Build directories ---
# These are relative to the build directory by default.
# For Docker/container builds, set absolute paths to volumes
# mounted from the host to persist downloads and sstate cache.
#
# Example for Docker:
#   DL_DIR = "/yocto-cache/downloads"
#   SSTATE_DIR = "/yocto-cache/sstate-cache"
#   TMPDIR = "/yocto-cache/tmp"
#
# For native Linux builds, defaults are fine (${TOPDIR}/downloads, etc.)

# --- Parallel build settings ---
# Adjust based on your machine. For Docker, leave some headroom.
# BB_NUMBER_THREADS = "8"
# PARALLEL_MAKE = "-j 8"

# --- License handling ---
LICENSE_FLAGS_ACCEPTED = "commercial"
```

- [ ] **Step 2: Verify file contents**

Run: `cat conf/local.conf | head -10`

Expected: First 10 lines of the file as written.

- [ ] **Step 3: Commit**

```bash
git add conf/local.conf
git commit -m "feat: add local.conf for Yocto build configuration

Configures RPi 3 as default MACHINE (RPi 5 documented as switch),
enables UART, GPU_MEM=128, V4L2 camera support, rpi-sdimg output,
and debug-tweaks for development."
```

---

## Task 5: Create `conf/bblayers.conf.sample`

**Files:**
- Create: `conf/bblayers.conf.sample`

- [ ] **Step 1: Create `conf/bblayers.conf.sample`**

This is a template. The `setup-build.sh` script will substitute `##TOPDIR##` with the actual path at setup time.

```bitbake
# POKY_DIR will be replaced by setup-build.sh with the actual path
# DO NOT edit this file directly — edit conf/bblayers.conf.sample in the repo root

POKY_BBLAYERS_CONF_VERSION = "2"

BBPATH = "${TOPDIR}"
BBFILES ?= ""

BBLAYERS ?= " \
    ##POKY_DIR##/meta \
    ##POKY_DIR##/meta-poky \
    ##POKY_DIR##/meta-yocto-bsp \
    ##OE_DIR##/meta-oe \
    ##OE_DIR##/meta-python \
    ##OE_DIR##/meta-networking \
    ##META_RPI_DIR## \
    ##PROJECT_DIR##/meta-pagespeak \
"
```

- [ ] **Step 2: Commit**

```bash
git add conf/bblayers.conf.sample
git commit -m "feat: add bblayers.conf template

Template with placeholders for setup-build.sh to substitute
with actual paths to poky, meta-openembedded, meta-raspberrypi,
and meta-pagespeak layers."
```

---

## Task 6: Create `setup-build.sh`

**Files:**
- Create: `setup-build.sh`

- [ ] **Step 1: Write `setup-build.sh`**

```bash
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
```

- [ ] **Step 2: Make it executable**

```bash
chmod +x setup-build.sh
```

- [ ] **Step 3: Verify the script is executable and has correct shebang**

Run: `head -1 setup-build.sh && ls -la setup-build.sh`

Expected: `#!/bin/bash` and `-rwxr-xr-x` permissions.

- [ ] **Step 4: Commit**

```bash
git add setup-build.sh
git commit -m "feat: add setup-build.sh for build environment initialization

Idempotent script that clones poky, meta-raspberrypi, and
meta-openembedded at kirkstone branch, sources oe-init-build-env,
and configures build directory with project local.conf and
bblayers.conf."
```

---

## Task 7: Write the README

**Files:**
- Modify: `README.md` (replace the existing `readme.md`)

- [ ] **Step 1: Remove old readme.md and create README.md**

```bash
git rm readme.md
```

- [ ] **Step 2: Write `README.md`**

```markdown
# PageSpeak — Yocto Base Image

Custom Yocto Linux image for the **PageSpeak** portable OCR-to-speech device.
Targets Raspberry Pi 3B and Raspberry Pi 5.

**Related repos:**
- [Project Overview & Wiki](https://github.com/cu-ecen-aeld/final-project-jsnapoli1/wiki)
- [Application Source Code](https://github.com/cu-ecen-aeld/final-project-jsnapoli1)

## Prerequisites

### Linux x86_64 (Native Build)

Install required host packages (Ubuntu/Debian):

```bash
sudo apt-get install gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
    iputils-ping python3-git python3-jinja2 python3-subunit zstd liblz4-tool \
    file locales libacl1
sudo locale-gen en_US.UTF-8
```

Ensure you have at least **80 GB of free disk space** and **8 GB of RAM** (16 GB recommended).

### macOS with Apple Silicon (Docker)

Yocto **cannot build natively on macOS**. Use the CROPS Docker container:

1. Install [Docker Desktop](https://www.docker.com/products/docker-desktop/) and allocate at least **8 GB RAM** and **80 GB disk** in Docker settings.

2. Run the CROPS container:

```bash
docker run --rm -it \
    -v $(pwd):/workdir \
    -v yocto-downloads:/workdir/sources/poky/build/downloads \
    -v yocto-sstate:/workdir/sources/poky/build/sstate-cache \
    crops/poky --workdir=/workdir
```

3. Inside the container, follow the Linux build steps below.

**Performance note:** Building under Docker on Apple Silicon uses Rosetta x86 emulation. Expect builds to take 2-4x longer than native Linux x86. Using named Docker volumes for downloads and sstate-cache (as shown above) avoids re-downloading on subsequent builds.

## Quick Start

### 1. Clone the repository

```bash
git clone https://github.com/cu-ecen-aeld/final-project-mu2d2.git
cd final-project-mu2d2
```

### 2. Run the setup script

```bash
./setup-build.sh
```

This clones poky, meta-raspberrypi, and meta-openembedded (Kirkstone branch), then configures the build directory.

### 3. Build the image

```bash
bitbake pagespeak-image
```

The first build takes several hours. Subsequent builds are much faster due to sstate caching.

### 4. Find the output image

After a successful build, the SD card image is at:

```
build/tmp/deploy/images/raspberrypi3/pagespeak-image-raspberrypi3.rpi-sdimg
```

(Replace `raspberrypi3` with `raspberrypi5` if building for RPi 5.)

## Flashing the Image

### Using `dd` (Linux/macOS)

**WARNING:** Double-check the device name — `dd` will overwrite whatever you point it at.

```bash
# Find your SD card device (e.g., /dev/sdX on Linux, /dev/diskN on macOS)
lsblk   # Linux
diskutil list   # macOS

# Unmount the SD card
sudo umount /dev/sdX*   # Linux
diskutil unmountDisk /dev/diskN   # macOS

# Flash
sudo dd if=build/tmp/deploy/images/raspberrypi3/pagespeak-image-raspberrypi3.rpi-sdimg \
    of=/dev/sdX bs=4M status=progress
sync
```

### Using Raspberry Pi Imager

1. Open Raspberry Pi Imager
2. Choose OS → Use custom → select the `.rpi-sdimg` file
3. Choose your SD card
4. Write

### Using bmaptool (fastest, Linux)

```bash
sudo bmaptool copy build/tmp/deploy/images/raspberrypi3/pagespeak-image-raspberrypi3.rpi-sdimg /dev/sdX
```

## Switching Target Machine

Edit `conf/local.conf` and change the `MACHINE` variable:

```bitbake
# Raspberry Pi 3B (default)
MACHINE = "raspberrypi3"

# Raspberry Pi 5
# MACHINE = "raspberrypi5"
```

Then re-run `./setup-build.sh` (or just re-run `bitbake pagespeak-image` if you're already in the build environment).

**Note on RPi 5:** The `raspberrypi5` MACHINE is available in meta-raspberrypi Kirkstone, but boot support may be limited due to U-Boot compatibility. If you encounter boot issues with RPi 5, consider upgrading to Yocto Scarthgap (5.0 LTS) which has better RPi 5 support via the `meta-lts-mixins` layer.

## First Boot Verification Checklist

After flashing and booting the RPi (connect via serial console at 115200 baud, or SSH over Ethernet):

- [ ] **Login:** System reaches login prompt. Log in as `root` (no password).

- [ ] **Camera (V4L2):** Connect a USB UVC webcam, then:
  ```bash
  dmesg | grep -i uvc
  # Should show: uvcvideo: Found UVC x.xx device ...
  v4l2-ctl --list-devices
  # Should list the USB camera as /dev/video0 or similar
  ```

- [ ] **Audio (ALSA):**
  ```bash
  aplay -l
  # Should list at least one audio device (bcm2835 headphones, or USB audio)
  speaker-test -t wav -c 2
  # Should produce audio through connected speakers/headphones
  ```

- [ ] **GPIO:**
  ```bash
  gpiodetect
  # Should show: gpiochip0 [pinctrl-bcm2835] (54 lines)  (or similar)
  gpioinfo gpiochip0 | head -10
  # Should list GPIO lines with their status
  ```

- [ ] **Network:**
  ```bash
  ip addr
  # Should show eth0 with an IP address (if connected via Ethernet)
  ```

## Repository Structure

```
final-project-mu2d2/
├── meta-pagespeak/                # Custom Yocto layer
│   ├── conf/
│   │   └── layer.conf             # Layer configuration
│   ├── recipes-core/
│   │   └── images/
│   │       └── pagespeak-image.bb # Image recipe
│   ├── recipes-kernel/
│   │   └── linux/
│   │       ├── linux-raspberrypi_%.bbappend
│   │       └── files/
│   │           └── pagespeak.cfg  # Kernel config fragment
│   ├── recipes-app/               # (future: application recipes)
│   ├── recipes-config/            # (future: udev rules, systemd services)
│   └── recipes-support/           # (future: support libraries)
├── conf/
│   ├── local.conf                 # Build configuration
│   └── bblayers.conf.sample       # Layer paths template
├── setup-build.sh                 # Build environment setup script
└── README.md                      # This file
```

## License

MIT, GPL
```

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "docs: replace readme with comprehensive build documentation

Covers prerequisites for Linux and macOS (Docker), quick start,
flashing instructions, machine switching, and first-boot
verification checklist."
```

---

## Task 8: Final verification and cleanup

**Files:**
- Verify: all files from Tasks 1-7

- [ ] **Step 1: Verify the complete directory structure**

Run: `find . -not -path './.git/*' -not -name '.git' -not -name '.DS_Store' -not -path './docs/*' | sort`

Expected output should include all files from the file structure table above.

- [ ] **Step 2: Verify `layer.conf` has correct syntax**

Run: `grep LAYERSERIES_COMPAT meta-pagespeak/conf/layer.conf`

Expected: `LAYERSERIES_COMPAT_meta-pagespeak = "kirkstone"`

- [ ] **Step 3: Verify `pagespeak-image.bb` includes all required packages**

Run: `grep IMAGE_INSTALL meta-pagespeak/recipes-core/images/pagespeak-image.bb`

Expected: Lines containing `v4l-utils`, `alsa-utils`, `libgpiod`, `libgpiod-tools`, `openssh-sftp-server`, `nano`, `kernel-modules`.

- [ ] **Step 4: Verify kernel config fragment**

Run: `cat meta-pagespeak/recipes-kernel/linux/files/pagespeak.cfg`

Expected: Contains `CONFIG_USB_VIDEO_CLASS=m`, `CONFIG_GPIO_CDEV=y`, `CONFIG_SND_USB_AUDIO=m`.

- [ ] **Step 5: Verify `local.conf` has all required settings**

Run: `grep -E '^(MACHINE|DISTRO|ENABLE_UART|GPU_MEM|VIDEO_CAMERA|IMAGE_FSTYPES)' conf/local.conf`

Expected:
```
MACHINE = "raspberrypi3"
DISTRO = "poky"
ENABLE_UART = "1"
GPU_MEM = "128"
VIDEO_CAMERA = "1"
IMAGE_FSTYPES:append = " rpi-sdimg"
```

- [ ] **Step 6: Verify `setup-build.sh` is executable and references correct branches**

Run: `grep YOCTO_RELEASE setup-build.sh && ls -la setup-build.sh`

Expected: `YOCTO_RELEASE="kirkstone"` and executable permissions.

- [ ] **Step 7: Verify git log shows clean commit history**

Run: `git log --oneline`

Expected: 6 commits (Tasks 1-7), each with a descriptive message.

- [ ] **Step 8: Run a dry-run syntax check on setup-build.sh**

Run: `bash -n setup-build.sh`

Expected: No output (no syntax errors).

---

## Notes for Build Verification

Full verification (the `bitbake pagespeak-image` build) requires:
- A Linux x86_64 machine or Docker container with CROPS
- ~80 GB free disk space
- Several hours for the first build

The tasks above can all be completed without running BitBake. The actual build should be run on appropriate hardware to verify:
1. `bitbake pagespeak-image` completes without errors
2. Output image exists at `build/tmp/deploy/images/<machine>/pagespeak-image-<machine>.rpi-sdimg`
3. Image boots on physical Raspberry Pi hardware
4. All verification checklist items pass
