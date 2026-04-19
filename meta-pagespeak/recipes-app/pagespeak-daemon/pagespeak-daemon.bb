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
           file://preprocess_test.cpp \
           file://ocr.cpp \
           file://ocr.h \
           file://ocr_test.cpp \
           file://tts.h \
           file://tts_stub.c \
           file://pagespeak-daemon.service \
          "

S = "${WORKDIR}"

DEPENDS += "opencv tesseract"

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
        -c -o ${S}/tts_stub.o  ${S}/tts_stub.c

    # Compile C++ sources (OpenCV, Tesseract)
    ${CXX} ${CXXFLAGS} -std=c++11 --sysroot=${STAGING_DIR_TARGET} \
        -I${STAGING_INCDIR}/opencv4 \
        -c -o ${S}/preprocess.o ${S}/preprocess.cpp
    ${CXX} ${CXXFLAGS} -std=c++11 --sysroot=${STAGING_DIR_TARGET} \
        -c -o ${S}/ocr.o ${S}/ocr.cpp

    # Compile and link preprocess_test binary
    ${CXX} ${CXXFLAGS} -std=c++11 --sysroot=${STAGING_DIR_TARGET} \
        -I${STAGING_INCDIR}/opencv4 -I${S} \
        -c -o ${S}/preprocess_test.o ${S}/preprocess_test.cpp
    ${CXX} ${LDFLAGS} --sysroot=${STAGING_DIR_TARGET} \
        -o ${S}/preprocess_test \
        ${S}/preprocess_test.o \
        ${S}/preprocess.o \
        ${S}/capture.o \
        -L${STAGING_LIBDIR} -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lm

    # Compile and link ocr_test binary
    ${CXX} ${CXXFLAGS} -std=c++11 --sysroot=${STAGING_DIR_TARGET} \
        -I${STAGING_INCDIR}/opencv4 -I${S} \
        -c -o ${S}/ocr_test.o ${S}/ocr_test.cpp
    ${CXX} ${LDFLAGS} --sysroot=${STAGING_DIR_TARGET} \
        -o ${S}/ocr_test \
        ${S}/ocr_test.o \
        ${S}/ocr.o \
        ${S}/preprocess.o \
        ${S}/capture.o \
        -L${STAGING_LIBDIR} -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
        -ltesseract -lm

    # Link pagespeak-daemon
    ${CXX} ${LDFLAGS} --sysroot=${STAGING_DIR_TARGET} \
        -o ${S}/pagespeak-daemon \
        ${S}/main.o \
        ${S}/capture.o \
        ${S}/preprocess.o \
        ${S}/ocr.o \
        ${S}/tts_stub.o \
        -L${STAGING_LIBDIR} -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
        -ltesseract -lm
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/pagespeak-daemon ${D}${bindir}/pagespeak-daemon
    install -m 0755 ${S}/preprocess_test  ${D}${bindir}/preprocess_test
    install -m 0755 ${S}/ocr_test         ${D}${bindir}/ocr_test

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/pagespeak-daemon.service ${D}${systemd_system_unitdir}/
}

FILES:${PN} = "${bindir}/pagespeak-daemon"
FILES:${PN} += "${systemd_system_unitdir}/pagespeak-daemon.service"
FILES:${PN}-tests = "${bindir}/preprocess_test ${bindir}/ocr_test"

PACKAGES =+ "${PN}-tests"

RDEPENDS:${PN} = "pagespeak-btn pagespeak-cam-driver"
