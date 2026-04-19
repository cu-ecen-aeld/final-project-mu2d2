/**
 * @file   capture.c
 * @brief  Camera capture implementation for PageSpeak daemon.
 *
 * Opens /dev/pagespeak-cam (pagespeak-cam driver) for exclusive access and
 * reads the underlying V4L2 device path via PAGESPEAK_CAM_QUERY_CAPS.
 * Captures JPEG frames from that V4L2 device using mmap streaming.
 *
 * The pagespeak-cam fd is held open for the lifetime of the context to
 * maintain the driver's exclusive-access lock (EBUSY on concurrent open).
 */

#include "capture.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <linux/videodev2.h>

#include "pagespeak_cam.h"

#define CAPTURE_WIDTH   1280
#define CAPTURE_HEIGHT  720
#define CAPTURE_PIXFMT  0x47504A4D  /* V4L2_PIX_FMT_MJPEG */
#define POLL_TIMEOUT_MS 5000
#define NUM_BUFS        4
#define WARMUP_FRAMES   30  /* frames discarded for auto-exposure to settle */

struct capture_ctx {
    int ctrl_fd;   /* /dev/pagespeak-cam — holds exclusive access lock */
    int v4l2_fd;   /* /dev/pagespeak-cam-raw — used for mmap streaming */
};

struct capture_ctx *capture_open(const char *ctrl_path)
{
    struct capture_ctx *ctx;
    struct pagespeak_cam_caps caps;
    struct pagespeak_cam_resolution res;
    struct pagespeak_cam_pixfmt pf;
    struct v4l2_format fmt;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        syslog(LOG_ERR, "capture_open: calloc failed");
        return NULL;
    }
    ctx->ctrl_fd = -1;
    ctx->v4l2_fd = -1;

    // Open control device: acquires exclusive access lock
    ctx->ctrl_fd = open(ctrl_path, O_RDWR);
    if (ctx->ctrl_fd < 0) {
        syslog(LOG_ERR, "capture_open: open %s failed: %s",
               ctrl_path, strerror(errno));
        goto fail;
    }

    // Configure resolution and pixel format
    res.width  = CAPTURE_WIDTH;
    res.height = CAPTURE_HEIGHT;
    if (ioctl(ctx->ctrl_fd, PAGESPEAK_CAM_SET_RESOLUTION, &res) < 0)
        syslog(LOG_WARNING, "capture_open: SET_RESOLUTION failed: %s",
               strerror(errno));

    pf.pixelformat = CAPTURE_PIXFMT;
    if (ioctl(ctx->ctrl_fd, PAGESPEAK_CAM_SET_PIXFMT, &pf) < 0)
        syslog(LOG_WARNING, "capture_open: SET_PIXFMT failed: %s",
               strerror(errno));

    // Query caps to get the raw V4L2 device path
    if (ioctl(ctx->ctrl_fd, PAGESPEAK_CAM_QUERY_CAPS, &caps) < 0) {
        syslog(LOG_ERR, "capture_open: QUERY_CAPS failed: %s", strerror(errno));
        goto fail;
    }

    syslog(LOG_DEBUG, "capture_open: raw V4L2 device: %s", caps.raw_device_path);

    // Open the raw V4L2 device for mmap streaming
    ctx->v4l2_fd = open(caps.raw_device_path, O_RDWR);
    if (ctx->v4l2_fd < 0) {
        syslog(LOG_ERR, "capture_open: open %s failed: %s",
               caps.raw_device_path, strerror(errno));
        goto fail;
    }

    // Set V4L2 format
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = CAPTURE_WIDTH;
    fmt.fmt.pix.height      = CAPTURE_HEIGHT;
    fmt.fmt.pix.pixelformat = CAPTURE_PIXFMT;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    if (ioctl(ctx->v4l2_fd, VIDIOC_S_FMT, &fmt) < 0) {
        syslog(LOG_ERR, "capture_open: VIDIOC_S_FMT failed: %s", strerror(errno));
        goto fail;
    }

    syslog(LOG_DEBUG, "capture_open: ready %ux%u", CAPTURE_WIDTH, CAPTURE_HEIGHT);
    return ctx;

fail:
    capture_close(ctx);
    return NULL;
}

bool capture_frame(struct capture_ctx *ctx, struct capture_frame *frame)
{
    struct v4l2_requestbuffers reqbufs;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    void    *bufs[NUM_BUFS];
    uint32_t buf_lengths[NUM_BUFS];
    struct pollfd pfd;
    bool success = false;
    int i;

    if (!ctx || ctx->v4l2_fd < 0 || !frame) {
        syslog(LOG_ERR, "capture_frame: invalid arguments");
        return false;
    }

    frame->data = NULL;
    frame->size = 0;

    for (i = 0; i < NUM_BUFS; i++)
        bufs[i] = MAP_FAILED;

    // Request mmap buffer pool
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count  = NUM_BUFS;
    reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx->v4l2_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        syslog(LOG_ERR, "capture_frame: VIDIOC_REQBUFS: %s", strerror(errno));
        return false;
    }

    // Map and queue all buffers
    for (i = 0; i < NUM_BUFS; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(ctx->v4l2_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            syslog(LOG_ERR, "capture_frame: VIDIOC_QUERYBUF[%d]: %s",
                   i, strerror(errno));
            goto cleanup;
        }
        buf_lengths[i] = buf.length;
        bufs[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                       MAP_SHARED, ctx->v4l2_fd, buf.m.offset);
        if (bufs[i] == MAP_FAILED) {
            syslog(LOG_ERR, "capture_frame: mmap[%d]: %s", i, strerror(errno));
            goto cleanup;
        }

        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(ctx->v4l2_fd, VIDIOC_QBUF, &buf) < 0) {
            syslog(LOG_ERR, "capture_frame: VIDIOC_QBUF[%d]: %s",
                   i, strerror(errno));
            goto cleanup;
        }
    }

    if (ioctl(ctx->v4l2_fd, VIDIOC_STREAMON, &type) < 0) {
        syslog(LOG_ERR, "capture_frame: VIDIOC_STREAMON: %s", strerror(errno));
        goto cleanup;
    }

    // Discard warm-up frames so auto-exposure settles
    for (i = 0; i < WARMUP_FRAMES; i++) {
        pfd.fd     = ctx->v4l2_fd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, POLL_TIMEOUT_MS) <= 0) {
            syslog(LOG_ERR, "capture_frame: poll timeout on warm-up frame %d", i);
            ioctl(ctx->v4l2_fd, VIDIOC_STREAMOFF, &type);
            goto cleanup;
        }
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->v4l2_fd, VIDIOC_DQBUF, &buf) < 0) {
            syslog(LOG_ERR, "capture_frame: VIDIOC_DQBUF warm-up: %s",
                   strerror(errno));
            ioctl(ctx->v4l2_fd, VIDIOC_STREAMOFF, &type);
            goto cleanup;
        }
        if (ioctl(ctx->v4l2_fd, VIDIOC_QBUF, &buf) < 0) {
            syslog(LOG_ERR, "capture_frame: VIDIOC_QBUF re-queue: %s",
                   strerror(errno));
            ioctl(ctx->v4l2_fd, VIDIOC_STREAMOFF, &type);
            goto cleanup;
        }
    }

    // Capture the settled frame
    pfd.fd     = ctx->v4l2_fd;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, POLL_TIMEOUT_MS) <= 0) {
        syslog(LOG_ERR, "capture_frame: poll timeout");
        ioctl(ctx->v4l2_fd, VIDIOC_STREAMOFF, &type);
        goto cleanup;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx->v4l2_fd, VIDIOC_DQBUF, &buf) < 0) {
        syslog(LOG_ERR, "capture_frame: VIDIOC_DQBUF: %s", strerror(errno));
        ioctl(ctx->v4l2_fd, VIDIOC_STREAMOFF, &type);
        goto cleanup;
    }

    ioctl(ctx->v4l2_fd, VIDIOC_STREAMOFF, &type);

    frame->data = (unsigned char *)malloc(buf.bytesused);
    if (!frame->data) {
        syslog(LOG_ERR, "capture_frame: malloc failed");
        goto cleanup;
    }
    memcpy(frame->data, bufs[buf.index], buf.bytesused);
    frame->size = buf.bytesused;

    syslog(LOG_DEBUG, "capture_frame: captured %zu bytes", frame->size);
    success = true;

cleanup:
    for (i = 0; i < NUM_BUFS; i++) {
        if (bufs[i] != MAP_FAILED)
            munmap(bufs[i], buf_lengths[i]);
    }
    return success;
}

void capture_free(struct capture_frame *frame)
{
    if (frame && frame->data) {
        free(frame->data);
        frame->data = NULL;
        frame->size = 0;
    }
}

void capture_close(struct capture_ctx *ctx)
{
    if (!ctx)
        return;
    if (ctx->v4l2_fd >= 0) {
        close(ctx->v4l2_fd);
        ctx->v4l2_fd = -1;
    }
    if (ctx->ctrl_fd >= 0) {
        close(ctx->ctrl_fd);
        ctx->ctrl_fd = -1;
    }
    free(ctx);
}

