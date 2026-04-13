/**
 * @file   preprocess_stub.c
 * @brief  STUB preprocessing implementation for PageSpeak daemon.
 *
 * This stub passes through the raw JPEG data without processing.
 * Replace with OpenCV implementation in future issue.
 *
 * TODO: Implement with OpenCV:
 * - cv::imdecode() to decode JPEG
 * - cv::cvtColor() to convert to grayscale
 * - cv::GaussianBlur() for noise reduction
 * - cv::adaptiveThreshold() for binarization
 */

#include "preprocess.h"
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

bool preprocess_image(const struct capture_frame *frame,
                      struct preprocess_result *result)
{
    if (!frame || !frame->data || !result) {
        return false;
    }

    result->data = NULL;
    result->size = 0;
    result->width = 0;
    result->height = 0;

    /* STUB: Copy raw JPEG data as-is */
    result->data = malloc(frame->size);
    if (!result->data) {
        syslog(LOG_ERR, "preprocess_image: malloc failed");
        return false;
    }

    memcpy(result->data, frame->data, frame->size);
    result->size = frame->size;

    syslog(LOG_INFO, "preprocess_image: STUB - passed through %zu bytes (OpenCV not implemented)",
           result->size);
    return true;
}

void preprocess_free(struct preprocess_result *result)
{
    if (result && result->data) {
        free(result->data);
        result->data = NULL;
        result->size = 0;
        result->width = 0;
        result->height = 0;
    }
}
