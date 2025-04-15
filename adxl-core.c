#include "adxl.h"

static const struct regmap_config regmap_spi_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.read_flag_mask = (BIT(7) | BIT(6)), /* Enable multi-byte read */
};

int adxl345_enable(struct adxl_device *adxl)
{
	return regmap_write(adxl->regmap, ADXL345_REG_POWER_CTL,
			    ADXL345_POWER_CTL_MEASURE);
}

int adxl345_disable(struct adxl_device *adxl)
{
	return regmap_write(adxl->regmap, ADXL345_REG_POWER_CTL,
			    ADXL345_POWER_CTL_STANDBY);
}

int adxl345_read_range(struct adxl_device *adxl)
{
	int ret, val;
	if ((ret = regmap_read(adxl->regmap, ADXL345_REG_DATA_FORMAT, &val)))
		return ret;
	adxl->measurement_range = ADXL345_DATA_FORMAT_RANGE & val;
	return 0;
}

int adxl345_write_range(struct adxl_device *adxl, u8 range)
{
	int ret = regmap_update_bits(adxl->regmap, ADXL345_REG_DATA_FORMAT,
				     ADXL345_DATA_FORMAT_RANGE, range);
	return ret < 0 ? ret :
			 (adxl->measurement_range = ADXL345_DATA_FORMAT_RANGE &
						    range);
}

int adxl345_read_rate(struct adxl_device *adxl)
{
	int ret, val;
	if ((ret = regmap_read(adxl->regmap, ADXL345_REG_BW_RATE, &val)))
		return ret;
	adxl->sample_rate = ADXL345_BW_RATE & val;
	return 0;
}

int adxl345_write_rate(struct adxl_device *adxl, u8 rate)
{
	int ret = regmap_update_bits(adxl->regmap, ADXL345_REG_BW_RATE,
				     ADXL345_BW_RATE, rate);
	return ret < 0 ? ret : (adxl->sample_rate = ADXL345_REG_BW_RATE & rate);
}

int adxl345_read_x(struct adxl_device *adxl)
{
	int ret = regmap_bulk_read(adxl->regmap, ADXL345_REG_DATAX0,
				   (s16 *)&adxl->x, 2);
	if (ret < 0)
		return ret;
	return 0;
}

int adxl345_read_y(struct adxl_device *adxl)
{
	int ret = regmap_bulk_read(adxl->regmap, ADXL345_REG_DATAY0,
				   (s16 *)&adxl->y, 2);
	if (ret < 0)
		return ret;
	return 0;
}

int adxl345_read_z(struct adxl_device *adxl)
{
	int ret = regmap_bulk_read(adxl->regmap, ADXL345_REG_DATAZ0,
				   (s16 *)&adxl->z, 2);
	if (ret < 0)
		return ret;
	return 0;
}

int adxl345_update_axis(struct adxl_device *adxl)
{
	int ret;
	u8 xyz_val[6];
	if ((ret = regmap_bulk_read(adxl->regmap, ADXL345_REG_DATAX0, xyz_val,
				    sizeof(xyz_val)))) {
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
	u32 regval;

	// 0. Regmap init
	adxl->regmap = devm_regmap_init_spi(spidev, &regmap_spi_config);
	if (IS_ERR(adxl->regmap))
		return dev_err_probe(dev, PTR_ERR(adxl->regmap),
				     "Failed to initialize regmap\n");

	// 1. Check if ADXL is actually there!?
	if ((ret = regmap_read(adxl->regmap, ADXL345_REG_DEVID, &regval)) < 0)
		return dev_err_probe(dev, ret, "Failed to read DEVID\n");

	if (regval != ADXL345_DEVID)
		return dev_err_probe(
			dev, -EINVAL,
			"Invalid DEVID, received 0x%x, expected 0x%x\n", regval,
			ADXL345_DEVID);

	// 2. Set data format
	if ((ret = regmap_write(adxl->regmap, ADXL345_REG_DATA_FORMAT,
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