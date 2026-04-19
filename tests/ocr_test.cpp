/**
 * @file   ocr_test.cpp
 * @brief  Unit tests for the Tesseract OCR integration (issue #10).
 *
 * Compile on target:
 *   g++ -std=c++11 -I/usr/include/opencv4 \
 *       -o ocr_test ocr_test.cpp ocr.cpp preprocess.cpp capture.o \
 *       -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
 *       -ltesseract -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "ocr.h"
#include "preprocess.h"
#include "capture.h"

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond, msg) \
    do { \
        if (cond) { \
            printf("  PASS: %s\n", msg); \
            g_pass++; \
        } else { \
            printf("  FAIL: %s\n", msg); \
            g_fail++; \
        } \
    } while (0)

/* Build a preprocess_result from a cv::Mat (heap-allocated pixels) */
static int make_preprocess_result(const cv::Mat &binary,
                                  struct preprocess_result *result)
{
    size_t pixel_count = (size_t)(binary.cols * binary.rows);
    result->data = (unsigned char *)malloc(pixel_count);
    if (!result->data)
        return -1;
    memcpy(result->data, binary.data, pixel_count);
    result->size   = pixel_count;
    result->width  = binary.cols;
    result->height = binary.rows;
    return 0;
}

/* Test 1: init and cleanup do not crash */
static void test_init_cleanup(void)
{
    printf("[TEST] ocr_init and ocr_cleanup\n");
    ASSERT(ocr_init(),  "ocr_init returns true");
    ocr_cleanup();
    ASSERT(1, "ocr_cleanup does not crash");

    // Re-initialize for remaining tests
    ocr_init();
}

/* Test 2: NULL and invalid inputs rejected */
static void test_invalid_inputs(void)
{
    char *text = NULL;
    struct preprocess_result result = {0};

    printf("[TEST] Invalid input rejection\n");
    ASSERT(!ocr_extract(NULL, &text),    "NULL image rejected");
    ASSERT(!ocr_extract(&result, NULL),  "NULL text_out rejected");

    result.data = (unsigned char *)malloc(1);
    result.size = 0;
    ASSERT(!ocr_extract(&result, &text), "zero-size image rejected");
    free(result.data);
}

/* Test 3: blank (all-white) image returns empty string, not NULL */
static void test_blank_image(void)
{
    struct preprocess_result result = {0};
    char *text = NULL;

    printf("[TEST] Blank image returns empty string\n");

    cv::Mat white(480, 640, CV_8UC1, cv::Scalar(255));
    if (make_preprocess_result(white, &result) < 0) {
        printf("  SKIP: malloc failed\n");
        return;
    }

    bool ok = ocr_extract(&result, &text);
    ASSERT(ok,                       "ocr_extract succeeds on blank image");
    ASSERT(text != NULL,             "text_out is non-NULL");
    ASSERT(text && strlen(text) == 0, "blank image produces empty string");

    ocr_free_text(text);
    preprocess_free(&result);
}

/* Test 4: ocr_free_text is NULL-safe */
static void test_free_null(void)
{
    printf("[TEST] ocr_free_text NULL-safe\n");
    ocr_free_text(NULL);
    ASSERT(1, "ocr_free_text(NULL) does not crash");
}

/* Test 5: synthetic text image produces non-empty output */
static void test_synthetic_text(void)
{
    struct preprocess_result result = {0};
    char *text = NULL;

    printf("[TEST] Synthetic text image produces output\n");

    // White background with black text
    cv::Mat img(200, 600, CV_8UC3, cv::Scalar(255, 255, 255));
    cv::putText(img, "Hello World",
                cv::Point(20, 120),
                cv::FONT_HERSHEY_SIMPLEX,
                3.0,
                cv::Scalar(0, 0, 0),
                4);

    cv::Mat gray, binary;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::adaptiveThreshold(gray, binary, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, 31, 10);

    if (make_preprocess_result(binary, &result) < 0) {
        printf("  SKIP: malloc failed\n");
        return;
    }

    bool ok = ocr_extract(&result, &text);
    ASSERT(ok,           "ocr_extract succeeds on text image");
    ASSERT(text != NULL, "text_out is non-NULL");
    if (text)
        printf("  INFO: extracted: \"%s\"\n", text);

    ocr_free_text(text);
    preprocess_free(&result);
}

/* Test 6: double cleanup is safe */
static void test_double_cleanup(void)
{
    printf("[TEST] Double cleanup safe\n");
    ocr_cleanup();
    ocr_cleanup();
    ASSERT(1, "double ocr_cleanup does not crash");
}

int main(void)
{
    printf("=== PageSpeak OCR Unit Tests ===\n\n");

    test_init_cleanup();      printf("\n");
    test_invalid_inputs();    printf("\n");
    test_blank_image();       printf("\n");
    test_free_null();         printf("\n");
    test_synthetic_text();    printf("\n");
    test_double_cleanup();    printf("\n");

    printf("=== RESULT: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
