SUMMARY = "PageSpeak OCR daemon"
DESCRIPTION = "Main daemon that orchestrates button events, camera capture, \
image preprocessing (OpenCV), OCR, and TTS for the PageSpeak device. \
OCR and TTS are currently stubs."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://main.c \
           file://capture.c \
           file://capture.h \
           file://pagespeak_cam.h \
           file://preprocess.cpp \
           file://preprocess.h \
           file://ocr.h \
           file://ocr_stub.c \
           file://tts.h \
           file://tts_stub.c \
           file://pagespeak-daemon.service \
          "

S = "${WORKDIR}"

DEPENDS += "opencv"

inherit systemd pkgconfig

SYSTEMD_SERVICE:${PN} = "pagespeak-daemon.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    # Compile C sources
    ${CC} ${CFLAGS} --sysroot=${STAGING_DIR_TARGET} \
        -c -o ${S}/main.o      ${S}/main.c
    ${CC} ${CFLAGS} --sysroot=${STAGING_DIR_TARGET} \
        -c -o ${S}/capture.o   ${S}/capture.c
    ${CC} ${CFLAGS} --sysroot=${STAGING_DIR_TARGET} \
        -c -o ${S}/ocr_stub.o  ${S}/ocr_stub.c
    ${CC} ${CFLAGS} --sysroot=${STAGING_DIR_TARGET} \
        -c -o ${S}/tts_stub.o  ${S}/tts_stub.c

    # Compile preprocess.cpp with C++ compiler (OpenCV 4 C++ API)
    ${CXX} ${CXXFLAGS} -std=c++11 --sysroot=${STAGING_DIR_TARGET} \
        -I${STAGING_INCDIR}/opencv4 \
        -c -o ${S}/preprocess.o ${S}/preprocess.cpp

    # Link with C++ linker to pull in C++ runtime
    ${CXX} ${LDFLAGS} --sysroot=${STAGING_DIR_TARGET} \
        -o ${S}/pagespeak-daemon \
        ${S}/main.o \
        ${S}/capture.o \
        ${S}/preprocess.o \
        ${S}/ocr_stub.o \
        ${S}/tts_stub.o \
        -L${STAGING_LIBDIR} -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lm
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
