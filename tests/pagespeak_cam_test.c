/*
 * pagespeak_cam_test.c — Userspace test for /dev/pagespeak-cam
 *
 * Usage: ./pagespeak_cam_test [output_file.jpg]
 *
 * Opens the device, queries capabilities, optionally sets resolution,
 * reads one frame, saves it to a file, and closes.
 *
 * Compile on target:
 *   gcc -o pagespeak_cam_test pagespeak_cam_test.c
 *
 * Or cross-compile with Yocto SDK:
 *   $CC -o pagespeak_cam_test pagespeak_cam_test.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "pagespeak_cam.h"

#define DEVICE_PATH "/dev/pagespeak-cam"
#define DEFAULT_OUTPUT "captured_frame.jpg"
#define FRAME_BUF_SIZE (2 * 1024 * 1024)  /* 2MB — matches kernel module */

/* V4L2_PIX_FMT_MJPEG fourcc value */
#define MJPEG_FOURCC 0x47504A4D

static int test_query_caps(int fd)
{
    struct pagespeak_cam_caps caps;
    int ret;

    printf("[TEST] Querying capabilities...\n");
    ret = ioctl(fd, PAGESPEAK_CAM_QUERY_CAPS, &caps);
    if (ret < 0) {
        perror("  FAIL: PAGESPEAK_CAM_QUERY_CAPS");
        return -1;
    }

    printf("  Device: %s\n", caps.device_name);
    printf("  Resolution: %ux%u (min %ux%u, max %ux%u)\n",
           caps.current_width, caps.current_height,
           caps.min_width, caps.min_height,
           caps.max_width, caps.max_height);
    printf("  Pixel format: 0x%08x\n", caps.current_pixelformat);
    printf("  PASS: capabilities queried\n");
    return 0;
}

static int test_set_resolution(int fd, uint32_t width, uint32_t height)
{
    struct pagespeak_cam_resolution res;
    int ret;

    printf("[TEST] Setting resolution to %ux%u...\n", width, height);
    res.width = width;
    res.height = height;

    ret = ioctl(fd, PAGESPEAK_CAM_SET_RESOLUTION, &res);
    if (ret < 0) {
        perror("  FAIL: PAGESPEAK_CAM_SET_RESOLUTION");
        return -1;
    }

    printf("  PASS: resolution set\n");
    return 0;
}

static int test_set_pixfmt(int fd, uint32_t pixfmt)
{
    struct pagespeak_cam_pixfmt pf;
    int ret;

    printf("[TEST] Setting pixel format to 0x%08x...\n", pixfmt);
    pf.pixelformat = pixfmt;

    ret = ioctl(fd, PAGESPEAK_CAM_SET_PIXFMT, &pf);
    if (ret < 0) {
        perror("  FAIL: PAGESPEAK_CAM_SET_PIXFMT");
        return -1;
    }

    printf("  PASS: pixel format set\n");
    return 0;
}

static int test_concurrent_open(void)
{
    int fd1, fd2;

    printf("[TEST] Testing concurrent open rejection...\n");

    fd1 = open(DEVICE_PATH, O_RDWR);
    if (fd1 < 0) {
        perror("  FAIL: first open");
        return -1;
    }

    fd2 = open(DEVICE_PATH, O_RDWR);
    if (fd2 >= 0) {
        printf("  FAIL: second open should have returned -EBUSY\n");
        close(fd2);
        close(fd1);
        return -1;
    }

    if (errno != EBUSY) {
        printf("  FAIL: expected EBUSY, got %s\n", strerror(errno));
        close(fd1);
        return -1;
    }

    printf("  PASS: second open correctly returned EBUSY\n");
    close(fd1);
    return 0;
}

static ssize_t test_read_frame(int fd, void *buf, size_t bufsize)
{
    ssize_t nread;

    printf("[TEST] Reading frame...\n");
    nread = read(fd, buf, bufsize);
    if (nread < 0) {
        perror("  FAIL: read");
        return -1;
    }

    if (nread == 0) {
        printf("  FAIL: read returned 0 bytes\n");
        return -1;
    }

    printf("  PASS: read %zd bytes\n", nread);
    return nread;
}

static int save_frame(const char *path, const void *data, size_t size)
{
    FILE *fp;

    printf("[TEST] Saving frame to %s...\n", path);
    fp = fopen(path, "wb");
    if (!fp) {
        perror("  FAIL: fopen");
        return -1;
    }

    if (fwrite(data, 1, size, fp) != size) {
        perror("  FAIL: fwrite");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    printf("  PASS: saved %zu bytes to %s\n", size, path);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *output_path = (argc > 1) ? argv[1] : DEFAULT_OUTPUT;
    void *frame_buf;
    ssize_t frame_size;
    int fd;
    int failures = 0;

    printf("=== PageSpeak Camera Driver Test ===\n");
    printf("Device: %s\n", DEVICE_PATH);
    printf("Output: %s\n\n", output_path);

    /* Test 1: Concurrent open rejection */
    if (test_concurrent_open() < 0)
        failures++;
    printf("\n");

    /* Test 2: Open, query caps, set resolution, set pixfmt */
    printf("[TEST] Opening %s for ioctl tests...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("  FAIL: open");
        printf("\n=== RESULT: FAIL (could not open device) ===\n");
        return 1;
    }
    printf("  PASS: device opened (fd=%d)\n\n", fd);

    /* Test 3: Query capabilities */
    if (test_query_caps(fd) < 0)
        failures++;
    printf("\n");

    /* Test 4: Set resolution (stored for next open's camera config) */
    if (test_set_resolution(fd, 640, 480) < 0)
        failures++;
    printf("\n");

    /* Test 5: Set pixel format (MJPEG) */
    if (test_set_pixfmt(fd, MJPEG_FOURCC) < 0)
        failures++;
    printf("\n");

    /* Verify settings were stored via QUERY_CAPS */
    printf("[TEST] Verifying settings via QUERY_CAPS...\n");
    {
        struct pagespeak_cam_caps caps;
        if (ioctl(fd, PAGESPEAK_CAM_QUERY_CAPS, &caps) == 0) {
            if (caps.current_width == 640 && caps.current_height == 480 &&
                caps.current_pixelformat == MJPEG_FOURCC) {
                printf("  PASS: settings match expected values\n");
            } else {
                printf("  FAIL: settings mismatch (%ux%u fmt=0x%08x)\n",
                       caps.current_width, caps.current_height,
                       caps.current_pixelformat);
                failures++;
            }
        } else {
            perror("  FAIL: QUERY_CAPS");
            failures++;
        }
    }
    printf("\n");

    /* Close and reopen for frame capture (open() applies stored settings) */
    close(fd);

    /* Test 6: Read a frame */
    printf("[TEST] Opening %s for frame capture...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("  FAIL: open for capture");
        printf("\n=== RESULT: FAIL (could not open device) ===\n");
        return 1;
    }
    printf("  PASS: device opened (fd=%d)\n\n", fd);

    frame_buf = malloc(FRAME_BUF_SIZE);
    if (!frame_buf) {
        perror("malloc");
        close(fd);
        return 1;
    }

    frame_size = test_read_frame(fd, frame_buf, FRAME_BUF_SIZE);
    if (frame_size > 0) {
        /* Test 5: Save frame to file */
        if (save_frame(output_path, frame_buf, frame_size) < 0)
            failures++;

        /* Basic JPEG validation: check for JPEG SOI marker */
        if (frame_size >= 2) {
            unsigned char *data = (unsigned char *)frame_buf;
            if (data[0] == 0xFF && data[1] == 0xD8) {
                printf("\n[TEST] JPEG validation...\n");
                printf("  PASS: valid JPEG SOI marker (0xFFD8)\n");
            } else {
                printf("\n[TEST] JPEG validation...\n");
                printf("  WARN: no JPEG SOI marker (got 0x%02x%02x)\n",
                       data[0], data[1]);
                failures++;
            }
        }
    } else {
        failures++;
    }
    printf("\n");

    free(frame_buf);

    /* Test 6: Close device */
    printf("[TEST] Closing device...\n");
    if (close(fd) < 0) {
        perror("  FAIL: close");
        failures++;
    } else {
        printf("  PASS: device closed\n");
    }

    printf("\n=== RESULT: %s (%d failures) ===\n",
           failures == 0 ? "ALL PASS" : "SOME FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
