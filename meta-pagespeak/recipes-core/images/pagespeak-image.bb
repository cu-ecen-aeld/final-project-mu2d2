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

# Ensure all built kernel modules are installed
IMAGE_INSTALL:append = " kernel-modules"

# GPIO button IRQ driver and userspace validation test binary
IMAGE_INSTALL:append = " pagespeak-btn"

# OpenCV — computer vision library (core + imgproc for Sprint 2 OCR pipeline)
IMAGE_INSTALL:append = " opencv"

# Leptonica — image processing library required by Tesseract
IMAGE_INSTALL:append = " leptonica"

# Tesseract OCR engine and English language data.
# Kirkstone meta-oe package names: tesseract (not tesseract-ocr) and
# tesseract-lang-eng (not tesseract-ocr-tessdata-eng).
IMAGE_INSTALL:append = " tesseract tesseract-lang-eng"

# espeak-ng — text-to-speech synthesis engine.
# Not in Kirkstone meta-oe; provided by meta-pagespeak/recipes-support/espeak-ng.
IMAGE_INSTALL:append = " espeak-ng"

# OpenCV validation binary (links libopencv_core.so, DoD check)
IMAGE_INSTALL:append = " opencv-validate"

# Development extras
EXTRA_IMAGE_FEATURES += "debug-tweaks ssh-server-dropbear"
