# espeak-ng_1.52.0.bb
#
# Custom Yocto recipe for eSpeak NG — required because espeak-ng is not
# present in the Kirkstone branch of meta-openembedded (only the older
# espeak 1.48.04 is available there). eSpeak NG is the actively-maintained
# successor to eSpeak and is required by the PageSpeak TTS pipeline.
#
# Usage on target:
#   espeak-ng "hello world"                     # play directly via ALSA
#   espeak-ng -w /tmp/out.wav "hello world"     # write WAV for aplay

SUMMARY     = "eSpeak NG — compact open source text-to-speech synthesizer"
HOMEPAGE    = "https://github.com/espeak-ng/espeak-ng"
BUGTRACKER  = "https://github.com/espeak-ng/espeak-ng/issues"

LICENSE          = "GPL-3.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=d32239bcb673463ab874e80d47fae504"

# espeak-ng 1.52.0 — first cmake release, last autoconf release
SRCREV = "4870adfa25b1a32b4361592f1be8a40337c58d6c"

# sonic is fetched by espeak-ng's CMakeLists via FetchContent at configure
# time. Yocto blocks network access during do_configure, so we pre-fetch
# sonic here and redirect FetchContent to the local copy via
# FETCHCONTENT_SOURCE_DIR_SONIC-GIT.
SRCREV_sonic = "fbf75c3d6d846bad3bb3d456cbc5d07d9fd8c104"

SRC_URI = " \
    git://github.com/espeak-ng/espeak-ng.git;protocol=https;nobranch=1 \
    git://github.com/waywardgeek/sonic.git;name=sonic;protocol=https;nobranch=1;destsuffix=sonic-src \
"

SRCREV_FORMAT = "default_sonic"

S = "${WORKDIR}/git"

# qemu-native provides qemu-aarch64 so cmake can run the cross-compiled
# espeak-ng binary during the data compilation step (compile-phonemes,
# compile-intonations, compile-dict). Without this, cmake tries to run
# the ARM ELF directly on x86 and fails with "syntax error: word unexpected".
DEPENDS += "qemu-native"

inherit cmake pkgconfig

# Write a wrapper script that invokes qemu-aarch64 with the ARM sysroot so
# shared libraries are found. CMAKE_CROSSCOMPILING_EMULATOR points to this
# wrapper, which cmake prepends to every cross-compiled binary invocation.
do_configure:prepend() {
    cat > ${WORKDIR}/qemu-wrapper.sh << WRAPPER
#!/bin/sh
exec ${STAGING_BINDIR_NATIVE}/qemu-aarch64 -L ${STAGING_DIR_TARGET} "\$@"
WRAPPER
    chmod +x ${WORKDIR}/qemu-wrapper.sh
}

# Disable pcaudio (not in Kirkstone meta-oe) and speech-player.
EXTRA_OECMAKE = " \
    -DBUILD_SHARED_LIBS=ON \
    -DUSE_SPEECHPLAYER=OFF \
    -DUSE_LIBPCAUDIO=OFF   \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DFETCHCONTENT_SOURCE_DIR_SONIC-GIT=${WORKDIR}/sonic-src \
    -DCMAKE_CROSSCOMPILING_EMULATOR=${WORKDIR}/qemu-wrapper.sh \
"

# Include the language data directory and shared library
FILES:${PN}     += "${libdir}/espeak-ng-data"
FILES:${PN}-dev += "${includedir}/espeak-ng"
