#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "gpioled.h"

int main(void) {
  int fd;
  int period;

  fd = open("/dev/gpioled", O_RDWR);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  // 1. NORMAL 모드 설정
  puts("SET LED MODE: NORMAL");
  if (ioctl(fd, LED_MODE_NORMAL) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close(fd);
    exit(1);
  }

  // 2. LED 켜기
  puts("LED ON (NORMAL)");
  if (write(fd, "1", 1) < 0) {
    perror("write ON");
  }
  sleep(5);

  // 3. LED 끄기
  puts("LED OFF (NORMAL)");
  if (write(fd, "0", 1) < 0) {
    perror("write OFF");
  }
  sleep(5);

  // 4. BLINK 모드 설정
  puts("SET LED MODE: BLINK");
  if (ioctl(fd, LED_MODE_BLINK) < 0) {
    perror("ioctl LED_MODE_BLINK");
  }

  // 5. BLINK 모드에서 led ON
  puts("LED ON for 5 sec (BLINK)");
  if (write(fd, "1", 1) < 0) {
    perror("write ON");
  }
  sleep(5);  // 5초 동안 깜빡임 관찰

  // 5. 주기를 100ms 로 변경
  puts("SET BLINK PERIOD: 100ms");
  period = 100;
  if (ioctl(fd, LED_SET_PERIOD, &period) < 0) {
    perror("ioctl LED_SET_PERIOD");
  }

  // 6. BLINK 모드에서 write "1" 하면 타이머 시작됨
  puts("LED ON for 5 sec (BLINK)");
  if (write(fd, "1", 1) < 0) {
    perror("write BLINK START");
  }
  sleep(5);  // 5초간 빠르게 깜빡임

  // 정리
  close(fd);
  return 0;
}