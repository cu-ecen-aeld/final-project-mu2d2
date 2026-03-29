FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://pagespeak.cfg"

# Autoload UVC and USB audio modules at boot.
# uvcvideo transitively loads videobuf2-core, videobuf2-v4l2,
# videobuf2-vmalloc, and videodev — no need to list them separately.
KERNEL_MODULE_AUTOLOAD += "uvcvideo snd-usb-audio"
