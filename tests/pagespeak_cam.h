/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * pagespeak_cam.h — ioctl definitions for /dev/pagespeak-cam
 *
 * Shared between the kernel module and userspace test programs.
 */

#ifndef PAGESPEAK_CAM_H
#define PAGESPEAK_CAM_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

#define PAGESPEAK_CAM_MAGIC 'P'

/*
 * Resolution setting — width and height in pixels.
 */
struct pagespeak_cam_resolution {
    uint32_t width;
    uint32_t height;
};

/*
 * Pixel format — V4L2 fourcc code (e.g., V4L2_PIX_FMT_MJPEG).
 */
struct pagespeak_cam_pixfmt {
    uint32_t pixelformat;
};

/*
 * Capability query result.
 */
struct pagespeak_cam_caps {
    uint32_t max_width;
    uint32_t max_height;
    uint32_t min_width;
    uint32_t min_height;
    uint32_t current_width;
    uint32_t current_height;
    uint32_t current_pixelformat;
    char     device_name[32];
    char     raw_device_path[64]; /* underlying V4L2 device for userspace mmap capture */
};

/* ioctl commands */
#define PAGESPEAK_CAM_SET_RESOLUTION \
    _IOW(PAGESPEAK_CAM_MAGIC, 1, struct pagespeak_cam_resolution)

#define PAGESPEAK_CAM_SET_PIXFMT \
    _IOW(PAGESPEAK_CAM_MAGIC, 2, struct pagespeak_cam_pixfmt)

#define PAGESPEAK_CAM_QUERY_CAPS \
    _IOR(PAGESPEAK_CAM_MAGIC, 3, struct pagespeak_cam_caps)

#endif /* PAGESPEAK_CAM_H */
