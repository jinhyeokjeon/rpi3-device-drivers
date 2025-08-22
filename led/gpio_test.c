#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
  char buf[BUFSIZ];
  char i = 0;
  int fd = -1;

  memset(buf, 0, BUFSIZ);

  printf("GPIO Set : %s\n", argv[1]);

  fd = open("/dev/gpio17", O_RDWR);
  if (fd == -1) {
    puts("open error");
    return -1;
  }
  write(fd, argv[1], strlen(argv[1]));
  read(fd, buf, BUFSIZ);

  printf("%s", buf);

  close(fd);

  return 0;
}