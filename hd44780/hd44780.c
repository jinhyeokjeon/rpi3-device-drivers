#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "hd44780.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jinhyeok Jeon");
MODULE_DESCRIPTION("I2C LCD Character Device Driver (write=print, ioctl=control)");

#define CLASS_NAME  "hd44780_class"
#define DEVICE_NAME "hd44780"
#define NODE_NAME   "hd44780"

static int device_major;
static int device_minor = 0;
static struct class* lcd_class;
static struct device* lcd_device;
static struct i2c_client* lcd_client;
static int backlight = 0;

/* devnode → /dev/i2c_lcd 자동 생성, 권한 0666 */
static char* lcd_devnode(const struct device* dev, umode_t* mode) {
  if (mode)
    *mode = 0666;
  return NULL;
}

/* === 저수준 LCD 제어 함수 (PCF8574 기반) === */
static void lcd_expander_write(uint8_t data) {
  uint8_t buf = data | (backlight ? 0x08 : 0x00);
  i2c_master_send(lcd_client, &buf, 1);
}

static void lcd_pulse_enable(uint8_t data) {
  lcd_expander_write(data | 0x04); // EN=1
  udelay(1);
  lcd_expander_write(data & ~0x04); // EN=0
  udelay(50);
}

static void lcd_write4bits(uint8_t value) {
  lcd_expander_write(value);
  lcd_pulse_enable(value);
}

static void lcd_send(uint8_t value, uint8_t mode) {
  uint8_t highnib = value & 0xF0;
  uint8_t lownib = (value << 4) & 0xF0;
  lcd_write4bits(highnib | mode);
  lcd_write4bits(lownib | mode);
}

static void lcd_command(uint8_t cmd) {
  lcd_send(cmd, 0);
  mdelay(2);
}

static void lcd_data(uint8_t data) {
  lcd_send(data, 0x01); // RS=1
  mdelay(2);
}

static void lcd_init_hw(void) {
  msleep(50);

  lcd_write4bits(0x30); msleep(5);
  lcd_write4bits(0x30); msleep(5);
  lcd_write4bits(0x30); msleep(5);

  lcd_write4bits(0x20); msleep(5);

  lcd_command(0x28); // Function Set: 4-bit, 2 line
  lcd_command(0x01); // Clear
  lcd_command(0x06); // Entry mode: increment
  lcd_command(0x02); // Home
}

/* === file_operations === */
static ssize_t lcd_write(struct file* file, const char __user* buf, size_t len, loff_t* off) {
  char kbuf[64];
  size_t to_copy = min(len, sizeof(kbuf) - 1);
  size_t i;

  if (copy_from_user(kbuf, buf, to_copy)) {
    return -EFAULT;
  }

  kbuf[to_copy] = '\0';

  for (i = 0; i < to_copy; i++) {
    if (kbuf[i] == '\n') {
      lcd_command(0xC0); // 2행 시작
    }
    else {
      lcd_data(kbuf[i]);
    }
  }

  return to_copy;
}

static long lcd_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {
  switch (cmd) {
  case LCD_IOCTL_BACKLIGHT_ON:
    backlight = 1;
    lcd_expander_write(0x00);
    break;

  case LCD_IOCTL_BACKLIGHT_OFF:
    backlight = 0;
    lcd_expander_write(0x00);
    break;

  case LCD_IOCTL_DISPLAY_ON:
    lcd_command(0x0C);
    break;

  case LCD_IOCTL_DISPLAY_OFF:
    lcd_command(0x08);
    break;

  case LCD_IOCTL_CLEAR:
    lcd_command(0x01);
    msleep(2);
    break;

  case LCD_IOCTL_HOME:
    lcd_command(0x02);
    msleep(2);
    break;

  case LCD_IOCTL_ENTRY_MODE: {
    int mode;
    if (copy_from_user(&mode, (int __user*)arg, sizeof(int)))
      return -EFAULT;
    if (mode >= LCD_ENTRY_LEFT && mode <= LCD_ENTRY_RIGHT_SHIFT)
      lcd_command(mode);
    else
      return -EINVAL;
    break;
  }

  default:
    return -ENOTTY;
  }
  return 0;
}

static struct file_operations lcd_fops = {
    .owner = THIS_MODULE,
    .write = lcd_write,
    .unlocked_ioctl = lcd_ioctl,
};

/* === I2C driver === */
static int lcd_probe(struct i2c_client* client) {
  int ret;

  lcd_client = client;

  /* char device 등록 */
  device_major = register_chrdev(0, DEVICE_NAME, &lcd_fops);
  if (device_major < 0) {
    pr_err("lcd: register_chrdev failed\n");
    return device_major;
  }

  lcd_class = class_create(CLASS_NAME);
  if (IS_ERR(lcd_class)) {
    ret = PTR_ERR(lcd_class);
    unregister_chrdev(device_major, DEVICE_NAME);
    return ret;
  }
  lcd_class->devnode = lcd_devnode;

  lcd_device = device_create(lcd_class, NULL, MKDEV(device_major, device_minor), NULL, NODE_NAME);
  if (IS_ERR(lcd_device)) {
    ret = PTR_ERR(lcd_device);
    class_destroy(lcd_class);
    unregister_chrdev(device_major, DEVICE_NAME);
    return ret;
  }

  lcd_init_hw();

  pr_info("lcd: probed at 0x%02x\n", client->addr);
  return 0;
}

static void lcd_remove(struct i2c_client* client) {
  device_destroy(lcd_class, MKDEV(device_major, device_minor));
  class_destroy(lcd_class);
  unregister_chrdev(device_major, DEVICE_NAME);

  backlight = 0;
  lcd_expander_write(0x00);
  lcd_command(0x01);
  msleep(2);
  lcd_command(0x08);

  pr_info("lcd: removed\n");
}

static const struct of_device_id lcd_of_match[] = {
    {.compatible = "hitachi,hd44780" },
    { }
};
MODULE_DEVICE_TABLE(of, lcd_of_match);

static const struct i2c_device_id lcd_id[] = {
    { "i2c_lcd", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, lcd_id);

static struct i2c_driver lcd_driver = {
    .driver = {
        .name = "i2c_lcd",
        .of_match_table = lcd_of_match,
    },
    .probe = lcd_probe,
    .remove = lcd_remove,
    .id_table = lcd_id,
};

/* init/exit 대신 매크로 하나로 */
module_i2c_driver(lcd_driver);