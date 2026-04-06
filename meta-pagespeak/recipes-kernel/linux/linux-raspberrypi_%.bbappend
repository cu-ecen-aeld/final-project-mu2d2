FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://pagespeak.cfg"

# Autoload UVC and USB audio modules at boot.
# uvcvideo transitively loads videobuf2-core, videobuf2-v4l2,
# videobuf2-vmalloc, and videodev — no need to list them separately.
KERNEL_MODULE_AUTOLOAD += "uvcvideo snd-usb-audio"

# Autoload the GPIO button IRQ module so /dev/pagespeak-btn is available
# immediately after boot without a manual insmod.
KERNEL_MODULE_AUTOLOAD += "pagespeak-btn"
