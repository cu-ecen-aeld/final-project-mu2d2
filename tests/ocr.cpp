/**
 * @file   ocr.cpp
 * @brief  Tesseract OCR implementation for PageSpeak daemon.
 *
 * Uses the Tesseract C API (tesseract/capi.h) to extract text from
 * preprocessed 8-bit binary images. Words with confidence below
 * OCR_CONFIDENCE_THRESHOLD are discarded before returning.
 *
 * The external interface (ocr.h) is C-compatible via extern "C" guards.
 */

#include "ocr.h"

#include <cstdlib>
#include <cstring>
#include <syslog.h>

#include <tesseract/capi.h>

/* Tessdata prefix: where Tesseract looks for language data files */
#define TESSDATA_PREFIX "/usr/share/tessdata"

/* Tesseract page segmentation mode: fully automatic layout analysis */
#define PSM_AUTO 3

static TessBaseAPI *g_tess = NULL;

bool ocr_init(void)
{
    if (g_tess) {
        syslog(LOG_WARNING, "ocr_init: already initialized");
        return true;
    }

    g_tess = TessBaseAPICreate();
    if (!g_tess) {
        syslog(LOG_ERR, "ocr_init: TessBaseAPICreate failed");
        return false;
    }

    if (TessBaseAPIInit3(g_tess, TESSDATA_PREFIX, "eng") != 0) {
        syslog(LOG_ERR, "ocr_init: TessBaseAPIInit3 failed (tessdata at %s)",
               TESSDATA_PREFIX);
        TessBaseAPIDelete(g_tess);
        g_tess = NULL;
        return false;
    }

    TessBaseAPISetPageSegMode(g_tess, (TessPageSegMode)PSM_AUTO);
    syslog(LOG_INFO, "ocr_init: Tesseract initialized (eng, PSM_AUTO)");
    return true;
}

bool ocr_extract(const struct preprocess_result *image, char **text_out)
{
    TessResultIterator *iter = NULL;
    char *raw_text           = NULL;
    char *out_buf            = NULL;
    size_t out_len           = 0;
    size_t out_cap           = 4096;
    bool success             = false;

    if (!image || !image->data || image->size == 0 || !text_out) {
        syslog(LOG_ERR, "ocr_extract: invalid arguments");
        return false;
    }

    if (!g_tess) {
        syslog(LOG_ERR, "ocr_extract: OCR not initialized, call ocr_init first");
        return false;
    }

    *text_out = NULL;

    // Feed the preprocessed 8-bit single-channel binary image to Tesseract
    TessBaseAPISetImage(g_tess,
                        image->data,
                        image->width,
                        image->height,
                        1,              /* bytes per pixel: 1 (grayscale) */
                        image->width);  /* bytes per line */

    // Run recognition
    if (TessBaseAPIRecognize(g_tess, NULL) != 0) {
        syslog(LOG_ERR, "ocr_extract: TessBaseAPIRecognize failed");
        goto cleanup;
    }

    // Allocate output buffer
    out_buf = (char *)malloc(out_cap);
    if (!out_buf) {
        syslog(LOG_ERR, "ocr_extract: malloc failed");
        goto cleanup;
    }
    out_buf[0] = '\0';

    // Iterate words and keep only those above the confidence threshold
    iter = TessBaseAPIGetIterator(g_tess);
    if (iter) {
        do {
            float conf = TessResultIteratorConfidence(iter, RIL_WORD);
            if (conf >= OCR_CONFIDENCE_THRESHOLD) {
                char *word = TessResultIteratorGetUTF8Text(iter, RIL_WORD);
                if (word) {
                    size_t word_len = strlen(word);
                    // Grow buffer if needed (word + space + NUL)
                    if (out_len + word_len + 2 > out_cap) {
                        out_cap = (out_len + word_len + 2) * 2;
                        char *tmp = (char *)realloc(out_buf, out_cap);
                        if (!tmp) {
                            TessDeleteText(word);
                            syslog(LOG_ERR, "ocr_extract: realloc failed");
                            goto cleanup;
                        }
                        out_buf = tmp;
                    }
                    memcpy(out_buf + out_len, word, word_len);
                    out_len += word_len;
                    out_buf[out_len++] = ' ';
                    out_buf[out_len]   = '\0';
                    TessDeleteText(word);
                }
            }
        } while (TessResultIteratorNext(iter, RIL_WORD));

        TessResultIteratorDelete(iter);
        iter = NULL;
    }

    // Strip trailing space
    while (out_len > 0 && out_buf[out_len - 1] == ' ')
        out_buf[--out_len] = '\0';

    *text_out = out_buf;
    out_buf   = NULL;

    syslog(LOG_INFO, "ocr_extract: extracted %zu chars from %dx%d image",
           out_len, image->width, image->height);
    success = true;

cleanup:
    if (iter)
        TessResultIteratorDelete(iter);
    if (raw_text)
        TessDeleteText(raw_text);
    free(out_buf);
    TessBaseAPIClear(g_tess);
    return success;
}

void ocr_free_text(char *text)
{
    free(text);  /* free(NULL) is safe */
}

void ocr_cleanup(void)
{
    if (g_tess) {
        TessBaseAPIEnd(g_tess);
        TessBaseAPIDelete(g_tess);
        g_tess = NULL;
        syslog(LOG_INFO, "ocr_cleanup: Tesseract released");
    }
}
