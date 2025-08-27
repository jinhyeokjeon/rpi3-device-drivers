#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define SYSFS_PATH_CO2  "/sys/bus/i2c/devices/1-0062/co2"
#define SYSFS_PATH_TEMP "/sys/bus/i2c/devices/1-0062/temp"
#define SYSFS_PATH_HUM  "/sys/bus/i2c/devices/1-0062/hum"

int read_sysfs_value(const char* path, char* buf, size_t size) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  ssize_t len = read(fd, buf, size - 1);
  if (len < 0) {
    perror("read");
    close(fd);
    return -1;
  }

  buf[len - 1] = '\0';  // 문자열 끝
  close(fd);
  return 0;
}

int main() {
  char co2[32], temp[32], hum[32];

  if (read_sysfs_value(SYSFS_PATH_CO2, co2, sizeof(co2)) < 0) return 1;
  if (read_sysfs_value(SYSFS_PATH_TEMP, temp, sizeof(temp)) < 0) return 1;
  if (read_sysfs_value(SYSFS_PATH_HUM, hum, sizeof(hum)) < 0) return 1;

  printf("  SCD41 Sensor Data\n");
  printf("  CO2   : %s ppm\n", co2);
  printf("  Temp  : %s °C\n", temp);
  printf("  Hum   : %s %%\n", hum);

  return 0;
}