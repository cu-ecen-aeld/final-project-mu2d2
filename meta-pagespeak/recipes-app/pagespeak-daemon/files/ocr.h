/**
 * @file   ocr.h
 * @brief  OCR interface for PageSpeak daemon.
 *
 * STUB: This interface will be implemented with Tesseract in a future issue.
 * The stub implementation returns placeholder text.
 */

#ifndef OCR_H
#define OCR_H

#include <stdbool.h>
#include "preprocess.h"

/**
 * @brief Initialize OCR engine.
 *
 * Future implementation will initialize Tesseract with English language data.
 *
 * @return true on success, false on error
 */
bool ocr_init(void);

/**
 * @brief Extract text from a preprocessed image.
 *
 * Future implementation will use Tesseract to perform OCR.
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
 * @brief Cleanup OCR engine and free resources.
 */
void ocr_cleanup(void);

#endif /* OCR_H */
