SUMMARY = "PageSpeak camera capture kernel module"
DESCRIPTION = "Character device driver (/dev/pagespeak-cam) that captures \
JPEG frames from a USB UVC webcam via kernel-space V4L2 read mode."
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

inherit module

SRC_URI = "file://Makefile \
           file://pagespeak_cam.c \
           file://pagespeak_cam.h \
          "

S = "${WORKDIR}"

RPROVIDES:${PN} += "kernel-module-pagespeak-cam"

# v4l2-ctl is needed at runtime for camera format configuration
RDEPENDS:${PN} += "v4l-utils"

# udev rules create the /dev/pagespeak-cam-raw symlink this driver depends on
RDEPENDS:${PN} += "pagespeak-udev-rules"

# Auto-load the module at boot via /etc/modules-load.d/pagespeak-cam.conf
KERNEL_MODULE_AUTOLOAD += "pagespeak_cam"
