/**
 * @file   preprocess.h
 * @brief  Image preprocessing interface for PageSpeak daemon.
 *
 * STUB: This interface will be implemented with OpenCV in a future issue.
 * The stub implementation passes through the raw JPEG data.
 */

#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include "capture.h"

/**
 * @brief Preprocessed image data ready for OCR.
 *
 * Future implementation will contain grayscale/binary image data.
 * Current stub just references the original JPEG.
 */
struct preprocess_result {
    unsigned char *data;    /**< Processed image data */
    size_t         size;    /**< Data size in bytes */
    int            width;   /**< Image width (0 if unknown) */
    int            height;  /**< Image height (0 if unknown) */
};

/**
 * @brief Preprocess a captured frame for OCR.
 *
 * Future implementation will:
 * - Decode JPEG
 * - Convert to grayscale
 * - Apply thresholding
 * - Reduce noise
 *
 * @param frame Captured JPEG frame
 * @param result Output preprocessed image (caller must free with preprocess_free)
 * @return true on success, false on error
 */
bool preprocess_image(const struct capture_frame *frame,
                      struct preprocess_result *result);

/**
 * @brief Free preprocessed image data.
 * @param result Result to free
 */
void preprocess_free(struct preprocess_result *result);

#endif /* PREPROCESS_H */
