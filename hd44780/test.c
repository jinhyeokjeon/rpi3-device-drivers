#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include "hd44780.h"

int main(void) {
  int fd;
  char buf[100];

  fd = open("/dev/hd44780", O_WRONLY);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  puts("DISPLAY ON");
  if (ioctl(fd, LCD_IOCTL_DISPLAY_ON) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close(fd);
    exit(1);
  }

  puts("BACKLIGHT ON");
  if (ioctl(fd, LCD_IOCTL_BACKLIGHT_ON) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close(fd);
    exit(1);
  }

  puts("WRITE ON LCD");
  strcpy(buf, "Hello World!\nThis is hd44780");
  if (write(fd, buf, strlen(buf)) < 0) {
    perror("write ON");
  }

  puts("sleep for 3 seconds . . .");
  sleep(3);

  puts("CLEAR");
  if (ioctl(fd, LCD_IOCTL_CLEAR) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close(fd);
    exit(1);
  }

  puts("sleep for 3 seconds . . .");
  sleep(3);

  puts("WRITE ON LCD(2)");
  strcpy(buf, "Good bye!");
  if (write(fd, buf, strlen(buf)) < 0) {
    perror("write ON");
  }

  puts("sleep for 3 seconds . . .");
  sleep(3);

  puts("CLEAR");
  if (ioctl(fd, LCD_IOCTL_CLEAR) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close(fd);
    exit(1);
  }

  puts("DISPLAY OFF");
  if (ioctl(fd, LCD_IOCTL_DISPLAY_OFF) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close(fd);
    exit(1);
  }

  puts("BACKLIGHT OFF");
  if (ioctl(fd, LCD_IOCTL_BACKLIGHT_OFF) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close(fd);
    exit(1);
  }

  return 0;
}