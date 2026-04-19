/**
 * @file   preprocess.c
 * @brief  OpenCV image preprocessing pipeline for PageSpeak OCR.
 *
 * Pipeline: JPEG decode → grayscale → Gaussian blur → deskew → adaptive threshold
 *
 * Output is raw 8-bit single-channel (binary) pixels, width × height bytes,
 * suitable for direct consumption by Tesseract TessBaseAPI::SetImage().
 *
 * Uses the OpenCV 2/3/4 C API (IplImage / cvXxx) so the translation unit
 * can remain a .c file compiled with a C++ compiler (-x c++ or .cpp rename
 * is NOT required; linking with -lstdc++ is sufficient).
 */

#include "preprocess.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>

// Skew angles smaller than this (degrees) are not corrected
#define DESKEW_MIN_ANGLE_DEG  0.5
// Skew angles larger than this (degrees) are ignored as noise
#define DESKEW_MAX_ANGLE_DEG  15.0
// Adaptive threshold block size (must be odd; larger = handles more lighting variation)
#define THRESH_BLOCK_SIZE     31
// Constant subtracted from weighted mean in adaptive threshold
#define THRESH_C              10

/*
 * detect_skew_angle: estimate document skew using Hough line transform.
 *
 * Returns the mean angle (degrees) of near-horizontal lines found in the
 * image, or 0.0 if no reliable lines are detected.
 */
static double detect_skew_angle(const IplImage *gray)
{
    IplImage *edges = cvCreateImage(cvGetSize(gray), IPL_DEPTH_8U, 1);
    CvMemStorage *storage = cvCreateMemStorage(0);
    CvSeq *lines;
    double angle_sum = 0.0;
    int count = 0;
    int i;

    cvCanny(gray, edges, 50, 150, 3);

    lines = cvHoughLines2(edges, storage, CV_HOUGH_STANDARD,
                          1,              // rho resolution: 1 pixel
                          CV_PI / 180.0,  // theta resolution: 1 degree
                          80,             // accumulator threshold
                          0, 0);

    if (lines) {
        int limit = lines->total < 50 ? lines->total : 50;
        for (i = 0; i < limit; i++) {
            float *line = (float *)cvGetSeqElem(lines, i);
            float theta = line[1]; /* angle of line normal (radians) */
            // Convert to deviation from horizontal (theta = π/2 → 0° skew)
            double angle_deg = (theta - CV_PI / 2.0) * 180.0 / CV_PI;
            if (fabs(angle_deg) <= DESKEW_MAX_ANGLE_DEG) {
                angle_sum += angle_deg;
                count++;
            }
        }
    }

    cvReleaseMemStorage(&storage);
    cvReleaseImage(&edges);

    return count > 0 ? angle_sum / count : 0.0;
}

/*
 * deskew_image: rotate src by -angle_deg around its center.
 *
 * White (255) is used to fill any exposed border region.
 * Caller is responsible for releasing the returned image.
 */
static IplImage *deskew_image(const IplImage *src, double angle_deg)
{
    CvPoint2D32f center;
    CvMat *rot;
    IplImage *dst;

    center.x = src->width  / 2.0f;
    center.y = src->height / 2.0f;

    rot = cvCreateMat(2, 3, CV_32FC1);
    cv2DRotationMatrix(center, -angle_deg, 1.0, rot);

    dst = cvCreateImage(cvGetSize(src), src->depth, src->nChannels);
    cvWarpAffine(src, dst, rot,
                 CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS,
                 cvScalarAll(255)); // white background

    cvReleaseMat(&rot);
    return dst;
}

bool preprocess_image(const struct capture_frame *frame,
                      struct preprocess_result *result)
{
    CvMat jpeg_mat;
    IplImage *decoded  = NULL;
    IplImage *gray     = NULL;
    IplImage *blurred  = NULL;
    IplImage *deskewed = NULL;
    IplImage *binary   = NULL;
    double skew_deg;
    size_t pixel_count;

    if (!frame || !frame->data || frame->size == 0 || !result) {
        syslog(LOG_ERR, "preprocess_image: invalid arguments");
        return false;
    }

    result->data   = NULL;
    result->size   = 0;
    result->width  = 0;
    result->height = 0;

    // Step 1: Decode JPEG from memory buffer
    jpeg_mat = cvMat(1, (int)frame->size, CV_8UC1, (void *)frame->data);
    decoded = cvDecodeImage(&jpeg_mat, CV_LOAD_IMAGE_COLOR);
    if (!decoded) {
        syslog(LOG_ERR, "preprocess_image: cvDecodeImage failed");
        return false;
    }

    // Step 2: Convert to grayscale
    gray = cvCreateImage(cvGetSize(decoded), IPL_DEPTH_8U, 1);
    cvCvtColor(decoded, gray, CV_BGR2GRAY);
    cvReleaseImage(&decoded);

    // Step 3: Gaussian blur, noise reduction
    blurred = cvCreateImage(cvGetSize(gray), IPL_DEPTH_8U, 1);
    cvSmooth(gray, blurred, CV_GAUSSIAN, 3, 3, 0, 0);
    cvReleaseImage(&gray);

    // Step 4: Deskew, detect and correct tilt up to ±15°
    skew_deg = detect_skew_angle(blurred);
    syslog(LOG_DEBUG, "preprocess_image: detected skew %.2f degrees", skew_deg);

    if (fabs(skew_deg) >= DESKEW_MIN_ANGLE_DEG) {
        deskewed = deskew_image(blurred, skew_deg);
        cvReleaseImage(&blurred);
    } else {
        deskewed = blurred; // no correction needed
    }

    // Step 5: Adaptive threshold, binarize text vs background
    binary = cvCreateImage(cvGetSize(deskewed), IPL_DEPTH_8U, 1);
    cvAdaptiveThreshold(deskewed, binary,
                        255,                             // max value
                        CV_ADAPTIVE_THRESH_GAUSSIAN_C,  // Gaussian weighting
                        CV_THRESH_BINARY,                // output: 0 or 255
                        THRESH_BLOCK_SIZE,
                        THRESH_C);
    cvReleaseImage(&deskewed);

    // Step 6: Copy pixels to output buffer
    pixel_count = (size_t)(binary->width * binary->height);
    result->data = (unsigned char *)malloc(pixel_count);
    if (!result->data) {
        syslog(LOG_ERR, "preprocess_image: malloc failed");
        cvReleaseImage(&binary);
        return false;
    }

    memcpy(result->data, binary->imageData, pixel_count);
    result->size   = pixel_count;
    result->width  = binary->width;
    result->height = binary->height;

    syslog(LOG_INFO, "preprocess_image: done %dx%d skew=%.2f deg",
           result->width, result->height, skew_deg);

    cvReleaseImage(&binary);
    return true;
}

void preprocess_free(struct preprocess_result *result)
{
    if (result && result->data) {
        free(result->data);
        result->data   = NULL;
        result->size   = 0;
        result->width  = 0;
        result->height = 0;
    }
}
