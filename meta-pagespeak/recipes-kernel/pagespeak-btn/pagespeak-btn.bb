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
inherit module

# All source files live in the files/ subdirectory next to this recipe
SRC_URI = " \
    file://pagespeak-btn.c        \
    file://pagespeak-btn-test.c   \
    file://Makefile               \
"

# BitBake unpacks file:// sources directly into WORKDIR
S = "${WORKDIR}"

# Compile the userspace test binary after the kernel module build completes.
do_compile:append() {
    ${CC} ${CFLAGS} ${LDFLAGS} ${TARGET_CC_ARCH} --sysroot=${STAGING_DIR_TARGET} \
        -o ${S}/pagespeak-btn-test \
        ${S}/pagespeak-btn-test.c
}

# Install the test binary to /usr/bin on the target rootfs
do_install:append() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/pagespeak-btn-test ${D}${bindir}/pagespeak-btn-test
}

# Declare the test binary as part of this package's file set
FILES:${PN} += "${bindir}/pagespeak-btn-test"

# Satisfy any package dependency on the kernel module by name
RPROVIDES:${PN} += "kernel-module-pagespeak-btn"

# Auto-load the module at boot via /etc/modules-load.d/pagespeak-btn.conf
KERNEL_MODULE_AUTOLOAD += "pagespeak-btn"
