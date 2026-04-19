/**
 * @file   preprocess_test.cpp
 * @brief  Unit tests for the OpenCV preprocessing pipeline (issue #9).
 *
 * Tests each preprocessing step independently by synthesizing JPEG input
 * frames in memory using cv::imencode.
 *
 * Compile on target:
 *   g++ -std=c++11 -I/usr/include/opencv4 \
 *       -o preprocess_test preprocess_test.cpp preprocess.cpp capture.o \
 *       -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lm
 *
 * Or cross-compile with Yocto SDK:
 *   $CXX -std=c++11 -I${SDKTARGETSYSROOT}/usr/include/opencv4 \
 *       -o preprocess_test preprocess_test.cpp preprocess.cpp capture.o \
 *       -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/imgcodecs.hpp>

#include "capture.h"
#include "preprocess.h"

/* Test infrastructure */

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

/* Build a capture_frame containing a JPEG-encoded synthetic image.
 * Caller must call capture_free() on the returned frame. */
static int make_jpeg_frame(IplImage *src, struct capture_frame *frame)
{
    std::vector<uchar> buf;
    std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 95 };
    cv::Mat mat(cv::Size(src->width, src->height), CV_8UC(src->nChannels),
                src->imageData, src->widthStep);

    frame->data = NULL;
    frame->size = 0;

    if (!cv::imencode(".jpg", mat, buf, params))
        return -1;

    frame->data = (unsigned char *)malloc(buf.size());
    if (!frame->data)
        return -1;

    memcpy(frame->data, buf.data(), buf.size());
    frame->size = buf.size();
    return 0;
}

/* Test 1: null and bad inputs are rejected */
static void test_invalid_inputs(void)
{
    struct preprocess_result result;
    struct capture_frame frame;
    unsigned char dummy = 0x00;

    printf("[TEST] Invalid input rejection\n");

    ASSERT(!preprocess_image(NULL, &result),
           "NULL frame pointer rejected");

    frame.data = NULL;
    frame.size = 0;
    ASSERT(!preprocess_image(&frame, NULL),
           "NULL result pointer rejected");

    frame.data = &dummy;
    frame.size = 0;
    ASSERT(!preprocess_image(&frame, &result),
           "zero-size frame rejected");
}

/* Test 2: JPEG decode */
static void test_jpeg_decode(void)
{
    IplImage *src;
    struct capture_frame frame;
    struct preprocess_result result;

    printf("[TEST] JPEG decode\n");

    src = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
    cvSet(src, cvScalar(100, 150, 200, 0), NULL);

    if (make_jpeg_frame(src, &frame) < 0) {
        printf("  SKIP: cvEncodeImage unavailable\n");
        cvReleaseImage(&src);
        return;
    }
    cvReleaseImage(&src);

    ASSERT(preprocess_image(&frame, &result), "preprocess_image succeeds on valid JPEG");
    ASSERT(result.width  == 320,              "output width matches input");
    ASSERT(result.height == 240,              "output height matches input");
    ASSERT(result.data   != NULL,             "output data pointer is non-NULL");
    ASSERT(result.size   == 320 * 240,        "output size equals width*height (8-bit pixels)");

    preprocess_free(&result);
    capture_free(&frame);
}

/* Test 3: grayscale and threshold, output is binary (0 or 255 only) */
static void test_binary_output(void)
{
    IplImage *src;
    struct capture_frame frame;
    struct preprocess_result result;
    size_t i;
    int non_binary = 0;

    printf("[TEST] Binary (thresholded) output\n");

    src = cvCreateImage(cvSize(64, 64), IPL_DEPTH_8U, 3);
    cvSet(src, cvScalar(200, 200, 200, 0), NULL);

    if (make_jpeg_frame(src, &frame) < 0) {
        printf("  SKIP: cvEncodeImage unavailable\n");
        cvReleaseImage(&src);
        return;
    }
    cvReleaseImage(&src);

    if (!preprocess_image(&frame, &result)) {
        printf("  FAIL: preprocess_image returned false\n");
        g_fail++;
        capture_free(&frame);
        return;
    }

    for (i = 0; i < result.size; i++) {
        if (result.data[i] != 0 && result.data[i] != 255)
            non_binary++;
    }

    ASSERT(non_binary == 0, "all output pixels are 0 or 255 (binary image)");

    preprocess_free(&result);
    capture_free(&frame);
}

/* Test 4: large input does not overflow */
static void test_large_frame(void)
{
    IplImage *src;
    struct capture_frame frame;
    struct preprocess_result result;

    printf("[TEST] Large frame (640x480)\n");

    src = cvCreateImage(cvSize(640, 480), IPL_DEPTH_8U, 3);
    cvSet(src, cvScalarAll(128), NULL);

    if (make_jpeg_frame(src, &frame) < 0) {
        printf("  SKIP: cvEncodeImage unavailable\n");
        cvReleaseImage(&src);
        return;
    }
    cvReleaseImage(&src);

    ASSERT(preprocess_image(&frame, &result), "640x480 frame processed successfully");
    ASSERT(result.size == 640 * 480,          "output size correct for 640x480");

    preprocess_free(&result);
    capture_free(&frame);
}

/* Test 5: preprocess_free clears all fields */
static void test_free(void)
{
    struct preprocess_result result;
    unsigned char dummy_data[4] = {0};

    printf("[TEST] preprocess_free\n");

    result.data   = dummy_data;
    result.size   = 4;
    result.width  = 2;
    result.height = 2;

    /* preprocess_free must not crash and should zero all fields */
    preprocess_free(&result);
    ASSERT(result.data   == NULL, "data pointer nulled after free");
    ASSERT(result.size   == 0,    "size zeroed after free");
    ASSERT(result.width  == 0,    "width zeroed after free");
    ASSERT(result.height == 0,    "height zeroed after free");

    /* Calling free a second time must be safe (no double-free) */
    preprocess_free(&result);
    ASSERT(1, "double-free is safe (no crash)");
}

/* Test 6: skewed image still produces correct dimensions */
static void test_deskew_dimensions(void)
{
    IplImage *src;
    IplImage *rot_src;
    CvMat *rot_mat;
    CvPoint2D32f center;
    struct capture_frame frame;
    struct preprocess_result result;

    printf("[TEST] Deskew — output dimensions preserved\n");

    src = cvCreateImage(cvSize(320, 240), IPL_DEPTH_8U, 3);
    cvSet(src, cvScalarAll(180), NULL);

    // Draw a horizontal black line to give HoughLines something to find
    cvLine(src,
           cvPoint(10, 120), cvPoint(310, 120 + 20), /* ~5° tilt */
           cvScalar(0, 0, 0, 0), 3, 8, 0);

    // Pre-rotate the image by 8° to simulate skew
    rot_src = cvCreateImage(cvGetSize(src), IPL_DEPTH_8U, 3);
    center.x = src->width  / 2.0f;
    center.y = src->height / 2.0f;
    rot_mat  = cvCreateMat(2, 3, CV_32FC1);
    cv2DRotationMatrix(center, 8.0, 1.0, rot_mat);
    cvWarpAffine(src, rot_src, rot_mat,
                 CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS,
                 cvScalarAll(255));
    cvReleaseMat(&rot_mat);
    cvReleaseImage(&src);

    if (make_jpeg_frame(rot_src, &frame) < 0) {
        printf("  SKIP: cvEncodeImage unavailable\n");
        cvReleaseImage(&rot_src);
        return;
    }
    cvReleaseImage(&rot_src);

    ASSERT(preprocess_image(&frame, &result),
           "skewed frame processed without error");
    ASSERT(result.width == 320 && result.height == 240,
           "output dimensions unchanged after deskew");

    preprocess_free(&result);
    capture_free(&frame);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== PageSpeak Preprocess Unit Tests ===\n\n");

    test_invalid_inputs();     printf("\n");
    test_jpeg_decode();        printf("\n");
    test_binary_output();      printf("\n");
    test_large_frame();        printf("\n");
    test_free();               printf("\n");
    test_deskew_dimensions();  printf("\n");

    printf("=== RESULT: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
