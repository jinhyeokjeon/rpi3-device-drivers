#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include "gpiosw.h"
#include "hd44780.h"

#define SYSFS_PATH_CO2     "/sys/bus/i2c/devices/1-0062/co2"
#define SYSFS_PATH_TEMP    "/sys/bus/i2c/devices/1-0062/temp"
#define SYSFS_PATH_HUM     "/sys/bus/i2c/devices/1-0062/hum"
#define SYSFS_PATH_ENABLE  "/sys/bus/i2c/devices/1-0062/enable"
#define GPIO_SW_PATH       "/dev/gpiosw"
#define HD44780_PATH       "/dev/hd44780"

void open_files();
void close_files();
void display_on();
void display_clear();
void set_cursor(int pos);
void write_to_lcd(char* buf);
void enable_scd41(bool on);

bool measuring = false;
int fd_co2, fd_temp, fd_hum, fd_sw, fd_lcd;
char co2_str[32], temp_str[32], hum_str[32];
void read_air_value();

bool running = false;

void* print_value(void* arg) {
  char buf[32];
  display_clear();
  read_air_value();
  sprintf(buf, "%s%cC\n%s%% / %sppm", temp_str, (char)0xDF, hum_str, co2_str);
  write_to_lcd(buf);
  usleep(1000000);
  while (running) {
    for (int i = 0; i < 50; ++i) {
      usleep(100000);
      if (!running) {
        break;
      }
    }
    if (!running) {
      break;
    }
    read_air_value();
    set_cursor(0); write_to_lcd(temp_str);
    set_cursor(16); write_to_lcd(hum_str);
    set_cursor(25); write_to_lcd(co2_str);
  }
}

void sigusr1_handler(int signo) {
  if (signo == SIGUSR1) {
    char buf[128];
    if (!measuring) {
      display_clear();
      enable_scd41(true);
      sprintf(buf, "MEASURING ... ");
      write_to_lcd(buf);
      for (int i = 5; i >= 1; --i) {
        sprintf(buf, "%d", i);
        write_to_lcd(buf);
        sleep(1);
        set_cursor(14);
      }
      measuring = true;

      pthread_t t;
      running = true;
      pthread_create(&t, NULL, print_value, NULL);
      pthread_detach(t);
    }
    else {
      running = false;
      enable_scd41(false);
      display_clear();
      sprintf(buf, "PRESS BUTTON TO\nSTART MEASURING");
      write_to_lcd(buf);
      measuring = false;
    }

    signal(SIGUSR1, sigusr1_handler);
  }
}

int main() {
  if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR) {
    perror("signal");
    return 1;
  }

  pid_t pid = getpid();
  open_files();

  if (ioctl(fd_sw, GPIO_IOCTL_REGISTER_PID, pid) < 0) {
    perror("ioctl LED_MODE_NORMAL");
    close_files();
    return 1;
  }

  display_on();
  write_to_lcd("PRESS BUTTON TO\nSTART MEASURING");

  while (1) {
    pause();
  }

  close_files();
  return 0;
}

void enable_scd41(bool on) {
  int fd_enable = open(SYSFS_PATH_ENABLE, O_WRONLY);
  if (fd_enable < 0) {
    perror("open enable");
    close_files();
    exit(1);
  }
  if (write(fd_enable, ((on == true) ? "1" : "0"), 1) < 0) {
    perror("write enable");
    close_files();
    exit(1);
  }
  close(fd_enable);
}

void read_air_value() {
  int len;
  lseek(fd_temp, 0, SEEK_SET);
  if ((len = read(fd_temp, temp_str, sizeof(temp_str) - 1)) < 0) {
    perror("read temp");
    close_files();
    exit(1);
  }
  temp_str[len - 1] = '\0';
  lseek(fd_hum, 0, SEEK_SET);
  if ((len = read(fd_hum, hum_str, sizeof(hum_str) - 1)) < 0) {
    perror("read hum");
    close_files();
    exit(1);
  }
  hum_str[len - 1] = '\0';
  lseek(fd_co2, 0, SEEK_SET);
  if ((len = read(fd_co2, co2_str, sizeof(co2_str) - 1)) < 0) {
    perror("read co2");
    close_files();
    exit(1);
  }
  co2_str[len - 1] = '\0';
}

void write_to_lcd(char* buf) {
  if (write(fd_lcd, buf, strlen(buf)) < 0) {
    perror("write ON");
    close_files();
    exit(1);
  }
}

void display_on() {
  if (ioctl(fd_lcd, LCD_IOCTL_CLEAR) < 0) {
    perror("ioctl LED_IOCTL_CLEAR");
    close_files();
    exit(1);
  }

  if (ioctl(fd_lcd, LCD_IOCTL_DISPLAY_ON) < 0) {
    perror("ioctl LED_IOCTL_DISPLAY_ON");
    close_files();
    exit(1);
  }

  if (ioctl(fd_lcd, LCD_IOCTL_BACKLIGHT_ON) < 0) {
    perror("ioctl LCD_IOCTL_BACKLIGHT_ON");
    close_files();
    exit(1);
  }
}
void display_clear() {
  if (ioctl(fd_lcd, LCD_IOCTL_CLEAR) < 0) {
    perror("ioctl LED_IOCTL_CLEAR");
    close_files();
    exit(1);
  }
}
void set_cursor(int pos) {
  if (ioctl(fd_lcd, LCD_IOCTL_SET_CURSOR, pos) < 0) {
    perror("ioctl LCD_IOCTL_SET_CURSOR");
    close_files();
    exit(1);
  }
}

void open_files() {
  fd_co2 = open(SYSFS_PATH_CO2, O_RDONLY);
  if (fd_co2 < 0) {
    perror("open co2");
    exit(1);
  }
  fd_temp = open(SYSFS_PATH_TEMP, O_RDONLY);
  if (fd_temp < 0) {
    perror("open temp");
    close(fd_co2);
    exit(1);
  }
  fd_hum = open(SYSFS_PATH_HUM, O_RDONLY);
  if (fd_hum < 0) {
    perror("open hum");
    close(fd_co2); close(fd_temp);
    exit(1);
  }
  fd_sw = open(GPIO_SW_PATH, O_RDONLY);
  if (fd_sw < 0) {
    perror("open gpiosw");
    close(fd_co2); close(fd_temp); close(fd_hum);
    exit(1);
  }
  fd_lcd = open(HD44780_PATH, O_WRONLY);
  if (fd_lcd < 0) {
    perror("open hd44780");
    close(fd_co2); close(fd_temp); close(fd_hum); close(fd_sw);
    exit(1);
  }
}

void close_files() {
  close(fd_co2);
  close(fd_temp);
  close(fd_hum);
  close(fd_sw);
  close(fd_lcd);
}