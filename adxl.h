/**
 * @file adxl.h
 * @author Ali Nasrolahi (A.Nasrolahi01@gmail.com)
 * @date 2025-03-28
 */
#pragma once

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include "uadxl.h"

#define ADXL_MAX_DEVICES 32
#define ADXL_OF_COMPAT_ID 0xcafe
#define ADXL_OF_COMPAT_DEVICE "zephyr,adxl345"
#define ADXL_BUF_SIZE 1024

#define ADXL345_REG_DEVID 0x00
#define ADXL345_REG_OFSX 0x1E
#define ADXL345_REG_OFSY 0x1F
#define ADXL345_REG_OFSZ 0x20
#define ADXL345_REG_OFS_AXIS(index) (ADXL345_REG_OFSX + (index))
#define ADXL345_REG_BW_RATE 0x2C
#define ADXL345_REG_POWER_CTL 0x2D
#define ADXL345_REG_DATA_FORMAT 0x31
#define ADXL345_REG_DATAX0 0x32
#define ADXL345_REG_DATAY0 0x34
#define ADXL345_REG_DATAZ0 0x36
#define ADXL345_REG_DATA_AXIS(index) \
	(ADXL345_REG_DATAX0 + (index) * sizeof(__le16))
#define ADXL345_REG_INT_ENABLE 0x2E

#define ADXL345_BW_RATE GENMASK(3, 0)

#define ADXL345_POWER_CTL_MEASURE BIT(3)
#define ADXL345_POWER_CTL_STANDBY 0x00

#define ADXL345_DATA_FORMAT_RANGE GENMASK(1, 0) /* Set the g range */
#define ADXL345_DATA_FORMAT_JUSTIFY BIT(2) /* Left-justified (MSB) mode */
#define ADXL345_DATA_FORMAT_FULL_RES BIT(3) /* Up to 13-bits resolution */
#define ADXL345_DATA_FORMAT_SPI_3WIRE BIT(6) /* 3-wire SPI mode */
#define ADXL345_DATA_FORMAT_SELF_TEST BIT(7) /* Enable a self test */

#define ADXL345_DATA_FORMAT_2G 0
#define ADXL345_DATA_FORMAT_4G 1
#define ADXL345_DATA_FORMAT_8G 2
#define ADXL345_DATA_FORMAT_16G 3

#define ADXL345_DEVID 0xE5

#define ADXL345_INT_OVERRUN BIT(0)
#define ADXL345_INT_WATERMARK BIT(1)
#define ADXL345_INT_FREE_FALL BIT(2)
#define ADXL345_INT_INACTIVITY BIT(3)
#define ADXL345_INT_ACTIVITY BIT(4)
#define ADXL345_INT_DOUBLE_TAP BIT(5)
#define ADXL345_INT_SINGLE_TAP BIT(6)
#define ADXL345_INT_DATA_READY BIT(7)

// #define ENABLE_INTERRUPT

struct adxl_device {
	struct cdev cdev;
	struct spi_device *spidev;
	struct device *device;
	struct regmap *regmap;
	int irq;
	int sample_rate;
	int measurement_range;
	int x, y, z;
};

int adxl345_sysfs_init(struct adxl_device *);
int adxl345_sysfs_deinit(struct adxl_device *);

int adxl345_probe(struct adxl_device *adxl);
int adxl345_update_axis(struct adxl_device *adxl);
int adxl345_read_x(struct adxl_device *adxl);
int adxl345_read_y(struct adxl_device *adxl);
int adxl345_read_z(struct adxl_device *adxl);
int adxl345_read_rate(struct adxl_device *adxl);
int adxl345_read_range(struct adxl_device *adxl);
int adxl345_write_range(struct adxl_device *adxl, u8 range);
int adxl345_write_rate(struct adxl_device *adxl, u8 rate);
int adxl345_enable(struct adxl_device *adxl);
int adxl345_disable(struct adxl_device *adxl);