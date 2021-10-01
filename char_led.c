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
#include <linux/delay.h>
#include <linux/ctype.h>

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

typedef enum { TURN_OFF, TURN_ON, TURN_BLINK } commands;

struct led_blink_ops {
    uint16_t interval_ms;
    uint16_t blinks_num;
    commands cmd;
};

#define DEF_INTERVAL  500
#define DEF_BLINK_NUM 5

#define MAX_TIMES_TO_BLINK 100
#define MIN_TIMES_TO_BLINK 1

#define MAX_INTER_MS   2500
#define MIN_INTER_MS   100
#define MAX_BLINK_ARGS 3
#define RED_LED_MINOR  1
#define BLUE_LED_MINOR 0

#define DEF_BLINK_OPS                                             \
    {                                                             \
        .interval_ms = DEF_INTERVAL, .blinks_num = DEF_BLINK_NUM, \
        .cmd = TURN_OFF                                           \
    }

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
static struct led_blink_ops bl_ops = DEF_BLINK_OPS;

static void print_ops_buff(const char *buff, size_t len)
{
    size_t i;
    pr_info("Buff element len is %d \n", len);
    pr_info("Buff elements are : \n");
    for (i = 0; i < len; i++) {
        printk("%d", buff[i]);
    }
}

static int check_cmd(long param)
{
    if (param < 0 || param > 2) {
        pr_err("Wrong command used %ld\n", param);
        return -EINVAL;
    }
    switch (param) {
    case TURN_OFF:
        pr_info("cmd to TURN_OFF recognized\n");
        bl_ops.cmd = TURN_OFF;
        break;
    case TURN_ON:
        pr_info("cmd to TURN_ON recognized\n");
        bl_ops.cmd = TURN_ON;
        break;
    case TURN_BLINK:
        bl_ops.cmd = TURN_BLINK;
        break;
    default:
        pr_err("Unrecognized param \n");
        return -EINVAL;
    }
    return 0;
}
static int parse_cmd_buff(const char *buff, size_t len)
{
    int   err     = 0;
    int   arg_num = 0;
    long  param;
    char *str_buff;
    print_ops_buff(buff, len);
    str_buff = (char *)kmalloc((len + 1), GFP_KERNEL);
    strncpy(str_buff, buff, len);
    str_buff[len] = '\0';
    pr_info("string buffer is: %s \n", str_buff);
    while (*str_buff) {
        if (isdigit(*str_buff)) {
            arg_num++;
            param = simple_strtol(str_buff, &str_buff, 10);
            pr_info("Current parameter parsed value is %ld", param);
            switch (arg_num) {
            case 1:
                err = check_cmd(param);
                if (err) {
                    goto out;
                }
                if (bl_ops.cmd != TURN_BLINK) {
                    goto out;
                }
                break;
            case 2:
                if ((param > MAX_TIMES_TO_BLINK) ||
                    (param < MIN_TIMES_TO_BLINK)) {
                    pr_err("Wrong times to blink parameter used\n");
                    err = -EINVAL;
                    goto out;
                }
                bl_ops.blinks_num = param;
                pr_info("Blinks num parameter used is %ld", param);
                break;
            case 3:
                if ((param > MAX_INTER_MS) || (param < MIN_INTER_MS)) {
                    pr_err("Wrong blink inetral parameter used\n");
                    err = -EINVAL;
                    goto out;
                }
                bl_ops.interval_ms = param;
                pr_info("Interval ms parameter used is %ld", param);
                goto out;
            default:
                break;
            }
        } else {
            str_buff++;
        }
    }
out:
    if (err) {
        pr_err("Error during blink args parsing\n");
    }
    if (str_buff) {
        kfree(str_buff);
    }
    return err;
}

static void blink_leds(const int minor)
{
    int i = 0;
    if (RED_LED_MINOR == minor) {
        for (i = 0; i < bl_ops.blinks_num; i++) {
            pr_info("BLinking led now\n");
            gpio_set_value(RED_LED_PIN, 1);
            msleep(bl_ops.interval_ms);
            gpio_set_value(RED_LED_PIN, 0);
        }
        bl_ops.cmd = TURN_OFF;
    }
    if (BLUE_LED_MINOR == minor) {
        for (i = 0; i < bl_ops.blinks_num; i++) {
            gpio_set_value(BLUE_LED_PIN, 1);
            msleep(bl_ops.interval_ms);
            gpio_set_value(BLUE_LED_PIN, 0);
        }
        bl_ops.cmd = TURN_OFF;
    }
}

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
    int   err = 0;
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
    count = copy_from_user(msg, buff, len);
    if (unlikely(count)) {
        pr_err("copy_from_user failed, not copied bytes len is %d", count);
        return -EAGAIN;
    }
    print_ops_buff(buff, len);
    err = parse_cmd_buff(msg, len);
    if (err) {
        goto out;
    }

    if (bl_ops.cmd == TURN_ON) {
        if (minor == 0) {
            pr_info("enable red led \n");
            gpio_set_value(RED_LED_PIN, 1); // RED_LED_PIN 0 ON
        }
        if (minor == 1) {
            pr_info("enable blue led \n");
            gpio_set_value(BLUE_LED_PIN, 1); // BLUE_LED_PIN 1 ON
        }
    } else if (bl_ops.cmd == TURN_OFF) {
        if (minor == 0) {
            pr_info("disable red led \n");
            gpio_set_value(RED_LED_PIN, 0); // RED_LED_PIN 0 OFF
        }
        if (minor == 1) {
            pr_info("disable red led \n");
            gpio_set_value(BLUE_LED_PIN, 0); // BLUE_LED_PIN 1 OFF
        }
    } else if (bl_ops.cmd == TURN_BLINK) {
        blink_leds(minor);
    } else {
        pr_err("Unknown command , 1 or 0 \n");
    }

out:
    if (err) {
        pr_err("Write function failed\n");
        len = 0;
    }
    mutex_unlock(&msg_lock);
    return len;
}

static int my_close(struct inode *inod, struct file *fil)
{
    int minor;
    minor = iminor(file_inode(fil));
    pr_info("Some body is closing me at minor %d\n", minor);

    return 0;
}
