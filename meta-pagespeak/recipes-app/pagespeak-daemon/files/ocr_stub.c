/**
 * @file   ocr_stub.c
 * @brief  STUB OCR implementation for PageSpeak daemon.
 *
 * This stub returns placeholder text without actual OCR.
 * Replace with Tesseract implementation in future issue.
 *
 * TODO: Implement with Tesseract:
 * - TessBaseAPI::Init() with "eng" language
 * - TessBaseAPI::SetImage() with preprocessed image data
 * - TessBaseAPI::GetUTF8Text() to extract text
 */

#include "ocr.h"
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

/* Placeholder text returned by stub */
static const char *STUB_TEXT = "[OCR STUB: Tesseract not implemented]";

bool ocr_init(void)
{
    syslog(LOG_INFO, "ocr_init: STUB - Tesseract not implemented");
    return true;
}

bool ocr_extract(const struct preprocess_result *image, char **text_out)
{
    size_t len;

    if (!image || !text_out) {
        return false;
    }

    *text_out = NULL;

    /* STUB: Return placeholder text */
    len = strlen(STUB_TEXT);
    *text_out = malloc(len + 1);
    if (!*text_out) {
        syslog(LOG_ERR, "ocr_extract: malloc failed");
        return false;
    }

    memcpy(*text_out, STUB_TEXT, len + 1);

    syslog(LOG_INFO, "ocr_extract: STUB - returned placeholder text (input was %zu bytes)",
           image->size);
    return true;
}

void ocr_free_text(char *text)
{
    free(text);  /* free(NULL) is safe */
}

void ocr_cleanup(void)
{
    syslog(LOG_INFO, "ocr_cleanup: STUB - nothing to clean up");
}
