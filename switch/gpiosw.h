#ifndef __GPIOSW_H_
#define __GPIOSW_H_

#include <linux/ioctl.h>

#define GPIO_IOCTL_MAGIC 'G'
#define GPIO_IOCTL_REGISTER_PID _IO(GPIO_IOCTL_MAGIC, 0)

#endif