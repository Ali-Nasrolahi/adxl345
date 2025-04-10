#include "adxl.h"

#define ADXL345_SPI_READ_SINGLE(reg) ((reg) | 0x80)
#define ADXL345_SPI_READ_MULTI(reg) ((reg) | 0xC0)
#define ADXL345_SPI_WRITE_SINGLE(reg) ((reg) & 0x3F)
#define ADXL345_SPI_WRITE_MULTI(reg) ((reg) | 0x40)

static int adxl345_read_reg8(struct adxl_device *adxl, u8 reg)
{
	return spi_w8r8(adxl->spidev, ADXL345_SPI_READ_SINGLE(reg));
}

static int adxl345_read_reg16(struct adxl_device *adxl, u8 reg)
{
	int ret;
	u8 rx[2];
	reg = ADXL345_SPI_READ_MULTI(reg);
	if ((ret = spi_write_then_read(adxl->spidev, &reg, 1, rx, 2)) < 0)
		return dev_err(&adxl->spidev->dev,
			       "failed to read a register!"),
		       ret;

	return (u16)((rx[1] << 8) | rx[0]);
}

static int adxl345_write_reg8(struct adxl_device *adxl, u8 reg, u8 val)
{
	u8 buf[2] = { ADXL345_SPI_WRITE_SINGLE(reg), val };
	return spi_write(adxl->spidev, buf, sizeof(buf));
}

static int adxl345_update_reg(struct adxl_device *adxl, u8 reg, u8 mask, u8 val)
{
	int ret;
	u8 tmp, orig;
	if ((ret = adxl345_read_reg8(adxl, reg)) < 0)
		return ret;
	orig = (u8)ret;
	tmp = orig & ~mask;
	tmp |= val & mask;
	return adxl345_write_reg8(adxl, reg, tmp);
}

int adxl345_enable(struct adxl_device *adxl)
{
	return adxl345_write_reg8(adxl, ADXL345_REG_POWER_CTL,
				  ADXL345_POWER_CTL_MEASURE);
}

int adxl345_disable(struct adxl_device *adxl)
{
	return adxl345_write_reg8(adxl, ADXL345_REG_POWER_CTL,
				  ADXL345_POWER_CTL_STANDBY);
}

int adxl345_read_range(struct adxl_device *adxl)
{
	int ret = adxl345_read_reg8(adxl, ADXL345_REG_DATA_FORMAT);
	return ret < 0 ? ret :
			 (adxl->measurement_range =
				  ADXL345_DATA_FORMAT_RANGE & ret);
}

int adxl345_write_range(struct adxl_device *adxl, u8 range)
{
	int ret = adxl345_update_reg(adxl, ADXL345_REG_DATA_FORMAT,
				     ADXL345_DATA_FORMAT_RANGE, range);
	return ret < 0 ? ret :
			 (adxl->measurement_range =
				  ADXL345_DATA_FORMAT_RANGE & range);
}

int adxl345_read_rate(struct adxl_device *adxl)
{
	int ret = adxl345_read_reg8(adxl, ADXL345_REG_BW_RATE);
	return ret < 0 ? ret : (adxl->sample_rate = ADXL345_BW_RATE & ret);
}

int adxl345_write_rate(struct adxl_device *adxl, u8 rate)
{
	int ret = adxl345_update_reg(adxl, ADXL345_REG_BW_RATE, ADXL345_BW_RATE,
				     rate);
	return ret < 0 ? ret : (adxl->sample_rate = ADXL345_BW_RATE & rate);
}

int adxl345_read_x(struct adxl_device *adxl)
{
	int ret;
	if ((ret = adxl345_read_reg16(adxl, ADXL345_REG_DATAX0)) < 0)
		return ret;
	adxl->x = (s16)ret;
	return 0;
}

int adxl345_read_y(struct adxl_device *adxl)
{
	int ret;
	if ((ret = adxl345_read_reg16(adxl, ADXL345_REG_DATAY0)) < 0)
		return ret;
	adxl->y = (s16)ret;
	return 0;
}

int adxl345_read_z(struct adxl_device *adxl)
{
	int ret;
	if ((ret = adxl345_read_reg16(adxl, ADXL345_REG_DATAZ0)) < 0)
		return ret;
	adxl->z = (s16)ret;
	return 0;
}

int adxl345_update_axis(struct adxl_device *adxl)
{
	int ret;
	u8 xyz_val[6];
	u8 reg = ADXL345_SPI_READ_MULTI(ADXL345_REG_DATAX0);
	if ((ret = spi_write_then_read(adxl->spidev, &reg, 1, xyz_val, 6))) {
		dev_dbg(&adxl->spidev->dev, "Failed to update axis\n");
		return ret;
	}

	adxl->x = (int16_t)((xyz_val[1] << 8) | xyz_val[0]);
	adxl->y = (int16_t)((xyz_val[3] << 8) | xyz_val[2]);
	adxl->z = (int16_t)((xyz_val[5] << 8) | xyz_val[4]);

	return 0;
}

int adxl345_probe(struct adxl_device *adxl)
{
	struct spi_device *spidev = adxl->spidev;
	struct device *dev = &spidev->dev;
	int ret;

	// 1. Check if ADXL is actually there!?
	if (adxl345_read_reg8(adxl, ADXL345_REG_DEVID) != ADXL345_DEVID)
		return dev_err_probe(dev, -EINVAL, "Invalid DEVID received\n");

	// 2. Set data format
	if ((ret = adxl345_write_reg8(adxl, ADXL345_REG_DATA_FORMAT,
				      ADXL345_DATA_FORMAT_FULL_RES)))
		return dev_err_probe(dev, ret, "Failed to set data format\n");

	// 3. Enable measurement
	if ((ret = adxl345_enable(adxl)))
		return dev_err_probe(dev, ret,
				     "Failed to enable measurement\n");

	// 4. Test if everything works!
	if ((ret = adxl345_update_axis(adxl)) < 0)
		return dev_err_probe(dev, ret, "Failed to read measurement\n");

	dev_info(dev, "Read axes are x: %d, y: %d, z: %d during probe\n",
		 adxl->x, adxl->y, adxl->z);

	adxl345_read_y(adxl);
	adxl345_read_x(adxl);
	adxl345_read_z(adxl);

	dev_info(dev,
		 "Separate read axes are x: %d, y: %d, z: %d during probe\n",
		 adxl->x, adxl->y, adxl->z);

	return 0;
}