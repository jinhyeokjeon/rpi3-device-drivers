#ifndef __HD44780_H_
#define __HD44780_H_

#include <linux/ioctl.h>
/* ioctl 명령 정의 */
#define LCD_IOCTL_MAGIC 'L'

enum lcd_ioctl_cmd {
  LCD_IOCTL_BACKLIGHT_ON = _IO(LCD_IOCTL_MAGIC, 0),
  LCD_IOCTL_BACKLIGHT_OFF = _IO(LCD_IOCTL_MAGIC, 1),
  LCD_IOCTL_DISPLAY_ON = _IO(LCD_IOCTL_MAGIC, 2),
  LCD_IOCTL_DISPLAY_OFF = _IO(LCD_IOCTL_MAGIC, 3),
  LCD_IOCTL_CLEAR = _IO(LCD_IOCTL_MAGIC, 4),
  LCD_IOCTL_HOME = _IO(LCD_IOCTL_MAGIC, 5),
  LCD_IOCTL_ENTRY_MODE = _IOW(LCD_IOCTL_MAGIC, 6, int),
};

/* Entry Mode flags (0x04 ~ 0x07) */
#define LCD_ENTRY_LEFT        0x04
#define LCD_ENTRY_LEFT_SHIFT  0x05
#define LCD_ENTRY_RIGHT       0x06   // 기본
#define LCD_ENTRY_RIGHT_SHIFT 0x07

#endif