# pagespeak-btn.bb
#
# Yocto recipe for the PageSpeak GPIO capture button driver.
# Builds the pagespeak-btn out-of-tree kernel module from source files
# committed alongside this recipe in the files/ subdirectory.

SUMMARY     = "PageSpeak GPIO capture button IRQ driver"
DESCRIPTION = "Out-of-tree kernel module registering a falling-edge IRQ on \
GPIO 17 (BCM numbering, physical pin 11) for the PageSpeak momentary capture \
button. Exposes /dev/pagespeak-btn as a character device."

LICENSE          = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

# The module class handles cross-compiling an out-of-tree kernel module
# against the Yocto-built kernel headers
inherit module

# All source files live in the files/ subdirectory next to this recipe
SRC_URI = " \
    file://pagespeak-btn.c \
    file://Makefile        \
"

# BitBake unpacks file:// sources directly into WORKDIR
S = "${WORKDIR}"

# Create /etc directories that kernel-module-split.bbclass expects
do_install:append() {
    install -d ${D}${sysconfdir}/modprobe.d
    install -d ${D}${sysconfdir}/modules-load.d
}

# Satisfy any package dependency on the kernel module by name
RPROVIDES:${PN} += "kernel-module-pagespeak-btn"

# Auto-load the module at boot via /etc/modules-load.d/pagespeak-btn.conf
KERNEL_MODULE_AUTOLOAD += "pagespeak-btn"
