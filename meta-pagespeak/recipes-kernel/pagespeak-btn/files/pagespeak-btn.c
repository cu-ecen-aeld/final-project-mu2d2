// SPDX-License-Identifier: GPL-2.0
/**
 * @file   pagespeak-btn.c
 * @brief  Out-of-tree kernel module for the PageSpeak GPIO capture button.
 *         Registers a falling-edge IRQ on a configurable GPIO pin (default 17,
 *         physical pin 11 on the 40-pin header). Applies a software debounce
 *         window (default 50 ms) in the IRQ handler to suppress contact bounce.
 *         Each validated press is logged via printk and queued for userspace
 *         consumption through the /dev/pagespeak-btn character device.
 *
 *         Wiring: connect one leg of the button to GPIO 17 (pin 11) and the
 *         other leg to GND (pin 9). Use an external 10k pull-up to 3.3 V.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>

// Driver and device identifiers used in printk, cdev, and udev
#define DRIVER_NAME  "pagespeak-btn"
#define DEVICE_NAME  "pagespeak-btn"
#define CLASS_NAME   "pagespeak"

// Default GPIO pin, overridable at insmod time: insmod pagespeak-btn.ko gpio_pin=22
static int gpio_pin = 17;

// Default debounce window in milliseconds
static int debounce_ms = 50;

module_param(gpio_pin,     int, 0444);
module_param(debounce_ms,  int, 0444);

MODULE_PARM_DESC(gpio_pin,    "GPIO BCM pin number for the capture button (default 17)");
MODULE_PARM_DESC(debounce_ms, "Software debounce window in milliseconds (default 50)");

/**
 * @brief Event structure written to userspace via read().
 *        The kernel module and the test application must define this identically.
 */
struct btn_event {
    u32 count;         // monotonically increasing press count since module load
    s64 timestamp_ns;  // ktime_get() value at the moment the press was accepted
};

// Allocated character device major/minor pair
static dev_t dev_num;

// cdev, class, and device handles for /dev/pagespeak-btn
static struct cdev       btn_cdev;
static struct class     *btn_class  = NULL;
static struct device    *btn_device = NULL;

// IRQ number assigned by gpio_to_irq, stored for free_irq on exit
static int irq_number = -1;

// Wait queue used to unblock read() and poll() callers on new events
static DECLARE_WAIT_QUEUE_HEAD(btn_wait_queue);

// Spinlock protecting the shared event state below
static spinlock_t btn_lock;

// Latest accepted button event, protected by btn_lock
static struct btn_event latest_event;

// Flag set by IRQ handler, cleared by read(), protected by btn_lock
static bool event_pending = false;

// ktime of the last accepted press, used for debounce comparison
static ktime_t last_irq_time;

// Total validated press count, incremented under btn_lock
static u32 press_count = 0;

/**
 * @brief IRQ handler invoked on every falling edge of the GPIO pin.
 *        Performs the 50 ms software debounce check using ktime_get().
 *        Accepted presses update shared state and wake waiting readers.
 *
 * @param irq    Linux IRQ number (passed by the kernel, informational)
 * @param dev_id Cookie pointer supplied to request_irq (unused, NULL)
 *
 * @return IRQ_HANDLED always, indicating this handler owns the interrupt
 */
static irqreturn_t btn_irq_handler(int irq, void *dev_id)
{
    ktime_t now = ktime_get();
    unsigned long flags;

    // Compute elapsed milliseconds since the last accepted press
    s64 delta_ms = ktime_to_ms(ktime_sub(now, last_irq_time));

    // Discard if the gap is shorter than the debounce window
    if (delta_ms < (s64)debounce_ms)
        return IRQ_HANDLED;

    // Accept this press, update shared state under spinlock
    spin_lock_irqsave(&btn_lock, flags);

    press_count++;
    last_irq_time            = now;
    latest_event.count        = press_count;
    latest_event.timestamp_ns = ktime_to_ns(now);
    event_pending             = true;

    spin_unlock_irqrestore(&btn_lock, flags);

    // Log to kernel ring buffer, visible in dmesg and syslog
    printk(KERN_INFO DRIVER_NAME ": press #%u at %lld ns\n",
           press_count, ktime_to_ns(now));

    // Wake any task blocked in btn_read() or waiting in poll()
    wake_up_interruptible(&btn_wait_queue);

    return IRQ_HANDLED;
}

/**
 * @brief file_operations open handler for /dev/pagespeak-btn.
 *        No per-open state is needed; the module maintains a single event queue.
 *
 * @param inode Inode of the device node
 * @param file  Open file structure created by the kernel
 *
 * @return 0 always
 */
static int btn_open(struct inode *inode, struct file *file)
{
    return 0;
}

/**
 * @brief file_operations read handler for /dev/pagespeak-btn.
 *        Blocks the calling task until a button press event is available,
 *        then copies one btn_event struct to the userspace buffer.
 *        Clears the pending flag so the next read blocks until the next press.
 *
 * @param file  Open file handle
 * @param buf   Userspace destination buffer
 * @param count Number of bytes the caller requested
 * @param ppos  File position offset (not used for this device)
 *
 * @return sizeof(struct btn_event) on success,
 *         EINVAL if buf is too small,
 *         ERESTARTSYS if interrupted by a signal,
 *         EFAULT if the copy to userspace fails
 */
static ssize_t btn_read(struct file *file, char __user *buf,
                        size_t count, loff_t *ppos)
{
    struct btn_event snapshot;
    unsigned long flags;
    int ret;

    // Caller must supply at least enough space for one event
    if (count < sizeof(struct btn_event))
        return -EINVAL;

    // Block until the IRQ handler signals a new event
    ret = wait_event_interruptible(btn_wait_queue, event_pending);
    if (ret)
        return -ERESTARTSYS;

    // Snapshot the latest event and clear the pending flag under spinlock
    spin_lock_irqsave(&btn_lock, flags);
    snapshot      = latest_event;
    event_pending = false;
    spin_unlock_irqrestore(&btn_lock, flags);

    // Transfer the snapshot to the caller's buffer
    if (copy_to_user(buf, &snapshot, sizeof(snapshot)))
        return -EFAULT;

    return sizeof(snapshot);
}

/**
 * @brief file_operations release handler for /dev/pagespeak-btn.
 *        Nothing to clean up per open file; always succeeds.
 *
 * @param inode Inode of the device node
 * @param file  Open file structure being closed
 *
 * @return 0 always
 */
static int btn_release(struct inode *inode, struct file *file)
{
    return 0;
}

// File operations table wired to the cdev
static const struct file_operations btn_fops = {
    .owner   = THIS_MODULE,
    .open    = btn_open,
    .read    = btn_read,
    .release = btn_release,
};

/**
 * @brief Module init function. Requests GPIO and IRQ, allocates and registers
 *        the character device, and creates the /dev/pagespeak-btn node.
 *        Uses a labeled error-unwind goto chain to avoid resource leaks.
 *
 * @return 0 on success, negative errno on any failure
 */
static int __init pagespeak_btn_init(void)
{
    int ret;

    // Initialize the spinlock and set the debounce reference time to zero
    spin_lock_init(&btn_lock);
    last_irq_time = ktime_set(0, 0);

    // Request exclusive ownership of the GPIO pin as a digital input
    ret = gpio_request_one(gpio_pin, GPIOF_IN, DRIVER_NAME);
    if (ret)
    {
        printk(KERN_ERR DRIVER_NAME ": gpio_request_one failed for pin %d, err %d\n",
               gpio_pin, ret);
        return ret;
    }

    // Translate the GPIO number to a Linux IRQ number
    irq_number = gpio_to_irq(gpio_pin);
    if (irq_number < 0)
    {
        printk(KERN_ERR DRIVER_NAME ": gpio_to_irq failed, err %d\n", irq_number);
        ret = irq_number;
        goto err_gpio;
    }

    // Register the IRQ handler for falling edges (button press pulls pin low)
    ret = request_irq(irq_number, btn_irq_handler,
                      IRQF_TRIGGER_FALLING, DRIVER_NAME, NULL);
    if (ret)
    {
        printk(KERN_ERR DRIVER_NAME ": request_irq failed, err %d\n", ret);
        goto err_gpio;
    }

    // Dynamically allocate a major/minor device number pair
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret)
    {
        printk(KERN_ERR DRIVER_NAME ": alloc_chrdev_region failed, err %d\n", ret);
        goto err_irq;
    }

    // Initialize the cdev structure and link it to our file_operations
    cdev_init(&btn_cdev, &btn_fops);
    btn_cdev.owner = THIS_MODULE;

    ret = cdev_add(&btn_cdev, dev_num, 1);
    if (ret)
    {
        printk(KERN_ERR DRIVER_NAME ": cdev_add failed, err %d\n", ret);
        goto err_chrdev;
    }

    // Create the device class so udev can generate the /dev node
    btn_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(btn_class))
    {
        ret = PTR_ERR(btn_class);
        printk(KERN_ERR DRIVER_NAME ": class_create failed, err %d\n", ret);
        goto err_cdev;
    }

    // Create the /dev/pagespeak-btn device node
    btn_device = device_create(btn_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(btn_device))
    {
        ret = PTR_ERR(btn_device);
        printk(KERN_ERR DRIVER_NAME ": device_create failed, err %d\n", ret);
        goto err_class;
    }

    printk(KERN_INFO DRIVER_NAME ": registered on GPIO %d, IRQ %d, debounce %d ms\n",
           gpio_pin, irq_number, debounce_ms);

    return 0;

// Error unwind path, releases resources in reverse order of acquisition
err_class:
    class_destroy(btn_class);
err_cdev:
    cdev_del(&btn_cdev);
err_chrdev:
    unregister_chrdev_region(dev_num, 1);
err_irq:
    free_irq(irq_number, NULL);
err_gpio:
    gpio_free(gpio_pin);
    return ret;
}

/**
 * @brief Module exit function. Destroys the device node and class, removes
 *        the cdev, releases the IRQ, and frees the GPIO. Called on rmmod.
 */
static void __exit pagespeak_btn_exit(void)
{
    // Tear down in reverse order of init to avoid dangling references
    device_destroy(btn_class, dev_num);
    class_destroy(btn_class);
    cdev_del(&btn_cdev);
    unregister_chrdev_region(dev_num, 1);
    free_irq(irq_number, NULL);
    gpio_free(gpio_pin);

    printk(KERN_INFO DRIVER_NAME ": unloaded\n");
}

module_init(pagespeak_btn_init);
module_exit(pagespeak_btn_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Muthuu Svs");
MODULE_DESCRIPTION("PageSpeak GPIO capture button IRQ driver");
MODULE_VERSION("1.0");
