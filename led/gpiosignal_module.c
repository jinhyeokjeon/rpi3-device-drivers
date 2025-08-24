#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jinhyeok Jeon");
MODULE_DESCRIPTION("Raspberry Pi GPIO 17 Device Module");

/* 커널 버전 6.6 이상부터 커널 모듈에서 사용하는 GPIO
핀 번호는 /sys/kernel/debug/gpio 에 매핑되어 있는 번호로 사용해야 함 */
#define GPIO_LED      529 
#define GPIO_LED_STR  "17"
#define GPIO_SW       533
#define GPIO_SW_STR   "21"

#define CLASS_NAME "gpio_class"
#define DEVICE_NAME "gpioled"
#define NODE_NAME "gpioled"

static int device_major; /* 디바이스 메이저 번호 */
static int device_minor = 0; /* 디바이스 마이너 번호 */
static struct class* gpio_class;
static struct device* gpio_device;

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

static struct task_struct* task;

static int switch_irq;
static irqreturn_t isr_func(int irq, void* data) {
  static unsigned long last_jiffies = 0;
  unsigned long now = jiffies;

  // 200ms 안에 들어온 인터럽트는 무시
  if (time_before(now, last_jiffies + msecs_to_jiffies(200)))
    return IRQ_HANDLED;

  last_jiffies = now;

  if (irq == switch_irq && !gpio_get_value(GPIO_LED)) {
    gpio_set_value(GPIO_LED, 1);
  }
  else if (irq == switch_irq && gpio_get_value(GPIO_LED)) {
    gpio_set_value(GPIO_LED, 0);
  }

  if (irq == switch_irq) {
    kill_pid(task_pid(task), SIGIO, 1);
  }

  return IRQ_HANDLED;
}

static char* gpio_devnode(const struct device* dev, umode_t* mode) {
  if (mode) {
    *mode = 0666; // rw-rw-rw-
  }
  return NULL;
}

int init_module(void) {
  if (gpio_request(GPIO_LED, "LED") < 0) {
    printk(KERN_INFO "gpio_request error\n");
    return -1;
  }
  gpio_direction_output(GPIO_LED, 0);

  if (gpio_request(GPIO_SW, "SWITCH") < 0) {
    gpio_free(GPIO_LED);
    printk(KERN_INFO "gpio_request error\n");
    return -1;
  }
  gpio_direction_input(GPIO_SW);

  switch_irq = gpio_to_irq(GPIO_SW); // GPIO 인터럽트 번호 획득
  if (request_irq(switch_irq, isr_func, IRQF_TRIGGER_FALLING, "switch", NULL) < 0) { // GPIO 인터럽트 핸들러 등록
    printk(KERN_INFO "gpio_to_irq error\n");
    return -1;
  }

  device_major = register_chrdev(0, DEVICE_NAME, &gpio_fops);
  gpio_class = class_create(CLASS_NAME);
  gpio_class->devnode = gpio_devnode;
  gpio_device = device_create(gpio_class, NULL, MKDEV(device_major, device_minor), NULL, NODE_NAME);

  printk(KERN_INFO "Init module - %s\n", DEVICE_NAME);

  return 0;
}

void cleanup_module(void) {
  device_destroy(gpio_class, MKDEV(device_major, device_minor));
  class_destroy(gpio_class);
  unregister_chrdev(device_major, DEVICE_NAME);

  free_irq(switch_irq, NULL);

  gpio_set_value(GPIO_LED, 0);
  gpio_free(GPIO_LED);
  gpio_free(GPIO_SW);

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
    strcpy(msg, "GPIO" GPIO_LED_STR ": ");
    if (gpio_get_value(GPIO_LED)) {
      strcat(msg, "HIGH\n");
    }
    else {
      strcat(msg, "LOW\n");
    }
    strcat(msg, "GPIO" GPIO_SW_STR ": ");
    if (gpio_get_value(GPIO_SW)) {
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

  if (!strcmp(msg, "0") || !strcmp(msg, "0\n")) {
    gpio_set_value(GPIO_LED, 0);
  }
  else if (!strcmp(msg, "1") || !strcmp(msg, "1\n")) {
    gpio_set_value(GPIO_LED, 1);
  }
  else {
    task = pid_task(find_vpid(simple_strtol(msg, NULL, 10)), PIDTYPE_PID);
    if (task == NULL) {
      printk("Error: Can't find PID from user application\n");
      return 0;
    }
  }

  return count;
}
