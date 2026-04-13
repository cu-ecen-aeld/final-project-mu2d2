/**
 * @file   capture.h
 * @brief  Camera capture interface for PageSpeak daemon.
 *
 * Uses the pagespeak-cam kernel driver which exposes /dev/pagespeak-cam.
 * The driver returns JPEG frames via read().
 */

#ifndef CAPTURE_H
#define CAPTURE_H

#include <stddef.h>
#include <stdbool.h>

/** Maximum frame buffer size (2MB, matches kernel driver limit) */
#define CAPTURE_MAX_FRAME_SIZE (2 * 1024 * 1024)

/**
 * @brief Captured frame data.
 */
struct capture_frame {
    unsigned char *data;    /**< JPEG frame data (caller must free via capture_free) */
    size_t         size;    /**< Actual frame size in bytes */
};

/**
 * @brief Open the camera device.
 * @param device_path Path to camera device (e.g., "/dev/pagespeak-cam")
 * @return File descriptor on success, -1 on error (errno set)
 */
int capture_open(const char *device_path);

/**
 * @brief Capture a single JPEG frame from the camera.
 * @param fd File descriptor from capture_open()
 * @param frame Output frame structure (caller must free with capture_free)
 * @return true on success, false on error
 */
bool capture_frame(int fd, struct capture_frame *frame);

/**
 * @brief Free frame data allocated by capture_frame().
 * @param frame Frame to free
 */
void capture_free(struct capture_frame *frame);

/**
 * @brief Close the camera device.
 * @param fd File descriptor to close
 */
void capture_close(int fd);

#endif /* CAPTURE_H */
