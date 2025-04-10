/**
 * @file uadxl.h
 * @author Ali Nasrolahi (A.Nasrolahi01@gmail.com)
 * @brief Userspace Header
 * @date 2025-03-28
 */
#pragma once

#include <linux/ioctl.h>

#define ADXL_MAGIC 0x4c
#define ADXL_MAXNR 6

#define ADXL_IOCTL_ENABLE _IO(ADXL_MAGIC, 0)
#define ADXL_IOCTL_DISABLE _IO(ADXL_MAGIC, 1)
#define ADXL_IOCTL_GET_RATE _IOR(ADXL_MAGIC, 2, int)
#define ADXL_IOCTL_SET_RATE _IOW(ADXL_MAGIC, 3, int)
#define ADXL_IOCTL_GET_RANGE _IOR(ADXL_MAGIC, 4, int)
#define ADXL_IOCTL_SET_RANGE _IOW(ADXL_MAGIC, 5, int)
#define ADXL_IOCTL_CALIBRATE _IO(ADXL_MAGIC, 6)
