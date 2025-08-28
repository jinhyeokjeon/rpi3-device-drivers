#ifndef __GPIOLED_H_
#define __GPIOLED_H_

#include <linux/ioctl.h>

#define GPIO_IOC_MAGIC 'l'

#define LED_MODE_NORMAL  _IO(GPIO_IOC_MAGIC, 0)
#define LED_MODE_BLINK   _IO(GPIO_IOC_MAGIC, 1)
#define LED_SET_PERIOD   _IOW(GPIO_IOC_MAGIC, 2, int)

#endif