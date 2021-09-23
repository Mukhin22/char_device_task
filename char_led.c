/**
 * @file   my_module.c
 * @author Anton Mukhin
 * @date   9/09/2021
 * @brief  A char device for controlling a GPIO LED.
 */
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define RED_LED_PIN     16
#define BLUE_LED_PIN    20
#define MAX_MESSAGE_LEN 32
#define MY_MAJOR        200
#define MY_MINOR        0
#define MY_DEV_COUNT    2
#define R_W_BUFF_LEN    1

#define GPIO_ANY_GPIO_DEVICE_DESC "LED_CTRL"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Mukhin");
MODULE_DESCRIPTION("A Char Device Driver module for LED controlling");

static int     my_open(struct inode *, struct file *);
static ssize_t my_read(struct file *, char *, size_t, loff_t *);
static ssize_t my_write(struct file *, const char *, size_t, loff_t *);
static int     my_close(struct inode *, struct file *);

static struct file_operations my_fops = {
    read: my_read,
    write: my_write,
    open: my_open,
    release: my_close,
    owner: THIS_MODULE
};

static char *      msg = NULL;
static struct cdev my_cdev;
static DEFINE_MUTEX(msg_lock);

int __init init_module(void)
{
    dev_t        devno;
    unsigned int count = MY_DEV_COUNT;
    int          err   = 0;

    devno = MKDEV(MY_MAJOR, MY_MINOR);
    register_chrdev_region(devno, count, GPIO_ANY_GPIO_DEVICE_DESC);

    msg = (char *)kmalloc(MAX_MESSAGE_LEN, GFP_KERNEL);
    if (msg == NULL) {
        pr_info("Allocation failed.\n");
        err = -ENOMEM;
        goto out;
    }

    if (!gpio_is_valid(RED_LED_PIN)) {
        pr_err("Invalid RED_LED_PIN\n");
        err = -ENODEV;
        goto out;
    }
    if (!gpio_is_valid(BLUE_LED_PIN)) {
        pr_err("Invalid RED_LED_PIN\n");
        err = -ENODEV;
        goto out;
    }

    gpio_request(RED_LED_PIN, "sysfs");
    gpio_direction_output(RED_LED_PIN, 0);

    gpio_request(BLUE_LED_PIN, "sysfs");
    gpio_direction_output(BLUE_LED_PIN, 0);

    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;

    err = cdev_add(&my_cdev, devno, count);
    if (err < 0) {
        pr_err("Device Add Error\n");
        goto out;
    }

    pr_info("This is my led control char driver\n");
    pr_info("'mknod /dev/LED_CTRL0 c %d 0'.\n", MY_MAJOR);
    pr_info("'mknod /dev/LED_CTRL1 c %d 1'.\n", MY_MAJOR);
out:
    if (msg && err) {
        kfree(msg);
    }
    return err;
}

void __exit cleanup_module(void)
{
    dev_t devno;
    pr_info("Deinit char led driver\n");

    gpio_set_value(RED_LED_PIN, 0);
    gpio_set_value(BLUE_LED_PIN, 0);
    gpio_free(RED_LED_PIN);
    gpio_free(BLUE_LED_PIN);
    devno = MKDEV(MY_MAJOR, MY_MINOR);

    mutex_lock(&msg_lock);
    if (msg) {
        kfree(msg);
    }
    mutex_unlock(&msg_lock);

    unregister_chrdev_region(devno, MY_DEV_COUNT);
    cdev_del(&my_cdev);
}

/*
 * file operation: OPEN
 * */
static int my_open(struct inode *inod, struct file *fil)
{
    int major;
    int minor;

    major = imajor(inod);
    minor = iminor(inod);
    pr_info("\nSome body is opening me at major %d  minor %d\n", major, minor);
    return 0;
}

/*
 * file operation: READ
 * */
static ssize_t my_read(struct file *fil, char *buff, size_t len, loff_t *off)
{
    char  led_value;
    short count;
    int   major, minor;

    if (len >= MAX_MESSAGE_LEN || len < 0) {
        pr_err("Invalid len parameter\n");
        return -EBADRQC;
    }

    major = imajor(file_inode(fil));
    minor = iminor(file_inode(fil));

    switch (minor) {
    case 0:
        led_value = gpio_get_value(RED_LED_PIN);
        break;
    case 1:
        led_value = gpio_get_value(BLUE_LED_PIN);
        break;
    default:
        pr_err("invalid minor value\n");
        return -EBADRQC;
    }

    count = copy_to_user(buff, &led_value, R_W_BUFF_LEN);
    if (unlikely(count)) {
        pr_err("Copy to user in read failed\n");
        return -EAGAIN;
    }

    pr_info("GPIO%d=%d, GPIO%d=%d\n",
            RED_LED_PIN,
            gpio_get_value(RED_LED_PIN),
            BLUE_LED_PIN,
            gpio_get_value(BLUE_LED_PIN));

    return R_W_BUFF_LEN;
}

static ssize_t
my_write(struct file *fil, const char *buff, size_t len, loff_t *off)
{
    int   minor;
    short count;

    if (len >= MAX_MESSAGE_LEN || len < 0) {
        pr_err("Invalid len parameter\n");
        return -EBADRQC;
    }

    mutex_lock(&msg_lock);
    memset(msg, 0, MAX_MESSAGE_LEN);
    /* need to get the device minor number because we have two devices */
    minor = iminor(file_inode(fil));
    /* copy the string from the user space program which open and write this
   * device */
    count = copy_from_user(msg, buff, R_W_BUFF_LEN);
    if (unlikely(count)) {
        pr_err("copy_from_user failed, not copied bytes len is %d", count);
        return -EAGAIN;
    }
    if (msg[0] == 1) {
        if (minor == 0) {
            gpio_set_value(RED_LED_PIN, 1); // RED_LED_PIN 0 ON
        }
        if (minor == 1) {
            gpio_set_value(BLUE_LED_PIN, 1); // BLUE_LED_PIN 1 ON
        }
    } else if (msg[0] == 0) {
        if (minor == 0) {
            gpio_set_value(RED_LED_PIN, 0); // RED_LED_PIN 0 OFF
        }
        if (minor == 1) {
            gpio_set_value(BLUE_LED_PIN, 0); // BLUE_LED_PIN 1 OFF
        }
    } else {
        pr_err("Unknown command , 1 or 0 \n");
    }
    mutex_unlock(&msg_lock);

    return R_W_BUFF_LEN;
}

static int my_close(struct inode *inod, struct file *fil)
{
    int minor;
    minor = iminor(file_inode(fil));
    pr_info("Some body is closing me at minor %d\n", minor);

    return 0;
}
