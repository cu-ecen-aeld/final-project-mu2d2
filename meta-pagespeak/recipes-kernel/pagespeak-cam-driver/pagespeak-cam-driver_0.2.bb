SUMMARY = "PageSpeak camera capture kernel module"
DESCRIPTION = "Character device driver (/dev/pagespeak-cam) that captures \
JPEG frames from a USB UVC webcam via kernel-space V4L2 read mode."
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

inherit module

SRC_URI = "file://Makefile \
           file://pagespeak_cam.c \
           file://pagespeak_cam.h \
           file://pagespeak_cam_test.c \
          "

S = "${WORKDIR}"

RPROVIDES:${PN} += "kernel-module-pagespeak-cam"

# v4l2-ctl is needed at runtime for camera format configuration
RDEPENDS:${PN} += "v4l-utils"

# udev rules create the /dev/pagespeak-cam-raw symlink this driver depends on
RDEPENDS:${PN} += "pagespeak-udev-rules"

# Auto-load the module at boot via /etc/modules-load.d/pagespeak-cam.conf
KERNEL_MODULE_AUTOLOAD += "pagespeak_cam"

# Compile the userspace test binary after the kernel module build completes
do_compile:append() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        -o ${S}/pagespeak-cam-test \
        ${S}/pagespeak_cam_test.c
}

# Install the test binary to /usr/bin on the target rootfs
do_install:append() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/pagespeak-cam-test ${D}${bindir}/pagespeak-cam-test
}

# Declare the test binary as part of this package's file set
FILES:${PN} += "${bindir}/pagespeak-cam-test"
