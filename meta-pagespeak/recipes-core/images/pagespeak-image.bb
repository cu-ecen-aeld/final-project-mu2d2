SUMMARY = "PageSpeak base image"
DESCRIPTION = "Bootable image for the PageSpeak OCR-to-speech device. \
Includes V4L2 camera support, ALSA audio, GPIO tools, OpenCV, Tesseract OCR, \
espeak-ng TTS, and basic debug utilities."
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

# Static IP via systemd-networkd for direct ethernet connection (192.168.10.2/24)
# systemd-networkd is part of the systemd package, not a separate install target
IMAGE_INSTALL:append = " network-config"

# Ensure all built kernel modules are installed, plus kmod for modprobe/insmod
IMAGE_INSTALL:append = " kernel-modules kmod"

# udev rules for stable camera device symlink
IMAGE_INSTALL:append = " pagespeak-udev-rules"

# Camera capture kernel module (depends on pagespeak-udev-rules for /dev/pagespeak-cam-raw)
IMAGE_INSTALL:append = " pagespeak-cam-driver"

# GPIO button IRQ driver and userspace validation test binary
IMAGE_INSTALL:append = " pagespeak-btn pagespeak-btn-test"

# OpenCV — computer vision library (core + imgproc for Sprint 2 OCR pipeline)
IMAGE_INSTALL:append = " opencv"

# Leptonica — image processing library required by Tesseract
IMAGE_INSTALL:append = " leptonica"

# Tesseract OCR engine and English language data.
# Kirkstone meta-oe package names: tesseract (not tesseract-ocr) and
# tesseract-lang-eng (not tesseract-ocr-tessdata-eng).
IMAGE_INSTALL:append = " tesseract tesseract-lang-eng"

# espeak — text-to-speech synthesis engine.
# Available in Kirkstone meta-oe as 'espeak' (1.48.04).
# NOTE: espeak ships libespeak.so / <espeak/speak_lib.h>, NOT the espeak-ng API.
# tts.c in the application repo must be updated to use the espeak API if this is kept.
IMAGE_INSTALL:append = " espeak"

# OpenCV validation binary (links libopencv_core.so, DoD check)
IMAGE_INSTALL:append = " opencv-validate"

# PageSpeak main daemon (button → camera → OCR pipeline)
IMAGE_INSTALL:append = " pagespeak-daemon"

# PageSpeak preprocessing unit test binary
IMAGE_INSTALL:append = " pagespeak-daemon-tests"

# Development extras
EXTRA_IMAGE_FEATURES += "debug-tweaks ssh-server-dropbear"
