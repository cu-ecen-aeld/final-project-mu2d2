# opencv-validate.bb
#
# Minimal DoD validation binary for issue #6.
# Links libopencv_core.so and libopencv_imgproc.so on the target to confirm
# OpenCV is correctly installed in the rootfs.
#
# On target, run: opencv-validate
# Expected:       opencv-validate: OK 64x64

SUMMARY     = "OpenCV DoD validation binary for PageSpeak issue #6"
DESCRIPTION = "Compiles a small C program that calls cvCreateImage and \
cvSmooth to verify libopencv_core and libopencv_imgproc are present and \
functional on the target rootfs."

LICENSE          = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = "opencv"

inherit pkgconfig

SRC_URI = "file://opencv-validate.c"

S = "${WORKDIR}"

do_compile() {
    ${CXX} ${CXXFLAGS} ${LDFLAGS} \
        -std=c++11 \
        -I${STAGING_INCDIR}/opencv4 \
        -o ${S}/opencv-validate \
        ${S}/opencv-validate.c \
        -L${STAGING_LIBDIR} -lopencv_core -lopencv_imgproc
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/opencv-validate ${D}${bindir}/opencv-validate
}

FILES:${PN} = "${bindir}/opencv-validate"
