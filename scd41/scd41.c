#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kthread.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jinhyeok Jeon");
MODULE_DESCRIPTION("SCD41 I2C Driver (DT-based)");

#define SCD41_DRV_NAME  "scd41"

static u8 scd41_crc8(const u8* data, int len) {
  u8 crc = 0xFF;
  int i, j;
  for (j = 0; j < len; j++) {
    crc ^= data[j];
    for (i = 0; i < 8; i++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x31;
      else
        crc <<= 1;
    }
  }
  return crc;
}

static int last_co2_ppm;
static int last_temp_c;  /* 0.01 단위 */
static int last_hum_pc;  /* 0.01 단위 */
static bool scd41_enabled = false;

static int scd41_read_measurement(struct i2c_client* client) {
  u8 cmd[2], buf[9];
  int ret;
  u16 co2_raw, temp_raw, hum_raw;
  int co2_ppm, temp_c, hum_pc;

  /* 1. read command */
  cmd[0] = 0xEC; cmd[1] = 0x05;
  ret = i2c_master_send(client, cmd, 2);
  if (ret < 0) return ret;

  msleep(1);

  ret = i2c_master_recv(client, buf, 9);
  if (ret < 0) return ret;
  if (ret != 9) return -EIO;

  /* CRC 체크 */
  if (scd41_crc8(buf, 2) != buf[2] ||
    scd41_crc8(buf + 3, 2) != buf[5] ||
    scd41_crc8(buf + 6, 2) != buf[8])
    return -EIO;

  /* 파싱 */
  co2_raw = (buf[0] << 8) | buf[1];
  temp_raw = (buf[3] << 8) | buf[4];
  hum_raw = (buf[6] << 8) | buf[7];

  co2_ppm = co2_raw;
  temp_c = -4500 + (17500 * (int)temp_raw) / 65536;
  hum_pc = (10000 * (int)hum_raw) / 65536;

  last_co2_ppm = co2_ppm;
  last_temp_c = temp_c;
  last_hum_pc = hum_pc;

  return 0;
}

static struct task_struct* scd41_thread;
static int scd41_thread_fn(void* data) {
  struct i2c_client* client = data;
  u8 cmd[2];
  int ret;

  scd41_enabled = true;
  pr_info("scd41: measurement started\n");

  /* Start measurement */
  cmd[0] = 0x21; cmd[1] = 0xB1;
  ret = i2c_master_send(client, cmd, 2);
  if (ret < 0) {
    pr_err("scd41: failed to start measurement\n");
    return ret;
  }

  /* 루프: 5초마다 측정 */
  while (true) {
    for (int i = 0; i < 50; ++i) {
      if (kthread_should_stop()) {
        break;
      }
      msleep(100);
    }
    if (kthread_should_stop()) {
      break;
    }
    scd41_read_measurement(client);
  }

  /* Stop measurement */
  cmd[0] = 0x3F; cmd[1] = 0x86;
  i2c_master_send(client, cmd, 2);
  pr_info("scd41: measurement stopped\n");

  scd41_enabled = false;

  return 0;
}

static ssize_t co2_show(struct device* dev, struct device_attribute* attr, char* buf) {
  return sprintf(buf, "%d\n", last_co2_ppm);
}
static ssize_t temp_show(struct device* dev, struct device_attribute* attr, char* buf) {
  return sprintf(buf, "%d.%02d\n", last_temp_c / 100, last_temp_c % 100);
}
static ssize_t hum_show(struct device* dev, struct device_attribute* attr, char* buf) {
  return sprintf(buf, "%d.%02d\n", last_hum_pc / 100, last_hum_pc % 100);
}
static ssize_t enable_show(struct device* dev, struct device_attribute* attr, char* buf) {
  return sprintf(buf, "%d\n", scd41_enabled ? 1 : 0);
}
static ssize_t enable_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) {
  char tmp[16];

  if (count >= sizeof(tmp) - 1) {
    return -EINVAL;
  }

  memcpy(tmp, buf, count);
  tmp[count] = '\0';

  if (!strcmp(tmp, "1") || !strcmp(tmp, "1\n")) {
    if (scd41_enabled) {
      return count;
    }
    scd41_thread = kthread_run(scd41_thread_fn, dev_get_drvdata(dev), "scd41_thread");
    if (IS_ERR(scd41_thread)) {
      pr_err("scd41: failed to create kthread\n");
      return PTR_ERR(scd41_thread);
    }
  }
  else if (!strcmp(tmp, "0") || !strcmp(tmp, "0\n")) {
    if (!scd41_enabled) {
      return count;
    }
    if (scd41_thread) {
      kthread_stop(scd41_thread);
      scd41_thread = NULL;
    }
  }
  else {
    return -EINVAL;
  }

  return count;
}

/* struct device_attribute: sysfs에 만들어질 파일 하나 */
/* DEVICE_ATTR_R0(name) 매크로가 만들어줌 */
/* struct device_attribute dev_attr_co2 = { .attr = { .name = "co2", .mode = 0444 }, .show = co2_show, } */
static DEVICE_ATTR_RO(co2);
static DEVICE_ATTR_RO(temp);
static DEVICE_ATTR_RO(hum);
static DEVICE_ATTR_RW(enable);

static struct attribute* scd41_attrs[] = {
  &dev_attr_co2.attr,
  &dev_attr_temp.attr,
  &dev_attr_hum.attr,
  &dev_attr_enable.attr,
  NULL,
};

static const struct attribute_group scd41_group = {
    .attrs = scd41_attrs,
};

/* probe: 모듈 로딩 시 Device Tree에서 client 매칭되면 호출 */
static int scd41_probe(struct i2c_client* client) {
  int ret;

  pr_info("scd41: probe called, addr=0x%02x\n", client->addr);

  ret = sysfs_create_group(&client->dev.kobj, &scd41_group);
  if (ret) {
    pr_err("scd41: failed to create sysfs group\n");
    return ret;
  }

  dev_set_drvdata(&client->dev, client);

  return 0;
}

static void scd41_remove(struct i2c_client* client) {
  if (scd41_thread) {
    kthread_stop(scd41_thread);
  }
  sysfs_remove_group(&client->dev.kobj, &scd41_group);
  pr_info("scd41: removed\n");
}

/* DT 매칭 테이블 */
static const struct of_device_id scd41_of_match[] = {
    {.compatible = "sensirion,scd41" },
    { }
};
MODULE_DEVICE_TABLE(of, scd41_of_match);

/* I2C 이름 기반 매칭 (레거시 호환) */
static const struct i2c_device_id scd41_id[] = {
    { SCD41_DRV_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, scd41_id);

/* 드라이버 구조체 */
static struct i2c_driver scd41_driver = {
    .driver = {
        .name = SCD41_DRV_NAME,
        .of_match_table = scd41_of_match,
    },
    .probe = scd41_probe,
    .remove = scd41_remove,
    .id_table = scd41_id,
};

/* DT 기반일 땐 이 매크로 하나로 init/exit 처리 */
module_i2c_driver(scd41_driver);
/*
static int __init scd41_driver_init(void) {
  return i2c_add_driver(&scd41_driver);
}
static void __exit scd41_driver_exit(void) {
  i2c_del_driver(&scd41_driver);
}

module_init(scd41_driver_init);
module_exit(scd41_driver_exit);
*/