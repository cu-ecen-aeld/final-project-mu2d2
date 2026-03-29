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
