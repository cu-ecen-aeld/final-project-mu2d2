// SPDX-License-Identifier: GPL-2.0-only
/*
 * pagespeak_cam.c — Access control and configuration driver for USB webcam
 *
 * Creates /dev/pagespeak-cam and provides:
 *   - Exclusive access enforcement (EBUSY on concurrent open)
 *   - Persistent V4L2 config storage (resolution, pixel format)
 *   - ioctl interface to query/set capture parameters
 *
 * Frame capture is handled entirely in userspace via the underlying
 * V4L2 device (/dev/pagespeak-cam-raw) using standard mmap streaming.
 * The raw device path is returned by PAGESPEAK_CAM_QUERY_CAPS so the
 * application knows which V4L2 node to open for capture.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "pagespeak_cam.h"

#define DEVICE_NAME     "pagespeak-cam"
#define CLASS_NAME      "pagespeak-cam"
#define VIDEO_DEV_PATH  "/dev/pagespeak-cam-raw"

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

    /* Persistent V4L2 configuration */
    uint32_t width;
    uint32_t height;
    uint32_t pixelformat;
};

static struct pagespeak_cam_dev *pcam_dev;

/*
 * ============================================================
 * File operations
 * ============================================================
 */

static int pagespeak_cam_open(struct inode *inode, struct file *filp)
{
    mutex_lock(&pcam_dev->lock);

    if (pcam_dev->is_open) {
        pr_err("pagespeak_cam: device already open, returning -EBUSY\n");
        mutex_unlock(&pcam_dev->lock);
        return -EBUSY;
    }

    pcam_dev->is_open = true;
    filp->private_data = pcam_dev;

    pr_info("pagespeak_cam: device opened\n");

    mutex_unlock(&pcam_dev->lock);
    return 0;
}

static int pagespeak_cam_release(struct inode *inode, struct file *filp)
{
    mutex_lock(&pcam_dev->lock);

    pcam_dev->is_open = false;

    pr_info("pagespeak_cam: device closed\n");

    mutex_unlock(&pcam_dev->lock);
    return 0;
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
        strscpy(caps.raw_device_path, VIDEO_DEV_PATH,
                sizeof(caps.raw_device_path));

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

    pcam_dev = kzalloc(sizeof(*pcam_dev), GFP_KERNEL);
    if (!pcam_dev)
        return -ENOMEM;

    mutex_init(&pcam_dev->lock);
    pcam_dev->is_open = false;
    pcam_dev->width = DEFAULT_WIDTH;
    pcam_dev->height = DEFAULT_HEIGHT;
    pcam_dev->pixelformat = DEFAULT_PIXFMT;

    ret = alloc_chrdev_region(&pcam_dev->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("pagespeak_cam: failed to allocate chrdev region: %d\n", ret);
        goto fail_alloc_region;
    }

    cdev_init(&pcam_dev->cdev, &pagespeak_cam_fops);
    pcam_dev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&pcam_dev->cdev, pcam_dev->dev_num, 1);
    if (ret < 0) {
        pr_err("pagespeak_cam: failed to add cdev: %d\n", ret);
        goto fail_cdev_add;
    }

    pcam_dev->dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(pcam_dev->dev_class)) {
        ret = PTR_ERR(pcam_dev->dev_class);
        pr_err("pagespeak_cam: failed to create class: %d\n", ret);
        goto fail_class;
    }

    pcam_dev->device = device_create(pcam_dev->dev_class, NULL,
                                     pcam_dev->dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(pcam_dev->device)) {
        ret = PTR_ERR(pcam_dev->device);
        pr_err("pagespeak_cam: failed to create device: %d\n", ret);
        goto fail_device;
    }

    pr_info("pagespeak_cam: driver initialized, major=%d minor=%d\n",
            MAJOR(pcam_dev->dev_num), MINOR(pcam_dev->dev_num));
    return 0;

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
MODULE_DESCRIPTION("Character device for USB webcam access control and configuration");
MODULE_VERSION("0.2");

