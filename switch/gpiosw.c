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
#include "gpiosw.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jinhyeok Jeon");
MODULE_DESCRIPTION("Raspberry Pi GPIO 17 Device Module");

/* 커널 버전 6.6 이상부터 커널 모듈에서 사용하는 GPIO
핀 번호는 /sys/kernel/debug/gpio 에 매핑되어 있는 번호로 사용해야 함 */
#define GPIO_SW      529 
#define GPIO_SW_STR  "17"

#define CLASS_NAME "gpio_class"
#define DEVICE_NAME "gpiosw"
#define NODE_NAME "gpiosw"

static int device_major; /* 디바이스 메이저 번호 */
static int device_minor = 0; /* 디바이스 마이너 번호 */
static struct class* gpio_class;
static struct device* gpio_device;

static char msg[BLOCK_SIZE] = { 0 };

static ssize_t gpio_read(struct file*, char*, size_t, loff_t*);
static long gpio_ioctl(struct file*, unsigned int, unsigned long);
static struct file_operations gpio_fops = {
  .owner = THIS_MODULE,
  .read = gpio_read,
  .unlocked_ioctl = gpio_ioctl,
};

static struct pid* user_pid = NULL;

static int switch_irq;
static irqreturn_t isr_func(int irq, void* data) {
  static unsigned long last_jiffies = 0;
  unsigned long now = jiffies;

  // 1000ms 안에 들어온 인터럽트는 무시
  if (time_before(now, last_jiffies + msecs_to_jiffies(1000))) {
    return IRQ_HANDLED;
  }

  last_jiffies = now;

  if (irq == switch_irq && user_pid) {
    int ret = kill_pid(user_pid, SIGUSR1, 1);
    if (ret < 0) {
      pr_info("gpiosw:failed to send SIGUSR1 (%d)\n", ret);
    }
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
  if (gpio_request(GPIO_SW, "SWITCH") < 0) {
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

  gpio_free(GPIO_SW);

  printk(KERN_INFO "Exit module - %s\n", DEVICE_NAME);
}

static ssize_t gpio_read(struct file* fp, char* buff, size_t len, loff_t* off) {
  if (*off == 0) {
    sprintf(msg, "GPIO: %s\n", (gpio_get_value(GPIO_SW) ? "HIGH" : "LOW"));
  }

  if (BLOCK_SIZE - *off < len) {
    len = BLOCK_SIZE - *off;
  }
  *off += len;

  return len - copy_to_user(buff, msg, len);
}

static long gpio_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {

  switch (cmd) {
  case GPIO_IOCTL_REGISTER_PID:
    /* 기존 pid 참조 해제 */
    if (user_pid) {
      put_pid(user_pid);
      user_pid = NULL;
    }

    /* 새로운 pid 등록 */
    user_pid = find_get_pid((pid_t)arg);
    if (!user_pid)
      return -ESRCH;

    pr_info("gpiosw: registered pid %d\n", (pid_t)arg);
    break;

  default:
    return -EINVAL;
  }

  return 0;
}

/*
pid_t 숫자만 저장 → 프로세스 죽고 재사용되면, 엉뚱한 프로세스에 시그널 보낼 수 있음
struct task_struct * 저장 → 프로세스 종료되면 free돼서 use-after-free 위험
struct pid * 저장 → 커널이 refcount로 관리 → 안전
*/