/**
 * @file   ocr.h
 * @brief  OCR interface for PageSpeak daemon.
 *
 * Implemented using the Tesseract C API (tesseract/capi.h).
 * Words below OCR_CONFIDENCE_THRESHOLD are discarded from output.
 */

#ifndef OCR_H
#define OCR_H

#include <stdbool.h>
#include "preprocess.h"

/* Words with Tesseract confidence below this value (0-100) are discarded */
#define OCR_CONFIDENCE_THRESHOLD 70

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Tesseract OCR engine with English language data.
 * @return true on success, false on error
 */
bool ocr_init(void);

/**
 * @brief Extract text from a preprocessed image.
 *
 * Words with confidence below OCR_CONFIDENCE_THRESHOLD are discarded.
 * Returns an empty string (not NULL) when no text is detected.
 *
 * @param image Preprocessed image from preprocess_image()
 * @param text_out Output text (caller must free with ocr_free_text)
 * @return true on success, false on error
 */
bool ocr_extract(const struct preprocess_result *image, char **text_out);

/**
 * @brief Free text allocated by ocr_extract().
 * @param text Text to free (NULL-safe)
 */
void ocr_free_text(char *text);

/**
 * @brief Cleanup the Tesseract engine and free all resources.
 */
void ocr_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* OCR_H */
