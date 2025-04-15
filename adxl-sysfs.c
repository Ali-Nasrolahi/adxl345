#include "adxl.h"

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int ret = adxl345_enable(dev_get_drvdata(dev));
	return ret < 0 ? ret : count;
}

static ssize_t disable_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret = adxl345_disable(dev_get_drvdata(dev));
	return ret < 0 ? ret : count;
}

static ssize_t rate_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	int ret;
	struct adxl_device *adxl = dev_get_drvdata(dev);
	if ((ret = adxl345_read_rate(adxl)) < 0)
		return ret;
	return sysfs_emit(buf, "%d\n", adxl->sample_rate);
}

static ssize_t rate_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct adxl_device *adxl = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	int ret = adxl345_write_rate(adxl, val);
	return ret < 0 ? ret : count;
}

static ssize_t range_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int ret;
	struct adxl_device *adxl = dev_get_drvdata(dev);
	if ((ret = adxl345_read_range(adxl)) < 0)
		return ret;
	return sysfs_emit(buf, "%d\n", adxl->measurement_range);
}

static ssize_t range_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct adxl_device *adxl = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	int ret = adxl345_write_range(adxl, val);
	return ret < 0 ? ret : count;
}

static ssize_t x_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	int ret;
	struct adxl_device *adxl = dev_get_drvdata(dev);
	if ((ret = adxl345_read_x(adxl)))
		return ret;
	return sysfs_emit(buf, "%d\n", adxl->x);
}

static ssize_t y_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	int ret;
	struct adxl_device *adxl = dev_get_drvdata(dev);
	if ((ret = adxl345_read_y(adxl)))
		return ret;
	return sysfs_emit(buf, "%d\n", adxl->y);
}

static ssize_t z_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	int ret;
	struct adxl_device *adxl = dev_get_drvdata(dev);
	if ((ret = adxl345_read_z(adxl)))
		return ret;
	return sysfs_emit(buf, "%d\n", adxl->z);
}

static DEVICE_ATTR_WO(enable);
static DEVICE_ATTR_WO(disable);
static DEVICE_ATTR_RW(rate);
static DEVICE_ATTR_RW(range);
static DEVICE_ATTR_RO(x);
static DEVICE_ATTR_RO(y);
static DEVICE_ATTR_RO(z);

int adxl345_sysfs_init(struct adxl_device *adxl_device)
{
	device_create_file(adxl_device->device, &dev_attr_enable);
	device_create_file(adxl_device->device, &dev_attr_disable);
	device_create_file(adxl_device->device, &dev_attr_rate);
	device_create_file(adxl_device->device, &dev_attr_range);
	device_create_file(adxl_device->device, &dev_attr_x);
	device_create_file(adxl_device->device, &dev_attr_y);
	device_create_file(adxl_device->device, &dev_attr_z);
	return 0;
}

int adxl345_sysfs_deinit(struct adxl_device *adxl_device)
{
	device_remove_file(adxl_device->device, &dev_attr_z);
	device_remove_file(adxl_device->device, &dev_attr_y);
	device_remove_file(adxl_device->device, &dev_attr_x);
	device_remove_file(adxl_device->device, &dev_attr_range);
	device_remove_file(adxl_device->device, &dev_attr_rate);
	device_remove_file(adxl_device->device, &dev_attr_disable);
	device_remove_file(adxl_device->device, &dev_attr_enable);
	return 0;
}