# espeak-ng_1.51.1.bb
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

# Commit pinned to the signed 1.51.1 release tag
SRCREV = "34762a2b9621d3643e67a00642984c21f0626bdc"
SRC_URI = "git://github.com/espeak-ng/espeak-ng.git;protocol=https;nobranch=1"

S = "${WORKDIR}/git"

inherit cmake pkgconfig

# Disable audio playback (pcaudio not in Kirkstone meta-oe).
# espeak-ng can still produce WAV files via: espeak-ng -w out.wav "text"
EXTRA_OECMAKE = " \
    -DBUILD_SHARED_LIBS=ON    \
    -DUSE_SPEECHPLAYER=OFF    \
"

# Include the language data directory and shared library
FILES:${PN}     += "${libdir}/espeak-ng-data"
FILES:${PN}-dev += "${includedir}/espeak-ng"
