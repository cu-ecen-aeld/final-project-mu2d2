/**
 * @file   opencv-validate.c
 * @brief  DoD validation binary for issue #6.
 *         Links against libopencv_core.so and libopencv_imgproc.so to confirm
 *         OpenCV is correctly cross-compiled and present in the target rootfs.
 *
 *         Creates a small greyscale Mat, applies a Gaussian blur, and prints
 *         the result dimensions. A successful run proves the shared libraries
 *         load and basic image operations work on the target hardware.
 *
 *         Usage: opencv-validate
 *         Expected output: "opencv-validate: OK <cols>x<rows>"
 */

#include <stdio.h>
#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>

int main(void)
{
    // Allocate a 64x64 single-channel 8-bit image filled with a test value
    IplImage *src = cvCreateImage(cvSize(64, 64), IPL_DEPTH_8U, 1);
    IplImage *dst = cvCreateImage(cvSize(64, 64), IPL_DEPTH_8U, 1);

    if (!src || !dst) {
        fprintf(stderr, "opencv-validate: FAIL, cvCreateImage returned NULL\n");
        return 1;
    }

    cvSet(src, cvScalarAll(128), NULL);

    // Apply a 5x5 Gaussian blur, exercises libopencv_imgproc
    cvSmooth(src, dst, CV_GAUSSIAN, 5, 5, 0, 0);

    printf("opencv-validate: OK %dx%d\n", dst->width, dst->height);

    cvReleaseImage(&src);
    cvReleaseImage(&dst);
    return 0;
}
