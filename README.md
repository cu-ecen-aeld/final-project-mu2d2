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
    --platform linux/amd64 \
    -v $(pwd):/workdir \
    -v yocto-downloads:/yocto-cache/downloads \
    -v yocto-sstate:/yocto-cache/sstate-cache \
    -v yocto-tmp:/yocto-cache/tmp \
    --entrypoint "" \
    crops/poky \
    bash -c "sudo chown $(id -u):$(id -g) /yocto-cache/downloads /yocto-cache/sstate-cache /yocto-cache/tmp && cd /workdir && bash"
```

This starts an interactive shell with Docker named volumes for the Yocto build cache. The volumes provide a case-sensitive ext4 filesystem (required by Yocto — macOS APFS/HFS+ is case-insensitive) and persist downloads/sstate across container restarts.

3. Inside the container, follow the Quick Start steps below.

**Performance note:** Building under Docker on Apple Silicon uses Rosetta x86 emulation. Expect builds to take 2-4x longer than native Linux x86.

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
