#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jinhyeok Jeon");
MODULE_DESCRIPTION("Raspberry Pi GPIO 17 Device Module");

#define BCM_IO_BASE   0x3F000000 /* Raspberry Pi 2/3의 I/O Peripherals 주소 */
#define GPIO_BASE     (BCM_IO_BASE + 0x200000) /* GPIO 컨트롤러의 주소 */
#define GPIO_SIZE     256
#define GPIO_NUM      17
#define GPIO_NUM_STR  "17"

/* GPIO 설정 매크로 */
#define GPIO_IN(g) (*(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))) /* 입력 설정 */
#define GPIO_OUT(g) (*(gpio+((g)/10)) |= (1<<(((g)%10)*3))) /* 출력 설정 */

#define GPIO_SET(g) (*(gpio+7) = 1<<g) /* 비트 설정 */
#define GPIO_CLR(g) (*(gpio+10) = 1<<g) /* 설정된 비트 해제 */
#define GPIO_GET(g) (*(gpio+13)&(1<<g)) /* 현재 GPIO의 비트에 대한 정보 획득 */

#define CLASS_NAME "gpio_class"
#define DEVICE_NAME "gpio17"
#define NODE_NAME "gpio17"

static int device_major; /* 디바이스 메이저 번호 */
static int device_minor = 0; /* 디바이스 마이너 번호 */
static struct class* gpio_class;
static struct device* gpio_device;

volatile unsigned* gpio; /* I/O 접근을 위한 volatile 변수 */
static char msg[BLOCK_SIZE] = { 0 };

static int gpio_open(struct inode*, struct file*);
static int gpio_close(struct inode*, struct file*);
static ssize_t gpio_read(struct file*, char*, size_t, loff_t*);
static ssize_t gpio_write(struct file*, const char*, size_t, loff_t*);
static struct file_operations gpio_fops = {
  .owner = THIS_MODULE,
  .read = gpio_read,
  .write = gpio_write,
  .open = gpio_open,
  .release = gpio_close,
};

static char* gpio_devnode(const struct device* dev, umode_t* mode) {
  if (mode) {
    *mode = 0666; // rw-rw-rw-
  }
  return NULL;
}

int init_module(void) {
  static void* map;

  device_major = register_chrdev(0, DEVICE_NAME, &gpio_fops);
  gpio_class = class_create(CLASS_NAME);
  gpio_class->devnode = gpio_devnode;
  gpio_device = device_create(gpio_class, NULL, MKDEV(device_major, device_minor), NULL, NODE_NAME);

  map = ioremap(GPIO_BASE, GPIO_SIZE);
  if (!map) {
    printk("Error: mapping GPIO memmory\n");
    return -EBUSY;
  }
  gpio = (volatile unsigned int*)map;

  GPIO_OUT(GPIO_NUM);

  printk(KERN_INFO "Init module - %s\n", DEVICE_NAME);

  return 0;
}

void cleanup_module(void) {
  device_destroy(gpio_class, MKDEV(device_major, device_minor));
  class_destroy(gpio_class);
  unregister_chrdev(device_major, DEVICE_NAME);

  if (gpio) {
    iounmap(gpio);
  }

  printk(KERN_INFO "Exit module - %s\n", DEVICE_NAME);
}

static int gpio_open(struct inode* inode, struct file* fp) {
  try_module_get(THIS_MODULE);
  return 0;
}

static int gpio_close(struct inode* inode, struct file* fp) {
  module_put(THIS_MODULE);
  return 0;
}

static ssize_t gpio_read(struct file* fp, char* buff, size_t len, loff_t* off) {
  if (*off == 0) {
    memset(msg, 0, sizeof(msg));
    strcpy(msg, "GPIO" GPIO_NUM_STR ": ");
    if (GPIO_GET(GPIO_NUM)) {
      strcat(msg, "HIGH\n");
    }
    else {
      strcat(msg, "LOW\n");
    }
  }

  if (BLOCK_SIZE - *off < len) {
    len = BLOCK_SIZE - *off;
  }
  *off += len;

  return len - copy_to_user(buff, msg, len);
}

static ssize_t gpio_write(struct file* inode, const char* buff, size_t len, loff_t* off) {
  short count;
  memset(msg, 0, BLOCK_SIZE);
  count = len - copy_from_user(msg, buff, len);

  if (msg[0] == '0') {
    GPIO_CLR(GPIO_NUM);
  }
  else if (msg[0] == '1') {
    GPIO_SET(GPIO_NUM);
  }
  else {
    return -EINVAL;
  }

  return count;
}
