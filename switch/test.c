#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "gpiosw.h"

void sigusr1_handler(int signo) {
  if (signo == SIGUSR1) {
    puts("Button clicked!");
    signal(SIGUSR1, sigusr1_handler);
  }
}

int main(void) {
  int fd;
  pid_t pid = getpid();

  if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR) {
    perror("signal");
    return 1;
  }

  fd = open("/dev/gpiosw", O_RDONLY);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  if (ioctl(fd, GPIO_IOCTL_REGISTER_PID, pid) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close(fd);
    return 1;
  }

  printf("Process %d registered, waiting for button press...\n", pid);

  while (1) {
    pause();
  }

  close(fd);
  return 0;
}