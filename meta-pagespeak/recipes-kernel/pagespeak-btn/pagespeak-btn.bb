# pagespeak-btn.bb
#
# Yocto recipe for the PageSpeak GPIO capture button driver.
# Builds the pagespeak-btn out-of-tree kernel module from source files
# committed alongside this recipe in the files/ subdirectory.
# Also compiles the pagespeak-btn-test userspace validation binary and
# installs it to /usr/bin on the target.

SUMMARY     = "PageSpeak GPIO capture button IRQ driver and test utility"
DESCRIPTION = "Out-of-tree kernel module registering a falling-edge IRQ on \
GPIO 17 (BCM numbering, physical pin 11) for the PageSpeak momentary capture \
button. Exposes /dev/pagespeak-btn as a character device. Includes the \
pagespeak-btn-test binary for DoD validation (20 press test)."

LICENSE          = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

# The module class handles cross-compiling an out-of-tree kernel module
# against the Yocto-built kernel headers
inherit module systemd

# All source files live in the files/ subdirectory next to this recipe
SRC_URI = " \
    file://pagespeak-btn.c        \
    file://pagespeak-btn-test.c   \
    file://Makefile               \
    file://pagespeak-btn.service  \
"

# BitBake unpacks file:// sources directly into WORKDIR
S = "${WORKDIR}"

# Compile the userspace test binary after the kernel module build completes.
# ${CC} is the Yocto cross-compiler; ${CFLAGS} and ${LDFLAGS} are set by
# the Yocto build environment for the target architecture.
do_compile:append() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        -o ${S}/pagespeak-btn-test \
        ${S}/pagespeak-btn-test.c
}

# Install the test binary to /usr/bin on the target rootfs
do_install:append() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/pagespeak-btn-test ${D}${bindir}/pagespeak-btn-test

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/pagespeak-btn.service ${D}${systemd_system_unitdir}/pagespeak-btn.service
}

# Declare the test binary and systemd service as part of this package
FILES:${PN} += "${bindir}/pagespeak-btn-test"
FILES:${PN} += "${systemd_system_unitdir}/pagespeak-btn.service"

SYSTEMD_SERVICE:${PN} = "pagespeak-btn.service"
SYSTEMD_AUTO_ENABLE = "enable"

# Satisfy any package dependency on the kernel module by name
RPROVIDES:${PN} += "kernel-module-pagespeak-btn"

# Module is loaded by pagespeak-btn.service (after dev-gpiochip0.device appears),
# not via modules-load.d, to avoid -EPROBE_DEFER on RPi5's PCIe-attached RP1 GPIO.
