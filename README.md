# PageSpeak — Yocto Base Image

Custom Yocto Linux image for the **PageSpeak** portable OCR-to-speech device.
Supports **Raspberry Pi 3, 4, and 5**.

**Related repos:**
- [Project Overview & Wiki](https://github.com/cu-ecen-aeld/final-project-jsnapoli1/wiki)
- [Application Source Code](https://github.com/cu-ecen-aeld/final-project-jsnapoli1)

## Supported Hardware

| Target | Machine Name | Architecture | Kernel | Status |
|--------|--------------|--------------|--------|--------|
| Raspberry Pi 3B/3B+ | `raspberrypi3` | ARM32 (armv7) | 5.15 | Tested |
| Raspberry Pi 4B | `raspberrypi4` | ARM32 (armv7) | 5.15 | Supported |
| Raspberry Pi 4B | `raspberrypi4-64` | ARM64 (aarch64) | 5.15 | Supported |
| Raspberry Pi 5 | `raspberrypi5` | ARM64 (aarch64) | 6.1 | Tested |

## Quick Start

### Option 1: Using the Build Script (Recommended)

The `build.sh` script handles all configuration and supports multi-target builds:

```bash
# Clone and setup
git clone https://github.com/cu-ecen-aeld/final-project-mu2d2.git
cd final-project-mu2d2
./setup-build.sh

# Build for a specific target
./build.sh rpi3              # Raspberry Pi 3
./build.sh rpi4              # Raspberry Pi 4 (32-bit)
./build.sh rpi5              # Raspberry Pi 5

# Build for all targets
./build.sh all

# Build only a specific recipe
./build.sh rpi3 pagespeak-cam-driver
./build.sh rpi5 pagespeak-btn
```

### Option 2: Manual BitBake Commands

```bash
# Setup
./setup-build.sh
cd build

# Build for RPi 3 (default)
bitbake pagespeak-image

# Build for a different target
MACHINE=raspberrypi5 bitbake pagespeak-image
MACHINE=raspberrypi4 bitbake pagespeak-image
```

## Output Images

After a successful build, SD card images are at:

```
build/tmp/deploy/images/<machine>/pagespeak-image-<machine>.rpi-sdimg
```

Examples:
- `build/tmp/deploy/images/raspberrypi3/pagespeak-image-raspberrypi3.rpi-sdimg`
- `build/tmp/deploy/images/raspberrypi5/pagespeak-image-raspberrypi5.rpi-sdimg`

## Prerequisites

### macOS with Apple Silicon (Docker)

Yocto **cannot build natively on macOS**. Use Docker:

1. Install [Docker Desktop](https://www.docker.com/products/docker-desktop/) with at least **8 GB RAM** and **80 GB disk**.

2. Build the ARM64 Docker image (one-time setup):
   ```bash
   docker build -t yocto-arm64 -f Dockerfile.arm64 .
   ```

3. Use `./build.sh` which automatically runs inside Docker, or run manually:
   ```bash
   docker run --rm -it \
       -v $(pwd):/workdir \
       -v yocto-downloads:/yocto-cache/downloads \
       -v yocto-sstate:/yocto-cache/sstate-cache \
       -v yocto-tmp:/yocto-cache/tmp \
       yocto-arm64:latest bash
   ```

**Note:** Docker volumes provide a case-sensitive ext4 filesystem (required by Yocto) and persist the build cache across runs.

### Linux x86_64 (Native Build)

Install required packages (Ubuntu/Debian):

```bash
sudo apt-get install gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
    iputils-ping python3-git python3-jinja2 python3-subunit zstd liblz4-tool \
    file locales libacl1
sudo locale-gen en_US.UTF-8
```

To build natively instead of in Docker:
```bash
DOCKER=0 ./build.sh rpi3
```

**Requirements:** 80 GB free disk space, 8 GB RAM (16 GB recommended).

## Custom Kernel Modules

PageSpeak includes two custom kernel modules:

### pagespeak-cam-driver

Character device driver for USB webcam frame capture via `/dev/pagespeak-cam`.

| Feature | Description |
|---------|-------------|
| Device node | `/dev/pagespeak-cam` (auto-created) |
| Frame capture | `read()` returns JPEG frames via V4L2 |
| Configuration | `ioctl()` for resolution, pixel format |
| Concurrency | Mutex-protected, single-opener (returns `-EBUSY`) |

Build only the camera driver:
```bash
./build.sh rpi3 pagespeak-cam-driver
./build.sh rpi5 pagespeak-cam-driver
```

### pagespeak-btn

GPIO interrupt driver for the capture button.

Build only the button driver:
```bash
./build.sh rpi3 pagespeak-btn
./build.sh rpi5 pagespeak-btn
```

## Flashing the Image

### Using `dd` (Linux/macOS)

**WARNING:** Double-check the device name — `dd` will overwrite whatever you point it at.

```bash
# Find your SD card device
lsblk                    # Linux: /dev/sdX
diskutil list            # macOS: /dev/diskN

# Unmount
sudo umount /dev/sdX*    # Linux
diskutil unmountDisk /dev/diskN   # macOS

# Flash (example for RPi 3)
sudo dd if=build/tmp/deploy/images/raspberrypi3/pagespeak-image-raspberrypi3.rpi-sdimg \
    of=/dev/sdX bs=4M status=progress
sync
```

### Using Raspberry Pi Imager

1. Open Raspberry Pi Imager
2. Choose OS → Use custom → select the `.rpi-sdimg` file
3. Choose your SD card
4. Write

## First Boot Verification

Connect via serial console (115200 baud) or SSH over Ethernet. Login as `root` (no password).

```bash
# Verify kernel modules loaded
lsmod | grep pagespeak
# Expected: pagespeak_cam, pagespeak_btn

# Verify device nodes
ls -la /dev/pagespeak-cam
# Expected: character device

# Test camera module (with USB webcam connected)
dmesg | grep pagespeak_cam
v4l2-ctl --list-devices

# Test GPIO
gpiodetect
gpioinfo gpiochip0 | head -10

# Test audio
aplay -l
speaker-test -t wav -c 2
```

## Repository Structure

```
final-project-mu2d2/
├── meta-pagespeak/                    # Custom Yocto layer
│   ├── conf/layer.conf                # Layer configuration (Kirkstone)
│   ├── recipes-core/images/
│   │   └── pagespeak-image.bb         # Main image recipe
│   └── recipes-kernel/
│       ├── linux/
│       │   ├── linux-raspberrypi_%.bbappend
│       │   └── files/pagespeak.cfg    # Kernel config fragment
│       ├── pagespeak-cam-driver/
│       │   ├── pagespeak-cam-driver_0.1.bb
│       │   └── files/
│       │       ├── pagespeak_cam.c    # Camera capture module
│       │       ├── pagespeak_cam.h    # ioctl definitions
│       │       ├── Makefile
│       │       └── COPYING
│       └── pagespeak-btn/
│           ├── pagespeak-btn.bb
│           └── files/
│               ├── pagespeak-btn.c    # GPIO button module
│               └── Makefile
├── tests/
│   ├── pagespeak_cam_test.c           # Userspace test for camera driver
│   └── pagespeak_cam.h
├── conf/
│   ├── local.conf                     # Build configuration
│   └── bblayers.conf.sample           # Layer paths template
├── build.sh                           # Multi-target build script
├── setup-build.sh                     # Initial setup script
├── Dockerfile.arm64                   # Docker image for Apple Silicon
└── README.md
```

## Build Configuration

### Changing Default Target

Edit `conf/local.conf`:

```bitbake
# Default machine (can be overridden by MACHINE env var or build.sh)
MACHINE ?= "raspberrypi3"
```

Or override at build time:
```bash
MACHINE=raspberrypi5 bitbake pagespeak-image
```

### Parallel Build Settings

In `conf/local.conf` or via environment:

```bash
BB_NUMBER_THREADS=16 PARALLEL_MAKE="-j 16" ./build.sh rpi5
```

### Build Cache

Docker builds use named volumes for persistent caching:
- `yocto-downloads` — Downloaded source tarballs
- `yocto-sstate` — Shared state cache (dramatically speeds up rebuilds)
- `yocto-tmp` — Build artifacts

To clear the cache:
```bash
docker volume rm yocto-downloads yocto-sstate yocto-tmp
```

## Troubleshooting

### Build fails with "case-sensitive filesystem" error

On macOS, Yocto requires a case-sensitive filesystem. The Docker volumes handle this automatically. If building natively on Linux, ensure your filesystem is case-sensitive (ext4, xfs).

### Module fails to load with "version magic" error

The kernel module was built for a different kernel version. Rebuild the module:
```bash
./build.sh rpi3 pagespeak-cam-driver -c cleansstate
./build.sh rpi3 pagespeak-cam-driver
```

### Docker build is slow on Apple Silicon

This is expected — Rosetta x86 emulation adds overhead. Use the native ARM64 Docker image (`Dockerfile.arm64`) for better performance. First builds take 2-4 hours; subsequent builds use sstate cache and are much faster.

## License

- Kernel modules: GPL-2.0
- Build scripts and configuration: MIT
