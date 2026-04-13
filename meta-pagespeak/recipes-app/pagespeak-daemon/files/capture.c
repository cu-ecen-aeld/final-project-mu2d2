/**
 * @file   capture.c
 * @brief  Camera capture implementation for PageSpeak daemon.
 *
 * Reads JPEG frames from /dev/pagespeak-cam (pagespeak-cam kernel driver).
 * The driver handles V4L2 interaction; we just read() to get frames.
 */

#include "capture.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

int capture_open(const char *device_path)
{
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        syslog(LOG_ERR, "capture_open: failed to open %s: %s",
               device_path, strerror(errno));
    } else {
        syslog(LOG_DEBUG, "capture_open: opened %s (fd=%d)", device_path, fd);
    }
    return fd;
}

bool capture_frame(int fd, struct capture_frame *frame)
{
    unsigned char *buffer = NULL;
    ssize_t bytes_read;

    if (!frame) {
        return false;
    }

    frame->data = NULL;
    frame->size = 0;

    /* Allocate maximum buffer size */
    buffer = malloc(CAPTURE_MAX_FRAME_SIZE);
    if (!buffer) {
        syslog(LOG_ERR, "capture_frame: malloc failed: %s", strerror(errno));
        return false;
    }

    /* Read frame from device - single read returns complete JPEG */
    bytes_read = read(fd, buffer, CAPTURE_MAX_FRAME_SIZE);
    if (bytes_read < 0) {
        syslog(LOG_ERR, "capture_frame: read failed: %s", strerror(errno));
        free(buffer);
        return false;
    }

    if (bytes_read == 0) {
        syslog(LOG_WARNING, "capture_frame: empty frame received");
        free(buffer);
        return false;
    }

    /* Shrink buffer to actual size to save memory */
    frame->data = realloc(buffer, bytes_read);
    if (!frame->data) {
        /* realloc failed but original buffer still valid */
        frame->data = buffer;
    }
    frame->size = (size_t)bytes_read;

    syslog(LOG_DEBUG, "capture_frame: captured %zu bytes", frame->size);
    return true;
}

void capture_free(struct capture_frame *frame)
{
    if (frame && frame->data) {
        free(frame->data);
        frame->data = NULL;
        frame->size = 0;
    }
}

void capture_close(int fd)
{
    if (fd >= 0) {
        close(fd);
        syslog(LOG_DEBUG, "capture_close: closed fd=%d", fd);
    }
}
