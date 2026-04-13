// SPDX-License-Identifier: GPL-2.0-only
/*
 * pagespeak_cam.c — Character device driver for USB webcam frame capture
 *
 * Creates /dev/pagespeak-cam. Captures JPEG frames from a UVC webcam
 * using kernel_read() on the video device (UVC read mode). Follows
 * aesdchar patterns: cdev registration, file_operations, copy_to_user,
 * mutex protection.
 *
 * Design note (ARM 5.15): set_fs() is removed on ARM 5.15+, so we
 * cannot issue V4L2 ioctls from kernel space (they use copy_from_user
 * internally). Instead we use kernel_read() which calls the video
 * device's .read file_operation — the UVC driver supports this when
 * V4L2_CAP_READWRITE is advertised. Camera format is configured from
 * userspace via v4l2-ctl before opening /dev/pagespeak-cam, or via
 * call_usermodehelper() on open().
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/kmod.h>

#include "pagespeak_cam.h"

#define DEVICE_NAME     "pagespeak-cam"
#define CLASS_NAME      "pagespeak-cam"
#define VIDEO_DEV_PATH  "/dev/pagespeak-cam-raw"

/* Maximum frame buffer size: 2MB (generous for 1080p MJPEG) */
#define MAX_FRAME_SIZE  (2 * 1024 * 1024)

/* Default capture parameters */
#define DEFAULT_WIDTH   640
#define DEFAULT_HEIGHT  480
#define DEFAULT_PIXFMT  0x47504A4D  /* V4L2_PIX_FMT_MJPEG = 'MJPG' */

struct pagespeak_cam_dev {
    struct cdev cdev;
    struct class *dev_class;
    struct device *device;
    dev_t dev_num;

    struct mutex lock;
    bool is_open;

    /* V4L2 state */
    struct file *video_filp;
    uint32_t width;
    uint32_t height;
    uint32_t pixelformat;

    /* Frame buffer */
    void *frame_buf;
};

static struct pagespeak_cam_dev *pcam_dev;

/*
 * ============================================================
 * Helpers
 * ============================================================
 */

/*
 * Configure the USB camera format from kernel space using
 * call_usermodehelper to run v4l2-ctl. This works on ARM 5.15
 * where kernel-space V4L2 ioctls are not possible.
 */
static int configure_camera_format(uint32_t width, uint32_t height,
                                   uint32_t pixfmt)
{
    char resolution_str[64];
    char pixfmt_str[8];
    char *argv[] = {
        "/usr/bin/v4l2-ctl",
        "-d", VIDEO_DEV_PATH,
        "--set-fmt-video",
        resolution_str,
        NULL
    };
    char *envp[] = {
        "HOME=/",
        "PATH=/sbin:/bin:/usr/sbin:/usr/bin",
        NULL
    };
    int ret;

    /* Format: width=W,height=H,pixelformat=MJPG */
    pixfmt_str[0] = (pixfmt >>  0) & 0xFF;
    pixfmt_str[1] = (pixfmt >>  8) & 0xFF;
    pixfmt_str[2] = (pixfmt >> 16) & 0xFF;
    pixfmt_str[3] = (pixfmt >> 24) & 0xFF;
    pixfmt_str[4] = '\0';

    snprintf(resolution_str, sizeof(resolution_str),
             "width=%u,height=%u,pixelformat=%s",
             width, height, pixfmt_str);

    pr_info("pagespeak_cam: configuring camera: %s\n", resolution_str);

    ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
    if (ret != 0)
        pr_err("pagespeak_cam: v4l2-ctl failed: %d\n", ret);

    return ret;
}

/*
 * ============================================================
 * File operations
 * ============================================================
 */

static int pagespeak_cam_open(struct inode *inode, struct file *filp)
{
    int ret;

    mutex_lock(&pcam_dev->lock);

    if (pcam_dev->is_open) {
        pr_err("pagespeak_cam: device already open, returning -EBUSY\n");
        mutex_unlock(&pcam_dev->lock);
        return -EBUSY;
    }

    /* Configure camera format via userspace helper */
    ret = configure_camera_format(pcam_dev->width, pcam_dev->height,
                                  pcam_dev->pixelformat);
    if (ret < 0) {
        pr_err("pagespeak_cam: camera config failed: %d\n", ret);
        mutex_unlock(&pcam_dev->lock);
        return ret;
    }

    /* Open the underlying V4L2 video device in read mode */
    pcam_dev->video_filp = filp_open(VIDEO_DEV_PATH, O_RDONLY, 0);
    if (IS_ERR(pcam_dev->video_filp)) {
        ret = PTR_ERR(pcam_dev->video_filp);
        pcam_dev->video_filp = NULL;
        pr_err("pagespeak_cam: failed to open %s: %d\n", VIDEO_DEV_PATH, ret);
        mutex_unlock(&pcam_dev->lock);
        return ret;
    }

    pcam_dev->is_open = true;
    filp->private_data = pcam_dev;

    pr_info("pagespeak_cam: device opened, capture %ux%u\n",
            pcam_dev->width, pcam_dev->height);

    mutex_unlock(&pcam_dev->lock);
    return 0;
}

static int pagespeak_cam_release(struct inode *inode, struct file *filp)
{
    mutex_lock(&pcam_dev->lock);

    if (pcam_dev->video_filp) {
        filp_close(pcam_dev->video_filp, NULL);
        pcam_dev->video_filp = NULL;
    }

    pcam_dev->is_open = false;

    pr_info("pagespeak_cam: device closed\n");

    mutex_unlock(&pcam_dev->lock);
    return 0;
}

static ssize_t pagespeak_cam_read(struct file *filp, char __user *buf,
                                   size_t count, loff_t *f_pos)
{
    ssize_t nread;
    loff_t pos = 0;

    mutex_lock(&pcam_dev->lock);

    if (!pcam_dev->video_filp) {
        pr_err("pagespeak_cam: read without valid video device\n");
        mutex_unlock(&pcam_dev->lock);
        return -EIO;
    }

    /*
     * Use kernel_read() to capture a frame. The UVC driver's .read
     * handler (uvc_v4l2_read) internally:
     *   1. Allocates a buffer if needed
     *   2. Starts streaming
     *   3. Dequeues one frame
     *   4. Copies frame data to the provided buffer
     *
     * kernel_read() is safe on ARM 5.15 — it calls the file's
     * .read or .read_iter callback without needing set_fs().
     */
    if (count > MAX_FRAME_SIZE)
        count = MAX_FRAME_SIZE;

    nread = kernel_read(pcam_dev->video_filp, pcam_dev->frame_buf,
                        count, &pos);
    if (nread < 0) {
        pr_err("pagespeak_cam: kernel_read failed: %zd\n", nread);
        mutex_unlock(&pcam_dev->lock);
        return nread;
    }

    if (nread == 0) {
        pr_err("pagespeak_cam: kernel_read returned 0 bytes\n");
        mutex_unlock(&pcam_dev->lock);
        return -EIO;
    }

    /* Deliver frame to userspace */
    if (copy_to_user(buf, pcam_dev->frame_buf, nread)) {
        pr_err("pagespeak_cam: copy_to_user failed\n");
        mutex_unlock(&pcam_dev->lock);
        return -EFAULT;
    }

    pr_info("pagespeak_cam: delivered %zd bytes to userspace\n", nread);

    mutex_unlock(&pcam_dev->lock);
    return nread;
}

static long pagespeak_cam_ioctl(struct file *filp, unsigned int cmd,
                                 unsigned long arg)
{
    int ret = 0;

    mutex_lock(&pcam_dev->lock);

    switch (cmd) {
    case PAGESPEAK_CAM_SET_RESOLUTION: {
        struct pagespeak_cam_resolution res;

        if (copy_from_user(&res, (void __user *)arg, sizeof(res))) {
            ret = -EFAULT;
            break;
        }

        if (res.width == 0 || res.height == 0 ||
            res.width > 1920 || res.height > 1080) {
            pr_err("pagespeak_cam: invalid resolution %ux%u\n",
                   res.width, res.height);
            ret = -EINVAL;
            break;
        }

        pcam_dev->width = res.width;
        pcam_dev->height = res.height;

        pr_info("pagespeak_cam: resolution set to %ux%u\n",
                pcam_dev->width, pcam_dev->height);
        break;
    }

    case PAGESPEAK_CAM_SET_PIXFMT: {
        struct pagespeak_cam_pixfmt pf;

        if (copy_from_user(&pf, (void __user *)arg, sizeof(pf))) {
            ret = -EFAULT;
            break;
        }

        pcam_dev->pixelformat = pf.pixelformat;

        pr_info("pagespeak_cam: pixelformat set to 0x%08x\n",
                pcam_dev->pixelformat);
        break;
    }

    case PAGESPEAK_CAM_QUERY_CAPS: {
        struct pagespeak_cam_caps caps;

        memset(&caps, 0, sizeof(caps));
        caps.max_width = 1920;
        caps.max_height = 1080;
        caps.min_width = 160;
        caps.min_height = 120;
        caps.current_width = pcam_dev->width;
        caps.current_height = pcam_dev->height;
        caps.current_pixelformat = pcam_dev->pixelformat;
        strscpy(caps.device_name, DEVICE_NAME, sizeof(caps.device_name));

        if (copy_to_user((void __user *)arg, &caps, sizeof(caps))) {
            ret = -EFAULT;
            break;
        }

        break;
    }

    default:
        pr_err("pagespeak_cam: unknown ioctl cmd 0x%x\n", cmd);
        ret = -ENOTTY;
        break;
    }

    mutex_unlock(&pcam_dev->lock);
    return ret;
}

static const struct file_operations pagespeak_cam_fops = {
    .owner          = THIS_MODULE,
    .open           = pagespeak_cam_open,
    .release        = pagespeak_cam_release,
    .read           = pagespeak_cam_read,
    .unlocked_ioctl = pagespeak_cam_ioctl,
};

/*
 * ============================================================
 * Module init / exit
 * ============================================================
 */

static int __init pagespeak_cam_init(void)
{
    int ret;

    pr_info("pagespeak_cam: initializing driver\n");

    /* Allocate device structure */
    pcam_dev = kzalloc(sizeof(*pcam_dev), GFP_KERNEL);
    if (!pcam_dev)
        return -ENOMEM;

    mutex_init(&pcam_dev->lock);
    pcam_dev->is_open = false;
    pcam_dev->width = DEFAULT_WIDTH;
    pcam_dev->height = DEFAULT_HEIGHT;
    pcam_dev->pixelformat = DEFAULT_PIXFMT;

    /* Allocate dynamic major number */
    ret = alloc_chrdev_region(&pcam_dev->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("pagespeak_cam: failed to allocate chrdev region: %d\n", ret);
        goto fail_alloc_region;
    }

    /* Initialize and add cdev */
    cdev_init(&pcam_dev->cdev, &pagespeak_cam_fops);
    pcam_dev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&pcam_dev->cdev, pcam_dev->dev_num, 1);
    if (ret < 0) {
        pr_err("pagespeak_cam: failed to add cdev: %d\n", ret);
        goto fail_cdev_add;
    }

    /* Create device class */
    pcam_dev->dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(pcam_dev->dev_class)) {
        ret = PTR_ERR(pcam_dev->dev_class);
        pr_err("pagespeak_cam: failed to create class: %d\n", ret);
        goto fail_class;
    }

    /* Create device node — this creates /dev/pagespeak-cam */
    pcam_dev->device = device_create(pcam_dev->dev_class, NULL,
                                     pcam_dev->dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(pcam_dev->device)) {
        ret = PTR_ERR(pcam_dev->device);
        pr_err("pagespeak_cam: failed to create device: %d\n", ret);
        goto fail_device;
    }

    /* Allocate frame buffer */
    pcam_dev->frame_buf = vmalloc(MAX_FRAME_SIZE);
    if (!pcam_dev->frame_buf) {
        ret = -ENOMEM;
        pr_err("pagespeak_cam: failed to allocate frame buffer\n");
        goto fail_framebuf;
    }

    pr_info("pagespeak_cam: driver initialized, major=%d minor=%d\n",
            MAJOR(pcam_dev->dev_num), MINOR(pcam_dev->dev_num));
    return 0;

fail_framebuf:
    device_destroy(pcam_dev->dev_class, pcam_dev->dev_num);
fail_device:
    class_destroy(pcam_dev->dev_class);
fail_class:
    cdev_del(&pcam_dev->cdev);
fail_cdev_add:
    unregister_chrdev_region(pcam_dev->dev_num, 1);
fail_alloc_region:
    kfree(pcam_dev);
    return ret;
}

static void __exit pagespeak_cam_exit(void)
{
    if (!pcam_dev)
        return;

    pr_info("pagespeak_cam: removing driver\n");

    vfree(pcam_dev->frame_buf);
    device_destroy(pcam_dev->dev_class, pcam_dev->dev_num);
    class_destroy(pcam_dev->dev_class);
    cdev_del(&pcam_dev->cdev);
    unregister_chrdev_region(pcam_dev->dev_num, 1);
    kfree(pcam_dev);

    pr_info("pagespeak_cam: driver removed\n");
}

module_init(pagespeak_cam_init);
module_exit(pagespeak_cam_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PageSpeak Team");
MODULE_DESCRIPTION("Character device for USB webcam frame capture");
MODULE_VERSION("0.1");
