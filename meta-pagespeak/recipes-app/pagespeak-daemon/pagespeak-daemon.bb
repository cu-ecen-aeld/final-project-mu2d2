SUMMARY = "PageSpeak OCR daemon"
DESCRIPTION = "Main daemon that orchestrates button events, camera capture, \
image preprocessing, OCR, and TTS for the PageSpeak device. \
Preprocessing, OCR, and TTS are currently stubs."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://main.c \
           file://capture.c \
           file://capture.h \
           file://preprocess.h \
           file://preprocess_stub.c \
           file://ocr.h \
           file://ocr_stub.c \
           file://tts.h \
           file://tts_stub.c \
           file://pagespeak-daemon.service \
          "

S = "${WORKDIR}"

inherit systemd

SYSTEMD_SERVICE:${PN} = "pagespeak-daemon.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} ${TARGET_CC_ARCH} --sysroot=${STAGING_DIR_TARGET} \
        -o ${S}/pagespeak-daemon \
        ${S}/main.c \
        ${S}/capture.c \
        ${S}/preprocess_stub.c \
        ${S}/ocr_stub.c \
        ${S}/tts_stub.c
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/pagespeak-daemon ${D}${bindir}/pagespeak-daemon

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/pagespeak-daemon.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} = "${bindir}/pagespeak-daemon"
FILES:${PN} += "${systemd_system_unitdir}/pagespeak-daemon.service"

RDEPENDS:${PN} = "pagespeak-btn pagespeak-cam-driver"
