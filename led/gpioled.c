#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include "gpioled.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jinhyeok Jeon");
MODULE_DESCRIPTION("Raspberry Pi GPIO 17 LED Device Driver Module");

#define GPIO_LED_NUM  529 
#define GPIO_LED_STR  "17"

#define CLASS_NAME "gpioled_class"
#define DEVICE_NAME "gpioled"
#define NODE_NAME "gpioled"

static int device_major;
static int device_minor = 0;
static struct class* gpio_class;
static struct device* gpio_device;
static char* gpio_devnode(const struct device* dev, umode_t* mode) {
  if (mode) {
    *mode = 0666; // rw-rw-rw-
  }
  return NULL;
}

static char msg[BLOCK_SIZE] = { 0 };
static int led_mode = 0; // 0 = normal, 1 = blink
static int blink_period = 1000;
static struct timer_list blink_timer;
static void blink_timer_func(struct timer_list* t) {
  if (led_mode == 1) {
    bool led_on = gpio_get_value(GPIO_LED_NUM);
    gpio_set_value(GPIO_LED_NUM, !led_on);
    mod_timer(&blink_timer, jiffies + msecs_to_jiffies(blink_period));
  }
}

static int gpio_open(struct inode*, struct file*);
static int gpio_close(struct inode*, struct file*);
static ssize_t gpio_read(struct file*, char*, size_t, loff_t*);
static ssize_t gpio_write(struct file*, const char*, size_t, loff_t*);
static long gpio_ioctl(struct file* file, unsigned int cmd, unsigned long arg);
static struct file_operations gpio_fops = {
  .owner = THIS_MODULE,
  .read = gpio_read,
  .write = gpio_write,
  .open = gpio_open,
  .release = gpio_close,
  .unlocked_ioctl = gpio_ioctl,
};

static int __init gpio_led_init(void) {
  int ret;

  ret = gpio_request(GPIO_LED_NUM, "LED");
  if (ret < 0) {
    printk(KERN_ERR "gpio_request error: %d\n", ret);
    return ret;
  }

  gpio_direction_output(GPIO_LED_NUM, 0);

  device_major = register_chrdev(0, DEVICE_NAME, &gpio_fops);
  if (device_major < 0) {
    printk(KERN_ERR "register_chrdev failed: %d\n", device_major);
    ret = device_major;
    goto err_free_gpio;
  }

  gpio_class = class_create(CLASS_NAME);
  if (IS_ERR(gpio_class)) {
    printk(KERN_ERR "class_create failed\n");
    ret = PTR_ERR(gpio_class);
    goto err_unregister_chrdev;
  }
  gpio_class->devnode = gpio_devnode;

  gpio_device = device_create(gpio_class, NULL, MKDEV(device_major, device_minor), NULL, NODE_NAME);
  if (IS_ERR(gpio_device)) {
    printk(KERN_ERR "device_create failed\n");
    ret = PTR_ERR(gpio_device);
    goto err_destroy_class;
  }

  timer_setup(&blink_timer, blink_timer_func, 0);

  printk(KERN_INFO "Init module - %s\n", DEVICE_NAME);
  return 0;

err_destroy_class:
  class_destroy(gpio_class);
err_unregister_chrdev:
  unregister_chrdev(device_major, DEVICE_NAME);
err_free_gpio:
  gpio_free(GPIO_LED_NUM);

  return ret;
}
static void __exit gpio_led_exit(void) {
  device_destroy(gpio_class, MKDEV(device_major, device_minor));
  class_destroy(gpio_class);
  unregister_chrdev(device_major, DEVICE_NAME);

  gpio_set_value(GPIO_LED_NUM, 0);
  gpio_free(GPIO_LED_NUM);

  del_timer_sync(&blink_timer);

  printk(KERN_INFO "Exit module - %s\n", DEVICE_NAME);
}

module_init(gpio_led_init);
module_exit(gpio_led_exit);

static int gpio_open(struct inode* inode, struct file* fp) {
  blink_period = 1000;
  return 0;
}

static int gpio_close(struct inode* inode, struct file* fp) {
  return 0;
}

/*
<cat file>
fd = open("file", O_RDONLY);
while((n = read(fd, buf, BUFSIZ)) > 0) {
  write(STDOUT_FILENO, buf, n);
}
*/
static ssize_t gpio_read(struct file* fp, char* buff, size_t len, loff_t* off) {
  int msg_len;
  int remaining;
  int ret;

  if (*off == 0) {
    msg_len = snprintf(msg, sizeof(msg), "GPIO%s: %s\n", GPIO_LED_STR, gpio_get_value(GPIO_LED_NUM) ? "HIGH" : "LOW");
  }
  else {
    msg_len = strlen(msg);
  }

  if (*off >= msg_len) {
    return 0;
  }

  remaining = msg_len - *off;
  if (len > remaining) {
    len = remaining;
  }

  ret = copy_to_user(buff, msg + *off, len);
  if (ret != 0) {
    return -EFAULT;
  }

  *off += len;
  return len;
}

/*
<echo "1" > /dev/gpioled>
write(fd, "1\n", 2);
*/
static ssize_t gpio_write(struct file* fp, const char* buff, size_t len, loff_t* off) {
  int ret;

  if (len >= BLOCK_SIZE) {
    return -EINVAL;
  }

  ret = copy_from_user(msg, buff, len);
  if (ret != 0) {
    return -EFAULT;
  }

  msg[len] = '\0';

  if (!strcmp(msg, "0") || !strcmp(msg, "0\n")) {
    if (led_mode == 0) {
      gpio_set_value(GPIO_LED_NUM, 0);
    }
    else if (led_mode == 1) {
      del_timer_sync(&blink_timer);
    }
  }
  else if (!strcmp(msg, "1") || !strcmp(msg, "1\n")) {
    if (led_mode == 0) {
      gpio_set_value(GPIO_LED_NUM, 1);
    }
    else if (led_mode == 1) {
      if (!timer_pending(&blink_timer)) {
        mod_timer(&blink_timer, jiffies);
      }
    }
  }
  else if (!strcmp(msg, "NORMAL") || !strcmp(msg, "NORMAL\n")) {
    led_mode = 0;
    del_timer_sync(&blink_timer);
    printk(KERN_INFO "LED MODE: NORMAL\n");
  }
  else if (!strcmp(msg, "BLINK") || !strcmp(msg, "BLINK\n")) {
    led_mode = 1;
    printk(KERN_INFO "LED MODE: BLINK\n");
  }
  else {
    return -EINVAL;
  }

  return len;
}

static long gpio_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {
  int tmp;

  switch (cmd) {
  case LED_MODE_NORMAL:
    led_mode = 0;
    del_timer_sync(&blink_timer);
    printk(KERN_INFO "LED MODE: NORMAL\n");
    break;
  case LED_MODE_BLINK:
    led_mode = 1;
    printk(KERN_INFO "LED MODE: BLINK\n");
    break;
  case LED_SET_PERIOD:
    if (copy_from_user(&tmp, (int __user*)arg, sizeof(int))) {
      return -EFAULT;
    }
    if (tmp <= 0) {
      return -EINVAL;
    }

    blink_period = tmp;
    printk(KERN_INFO "Led blink period set to %dms\n", blink_period);
    break;
  default:
    return -EINVAL;
  }
  return 0;
}
