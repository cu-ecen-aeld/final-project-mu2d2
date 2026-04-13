SUMMARY = "udev rules for PageSpeak camera"
DESCRIPTION = "Creates stable /dev/pagespeak-cam-raw symlink for UVC webcams \
and sets permissions for non-root access."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://90-pagespeak-cam.rules"

INHIBIT_DEFAULT_DEPS = "1"

do_install() {
    install -d ${D}${sysconfdir}/udev/rules.d
    install -m 0644 ${WORKDIR}/90-pagespeak-cam.rules ${D}${sysconfdir}/udev/rules.d/
}

FILES:${PN} = "${sysconfdir}/udev/rules.d"
