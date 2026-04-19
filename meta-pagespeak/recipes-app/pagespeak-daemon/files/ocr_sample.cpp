/**
 * @file   ocr_sample.cpp
 * @brief  Standalone tool: JPEG file → preprocess → OCR → stdout.
 *
 * Usage: ./ocr_sample <input.jpg>
 *
 * Prints extracted text and processing time. Use this to test against
 * sample images for issue #9/#10 documentation requirements.
 *
 * Compile on target:
 *   g++ -std=c++11 -I/usr/include/opencv4 \
 *       -o ocr_sample ocr_sample.cpp ocr.cpp preprocess.cpp capture.o \
 *       -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -ltesseract -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <opencv2/imgcodecs.hpp>

#include "preprocess.h"
#include "ocr.h"

static double elapsed_ms(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec  - start->tv_sec)  * 1000.0
         + (end->tv_nsec - start->tv_nsec) / 1e6;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.jpg> [output_binary.png]\n", argv[0]);
        return 1;
    }

    const char *input_path  = argv[1];
    const char *output_path = argc > 2 ? argv[2] : NULL;

    // Load JPEG into a capture_frame (raw bytes)
    FILE *fp = fopen(input_path, "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    struct capture_frame frame;
    frame.data = (unsigned char *)malloc(fsize);
    frame.size = fsize;
    if (!frame.data || fread(frame.data, 1, fsize, fp) != (size_t)fsize) {
        fprintf(stderr, "Failed to read %s\n", input_path);
        fclose(fp);
        free(frame.data);
        return 1;
    }
    fclose(fp);

    printf("Input: %s (%ld bytes)\n", input_path, fsize);

    // Step 1: Preprocess
    struct timespec t0, t1, t2;
    struct preprocess_result preprocessed = {0};

    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (!preprocess_image(&frame, &preprocessed)) {
        fprintf(stderr, "preprocess_image failed\n");
        free(frame.data);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    printf("Preprocess: %dx%d binary image, %.1f ms\n",
           preprocessed.width, preprocessed.height,
           elapsed_ms(&t0, &t1));

    // Optionally save the binary (preprocessed) image
    if (output_path) {
        cv::Mat out(preprocessed.height, preprocessed.width, CV_8UC1,
                    preprocessed.data);
        cv::imwrite(output_path, out);
        printf("Saved preprocessed image: %s\n", output_path);
    }

    // Step 2: OCR
    if (!ocr_init()) {
        fprintf(stderr, "ocr_init failed\n");
        preprocess_free(&preprocessed);
        free(frame.data);
        return 1;
    }

    char *text = NULL;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    bool ok = ocr_extract(&preprocessed, &text);
    clock_gettime(CLOCK_MONOTONIC, &t2);

    printf("OCR: %.1f ms\n", elapsed_ms(&t1, &t2));
    printf("Total: %.1f ms\n", elapsed_ms(&t0, &t2));
    printf("\n--- Extracted Text ---\n%s\n----------------------\n",
           (text && text[0]) ? text : "(no text detected)");

    ocr_free_text(text);
    ocr_cleanup();
    preprocess_free(&preprocessed);
    free(frame.data);

    return ok ? 0 : 1;
}
