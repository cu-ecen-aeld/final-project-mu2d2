/**
 * @file   preprocess.c
 * @brief  OpenCV image preprocessing pipeline for PageSpeak OCR.
 *
 * Pipeline: JPEG decode, grayscale, Gaussian blur, deskew, adaptive threshold.
 *
 * Output is raw 8-bit single-channel (binary) pixels, width x height bytes,
 * suitable for Tesseract TessBaseAPI::SetImage().
 *
 * Compiled as C++ (-x c++) to use OpenCV 4 C++ API (cv::Mat).
 * The external interface (preprocess.h) remains C-compatible.
 */

#include "preprocess.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <syslog.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

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
static double detect_skew_angle(const cv::Mat &gray)
{
    cv::Mat edges;
    std::vector<cv::Vec2f> lines;
    double angle_sum = 0.0;
    int count = 0;

    cv::Canny(gray, edges, 50, 150, 3);
    cv::HoughLines(edges, lines, 1, CV_PI / 180.0, 80);

    int limit = (int)lines.size() < 50 ? (int)lines.size() : 50;
    for (int i = 0; i < limit; i++) {
        double theta     = lines[i][1];
        // Convert to deviation from horizontal (theta = π/2 means 0° skew)
        double angle_deg = (theta - CV_PI / 2.0) * 180.0 / CV_PI;
        if (std::fabs(angle_deg) <= DESKEW_MAX_ANGLE_DEG) {
            angle_sum += angle_deg;
            count++;
        }
    }

    return count > 0 ? angle_sum / count : 0.0;
}

/*
 * deskew_image: rotate src by -angle_deg around its center.
 *
 * White (255) fills any exposed border region.
 */
static cv::Mat deskew_image(const cv::Mat &src, double angle_deg)
{
    cv::Point2f center(src.cols / 2.0f, src.rows / 2.0f);
    cv::Mat rot = cv::getRotationMatrix2D(center, -angle_deg, 1.0);
    cv::Mat dst;
    cv::warpAffine(src, dst, rot, src.size(),
                   cv::INTER_LINEAR | cv::WARP_FILL_OUTLIERS,
                   cv::BORDER_CONSTANT, cv::Scalar(255));
    return dst;
}

bool preprocess_image(const struct capture_frame *frame,
                      struct preprocess_result *result)
{
    if (!frame || !frame->data || frame->size == 0 || !result) {
        syslog(LOG_ERR, "preprocess_image: invalid arguments");
        return false;
    }

    result->data   = NULL;
    result->size   = 0;
    result->width  = 0;
    result->height = 0;

    // Step 1: Decode JPEG from memory buffer
    cv::Mat jpeg_buf(1, (int)frame->size, CV_8UC1, (void *)frame->data);
    cv::Mat decoded = cv::imdecode(jpeg_buf, cv::IMREAD_COLOR);
    if (decoded.empty()) {
        syslog(LOG_ERR, "preprocess_image: imdecode failed");
        return false;
    }

    // Step 2: Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(decoded, gray, cv::COLOR_BGR2GRAY);

    // Step 3: Gaussian blur, noise reduction
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0);

    // Step 4: Deskew, detect and correct tilt up to ±15°
    double skew_deg = detect_skew_angle(blurred);
    syslog(LOG_DEBUG, "preprocess_image: detected skew %.2f degrees", skew_deg);

    cv::Mat deskewed;
    if (std::fabs(skew_deg) >= DESKEW_MIN_ANGLE_DEG)
        deskewed = deskew_image(blurred, skew_deg);
    else
        deskewed = blurred; // no correction needed

    // Step 5: Adaptive threshold, binarize text vs background
    cv::Mat binary;
    cv::adaptiveThreshold(deskewed, binary,
                          255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY,
                          THRESH_BLOCK_SIZE,
                          THRESH_C);

    // Step 6: Copy pixels to output buffer
    size_t pixel_count = (size_t)(binary.cols * binary.rows);
    result->data = (unsigned char *)std::malloc(pixel_count);
    if (!result->data) {
        syslog(LOG_ERR, "preprocess_image: malloc failed");
        return false;
    }

    std::memcpy(result->data, binary.data, pixel_count);
    result->size   = pixel_count;
    result->width  = binary.cols;
    result->height = binary.rows;

    syslog(LOG_INFO, "preprocess_image: done %dx%d skew=%.2f deg",
           result->width, result->height, skew_deg);

    return true;
}

void preprocess_free(struct preprocess_result *result)
{
    if (result && result->data) {
        std::free(result->data);
        result->data   = NULL;
        result->size   = 0;
        result->width  = 0;
        result->height = 0;
    }
}

