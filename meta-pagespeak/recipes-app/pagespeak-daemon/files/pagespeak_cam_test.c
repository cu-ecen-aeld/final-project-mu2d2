/*
 * pagespeak_cam_test.c — Userspace test for /dev/pagespeak-cam
 *
 * Usage: ./pagespeak_cam_test [output_file.jpg]
 *
 * Opens /dev/pagespeak-cam for ioctl config and access control, then
 * captures one frame directly from the underlying V4L2 device using
 * standard mmap streaming. The raw device path is retrieved from
 * PAGESPEAK_CAM_QUERY_CAPS.
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
#include <sys/mman.h>
#include <poll.h>
#include <linux/videodev2.h>

#include "pagespeak_cam.h"

#define DEVICE_PATH     "/dev/pagespeak-cam"
#define DEFAULT_OUTPUT  "captured_frame.jpg"
#define FRAME_BUF_SIZE  (2 * 1024 * 1024)  /* 2MB */

#define MJPEG_FOURCC    0x47504A4D  /* V4L2_PIX_FMT_MJPEG */
#define CAPTURE_WIDTH   1280
#define CAPTURE_HEIGHT  720

static int test_query_caps(int fd, struct pagespeak_cam_caps *caps)
{
    int ret;

    printf("[TEST] Querying capabilities...\n");
    ret = ioctl(fd, PAGESPEAK_CAM_QUERY_CAPS, caps);
    if (ret < 0) {
        perror("  FAIL: PAGESPEAK_CAM_QUERY_CAPS");
        return -1;
    }

    printf("  Device: %s\n", caps->device_name);
    printf("  Raw device: %s\n", caps->raw_device_path);
    printf("  Resolution: %ux%u (min %ux%u, max %ux%u)\n",
           caps->current_width, caps->current_height,
           caps->min_width, caps->min_height,
           caps->max_width, caps->max_height);
    printf("  Pixel format: 0x%08x\n", caps->current_pixelformat);
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

#define NUM_BUFS    4   /* mmap buffer pool size */
#define WARMUP_FRAMES 30  /* frames to discard for auto-exposure to settle */

/*
 * Capture one frame from the raw V4L2 device using mmap streaming.
 * The pagespeak-cam fd must remain open during capture to hold the
 * exclusive access lock.
 */
static ssize_t capture_frame_mmap(const char *raw_dev,
                                   uint32_t width, uint32_t height,
                                   uint32_t pixfmt,
                                   void *out_buf, size_t out_size)
{
    int vfd = -1;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers reqbufs;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ssize_t frame_size = -1;
    struct pollfd pfd;

    printf("[TEST] Capturing frame via V4L2 mmap from %s...\n", raw_dev);

    vfd = open(raw_dev, O_RDWR);
    if (vfd < 0) {
        perror("  FAIL: open raw V4L2 device");
        return -1;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = pixfmt;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    if (ioctl(vfd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("  FAIL: VIDIOC_S_FMT");
        goto cleanup;
    }
    printf("  Format set: %ux%u pixfmt=0x%08x\n",
           fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat);

    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count  = 4;  /* allocate extra buffers for warm-up frames */
    reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (ioctl(vfd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        perror("  FAIL: VIDIOC_REQBUFS");
        goto cleanup;
    }

    /* Map all 4 buffers and track their lengths/addresses for munmap */
    void    *bufs[NUM_BUFS];
    uint32_t buf_lengths[NUM_BUFS];
    int      i;

    for (i = 0; i < NUM_BUFS; i++)
        bufs[i] = MAP_FAILED;

    for (i = 0; i < NUM_BUFS; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(vfd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("  FAIL: VIDIOC_QUERYBUF");
            goto cleanup;
        }
        buf_lengths[i] = buf.length;
        bufs[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                       MAP_SHARED, vfd, buf.m.offset);
        if (bufs[i] == MAP_FAILED) {
            perror("  FAIL: mmap");
            goto cleanup;
        }

        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(vfd, VIDIOC_QBUF, &buf) < 0) {
            perror("  FAIL: VIDIOC_QBUF");
            goto cleanup;
        }
    }

    if (ioctl(vfd, VIDIOC_STREAMON, &type) < 0) {
        perror("  FAIL: VIDIOC_STREAMON");
        goto cleanup;
    }

    /* Drain WARMUP_FRAMES frames, re-queuing each buffer so auto-exposure settles */
    printf("  Warming up: discarding %d frames...\n", WARMUP_FRAMES);
    for (i = 0; i < WARMUP_FRAMES; i++) {
        pfd.fd     = vfd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, 5000) <= 0) {
            printf("  FAIL: poll timeout on warm-up frame %d\n", i);
            ioctl(vfd, VIDIOC_STREAMOFF, &type);
            goto cleanup;
        }
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(vfd, VIDIOC_DQBUF, &buf) < 0) {
            perror("  FAIL: VIDIOC_DQBUF warm-up");
            ioctl(vfd, VIDIOC_STREAMOFF, &type);
            goto cleanup;
        }
        if (ioctl(vfd, VIDIOC_QBUF, &buf) < 0) {
            perror("  FAIL: VIDIOC_QBUF re-queue");
            ioctl(vfd, VIDIOC_STREAMOFF, &type);
            goto cleanup;
        }
    }

    // Capture the final settled frame (frame 31)
    pfd.fd     = vfd;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 5000) <= 0) {
        printf("  FAIL: poll timeout waiting for frame\n");
        ioctl(vfd, VIDIOC_STREAMOFF, &type);
        goto cleanup;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(vfd, VIDIOC_DQBUF, &buf) < 0) {
        perror("  FAIL: VIDIOC_DQBUF");
        ioctl(vfd, VIDIOC_STREAMOFF, &type);
        goto cleanup;
    }

    frame_size = (buf.bytesused < out_size) ? buf.bytesused : out_size;
    memcpy(out_buf, bufs[buf.index], frame_size);
    printf("  PASS: captured %zd bytes (buf index %u)\n", frame_size, buf.index);

    ioctl(vfd, VIDIOC_STREAMOFF, &type);

cleanup:
    for (i = 0; i < NUM_BUFS; i++) {
        if (bufs[i] && bufs[i] != MAP_FAILED)
            munmap(bufs[i], buf_lengths[i]);
    }
    if (vfd >= 0)
        close(vfd);
    return frame_size;
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
    struct pagespeak_cam_caps caps;
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

    /* Test 2: Open device */
    printf("[TEST] Opening %s...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("  FAIL: open");
        printf("\n=== RESULT: FAIL (could not open device) ===\n");
        return 1;
    }
    printf("  PASS: device opened (fd=%d)\n\n", fd);

    /* Test 3: Query capabilities (also retrieves raw_device_path) */
    memset(&caps, 0, sizeof(caps));
    if (test_query_caps(fd, &caps) < 0)
        failures++;
    printf("\n");

    /* Test 4: Set resolution */
    if (test_set_resolution(fd, CAPTURE_WIDTH, CAPTURE_HEIGHT) < 0)
        failures++;
    printf("\n");

    /* Test 5: Set pixel format */
    if (test_set_pixfmt(fd, MJPEG_FOURCC) < 0)
        failures++;
    printf("\n");

    /* Test 6: Verify settings were stored */
    printf("[TEST] Verifying settings via QUERY_CAPS...\n");
    {
        struct pagespeak_cam_caps verify;
        if (ioctl(fd, PAGESPEAK_CAM_QUERY_CAPS, &verify) == 0) {
            if (verify.current_width == CAPTURE_WIDTH &&
                verify.current_height == CAPTURE_HEIGHT &&
                verify.current_pixelformat == MJPEG_FOURCC) {
                printf("  PASS: settings match expected values\n");
            } else {
                printf("  FAIL: settings mismatch (%ux%u fmt=0x%08x)\n",
                       verify.current_width, verify.current_height,
                       verify.current_pixelformat);
                failures++;
            }
        } else {
            perror("  FAIL: QUERY_CAPS");
            failures++;
        }
    }
    printf("\n");

    /*
     * Test 7: Capture a frame via V4L2 mmap on the raw device.
     * /dev/pagespeak-cam remains open to hold the exclusive access lock.
     */
    frame_buf = malloc(FRAME_BUF_SIZE);
    if (!frame_buf) {
        perror("malloc");
        close(fd);
        return 1;
    }

    frame_size = capture_frame_mmap(caps.raw_device_path,
                                    CAPTURE_WIDTH, CAPTURE_HEIGHT, MJPEG_FOURCC,
                                    frame_buf, FRAME_BUF_SIZE);
    if (frame_size > 0) {
        if (save_frame(output_path, frame_buf, frame_size) < 0)
            failures++;

        printf("\n[TEST] JPEG validation...\n");
        if (frame_size >= 2) {
            unsigned char *data = (unsigned char *)frame_buf;
            if (data[0] == 0xFF && data[1] == 0xD8)
                printf("  PASS: valid JPEG SOI marker (0xFFD8)\n");
            else {
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

    /* Test 8: Close device */
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

