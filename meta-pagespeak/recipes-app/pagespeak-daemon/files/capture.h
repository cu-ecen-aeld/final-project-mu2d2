/**
 * @file   capture.h
 * @brief  Camera capture interface for PageSpeak daemon.
 *
 * Opens /dev/pagespeak-cam (pagespeak-cam driver) for exclusive access
 * and ioctl configuration, then captures JPEG frames from the underlying
 * V4L2 device via mmap streaming.
 */

#ifndef CAPTURE_H
#define CAPTURE_H

#include <stddef.h>
#include <stdbool.h>

/** Maximum frame buffer size (2MB) */
#define CAPTURE_MAX_FRAME_SIZE (2 * 1024 * 1024)

/**
 * @brief Captured frame data.
 */
struct capture_frame {
    unsigned char *data;    /**< JPEG frame data (caller must free via capture_free) */
    size_t         size;    /**< Actual frame size in bytes */
};

/**
 * @brief Opaque camera context (holds both control and V4L2 file descriptors).
 */
struct capture_ctx;

/**
 * @brief Open the camera and prepare it for frame capture.
 *
 * Opens the pagespeak-cam control device for exclusive access, reads the
 * raw V4L2 device path via QUERY_CAPS, and opens the V4L2 device.
 *
 * @param ctrl_path Path to the control device (e.g., "/dev/pagespeak-cam")
 * @return Allocated capture context on success, NULL on error
 */
struct capture_ctx *capture_open(const char *ctrl_path);

/**
 * @brief Capture a single JPEG frame using V4L2 mmap streaming.
 * @param ctx  Context from capture_open()
 * @param frame Output frame (caller must free with capture_free)
 * @return true on success, false on error
 */
bool capture_frame(struct capture_ctx *ctx, struct capture_frame *frame);

/**
 * @brief Free frame data allocated by capture_frame().
 * @param frame Frame to free
 */
void capture_free(struct capture_frame *frame);

/**
 * @brief Close the camera and free the context.
 * @param ctx Context from capture_open()
 */
void capture_close(struct capture_ctx *ctx);

#endif /* CAPTURE_H */

