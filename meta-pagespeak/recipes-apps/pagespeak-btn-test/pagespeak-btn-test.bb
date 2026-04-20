# pagespeak-btn-test.bb
#
# Userspace validation binary for the pagespeak-btn kernel module.
# Reads button press events from /dev/pagespeak-btn and logs them.

SUMMARY     = "PageSpeak button driver test utility"
DESCRIPTION = "Userspace validation program that reads button press events \
from /dev/pagespeak-btn and logs them to syslog and stdout. Used for DoD \
validation (20 press test)."

LICENSE          = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

SRC_URI = "file://pagespeak-btn-test.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o pagespeak-btn-test pagespeak-btn-test.c
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 pagespeak-btn-test ${D}${bindir}/pagespeak-btn-test
}

RDEPENDS:${PN} = "pagespeak-btn"
