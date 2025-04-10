#include "adxl.h"

/* Global Driver Data */
static int major_number;
static atomic_t device_count = ATOMIC_INIT(0);
static struct class *adxl_class;

static const struct of_device_id adxl_of_match[] = {
	{ .compatible = ADXL_OF_COMPAT_DEVICE,
	  .data = (void *)ADXL_OF_COMPAT_ID },
	{}
};

MODULE_DEVICE_TABLE(of, adxl_of_match);

/* File Operations --- Begin */
static int adxl_open(struct inode *inode, struct file *file)
{
	struct adxl_device *adxl =
		container_of(inode->i_cdev, struct adxl_device, cdev);

	file->private_data = adxl;

	dev_dbg(&adxl->spidev->dev, "new fd opened\n");

	return 0;
}

static int adxl_release(struct inode *inode, struct file *file)
{
	struct adxl_device *adxl =
		container_of(inode->i_cdev, struct adxl_device, cdev);

	dev_dbg(&adxl->spidev->dev, "fd released\n");

	return 0;
}

static ssize_t adxl_read(struct file *file, char __user *ubuf, size_t len,
			 loff_t *offset)
{
	struct adxl_device *adxl = file->private_data;

	char *kbuf = kvmalloc(ADXL_BUF_SIZE, GFP_KERNEL);
	if (!kbuf)
		return kfree(kbuf), -ENOMEM;

	if (adxl345_update_axis(adxl) != 0)
		return kfree(kbuf), -EFAULT;

	snprintf(kbuf, ADXL_BUF_SIZE, "%d,%d,%d\n", adxl->x, adxl->y, adxl->z);

	if (*offset >= strlen(kbuf))
		return kfree(kbuf), 0;

	len = umin(len, strlen(kbuf) - *offset);

	if (copy_to_user(ubuf, kbuf + *offset, len))
		return kfree(kbuf), -EFAULT;

	*offset += len;
	kfree(kbuf);
	return len;
}

static long adxl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct adxl_device *dev = file->private_data;

	if (_IOC_TYPE(cmd) != ADXL_MAGIC || _IOC_NR(cmd) > ADXL_MAXNR)
		return -ENOTTY;

	int tmpval;
	switch (cmd) {
	case ADXL_IOCTL_CALIBRATE:
		return -ENOTTY;
		break;

	case ADXL_IOCTL_ENABLE:
		return adxl345_enable(dev);

	case ADXL_IOCTL_DISABLE:
		return adxl345_disable(dev);

	case ADXL_IOCTL_GET_RATE:
		if (adxl345_read_rate(dev) < 0 ||
		    put_user(dev->sample_rate, (int __user *)arg))
			return -EFAULT;
		dev_dbg(&dev->spidev->dev, "ioctl_get_rate %d\n",
			dev->sample_rate);
		break;

	case ADXL_IOCTL_SET_RATE:
		if (get_user(tmpval, (int __user *)arg) ||
		    adxl345_write_rate(dev, tmpval) < 0)
			return -EFAULT;
		dev_dbg(&dev->spidev->dev, "ioctl_set_rate %d\n",
			dev->sample_rate);
		break;

	case ADXL_IOCTL_GET_RANGE:
		if (adxl345_read_range(dev) < 0 ||
		    put_user(dev->measurement_range, (int __user *)arg))
			return -EFAULT;
		dev_dbg(&dev->spidev->dev, "ioctl_get_range %d\n",
			dev->measurement_range);
		break;

	case ADXL_IOCTL_SET_RANGE:
		if (get_user(tmpval, (int __user *)arg) ||
		    adxl345_write_range(dev, tmpval) < 0)
			return -EFAULT;
		dev_dbg(&dev->spidev->dev, "ioctl_set_range %d\n",
			dev->measurement_range);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static struct file_operations adxl_fops = {
	.owner = THIS_MODULE,
	.open = adxl_open,
	.release = adxl_release,
	.read = adxl_read,
	.unlocked_ioctl = adxl_ioctl,
};

/* File Operations --- End */

/* Sysfs --- Being */

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	return adxl345_enable(dev_get_drvdata(dev));
}

static ssize_t disable_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	return adxl345_disable(dev_get_drvdata(dev));
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

/* Sysfs --- End */

static int adxl_probe(struct spi_device *c)
{
	int ret;
	struct device *dev = &c->dev;

	/* 1. ADXL Device Creation */
	struct adxl_device *adxl_device =
		devm_kzalloc(dev, sizeof(struct adxl_device), GFP_KERNEL);

	if (!adxl_device)
		return -ENOMEM;

	adxl_device->spidev = c;
	spi_set_drvdata(c, adxl_device);

	/* 2. Setup the sensor */
	if ((ret = adxl345_probe(adxl_device)))
		return dev_err_probe(dev, ret, "Sensor setup failed\n");

#if 0
	if (device_property_read_u32(&c->dev, "test",
				     &adxl_device->test))
		return dev_err(dev,
			       "Driver needs 'test' property to be specified!\n"),
		       -EINVAL;
#endif

	/* 3. Character Device preparations */
	int minor = atomic_fetch_inc(&device_count);
	dev_t devno = MKDEV(major_number, minor);

	cdev_init(&adxl_device->cdev, &adxl_fops);
	ret = cdev_add(&adxl_device->cdev, devno, 1);
	if (ret < 0) {
		atomic_dec(&device_count);
		return dev_err_probe(&c->dev, ret, "Failed to add cdev\n");
	}

	adxl_device->device =
		device_create(adxl_class, dev, devno, NULL, "adxl%d", minor);
	if (IS_ERR(adxl_device->device)) {
		cdev_del(&adxl_device->cdev);
		atomic_dec(&device_count);
		return dev_err_probe(&c->dev, PTR_ERR(adxl_device->device),
				     "Failed to create device\n");
	}

	dev_set_drvdata(adxl_device->device, adxl_device);

	device_create_file(adxl_device->device, &dev_attr_enable);
	device_create_file(adxl_device->device, &dev_attr_disable);
	device_create_file(adxl_device->device, &dev_attr_rate);
	device_create_file(adxl_device->device, &dev_attr_range);
	device_create_file(adxl_device->device, &dev_attr_x);
	device_create_file(adxl_device->device, &dev_attr_y);
	device_create_file(adxl_device->device, &dev_attr_z);

	dev_info(&c->dev, "Device probed!\n");

	return 0;
}

static void adxl_remove(struct spi_device *c)
{
	struct adxl_device *adxl_device = spi_get_drvdata(c);
	dev_t devno = adxl_device->cdev.dev;
	device_remove_file(adxl_device->device, &dev_attr_z);
	device_remove_file(adxl_device->device, &dev_attr_y);
	device_remove_file(adxl_device->device, &dev_attr_x);
	device_remove_file(adxl_device->device, &dev_attr_range);
	device_remove_file(adxl_device->device, &dev_attr_rate);
	device_remove_file(adxl_device->device, &dev_attr_disable);
	device_remove_file(adxl_device->device, &dev_attr_enable);
	cdev_del(&adxl_device->cdev);
	device_destroy(adxl_class, devno);
	atomic_dec(&device_count);
	dev_info(&c->dev, "Client removed!\n");
}

static struct spi_driver adxl_driver = {
    .probe = adxl_probe,
    .remove = adxl_remove,
    .driver = {
        .name = "adxl",
        .of_match_table = adxl_of_match,
		.owner = THIS_MODULE,
    },
};

static int __init adxl_init(void)
{
	int ret;
	dev_t dev;

	ret = alloc_chrdev_region(&dev, 0, ADXL_MAX_DEVICES, "adxl");
	if (ret < 0) {
		pr_err("Failed to allocate chr region\n");
		return ret;
	}

	major_number = MAJOR(dev);

	adxl_class = class_create("adxl_class");
	if (IS_ERR(adxl_class)) {
		pr_err("Failed to create device class\n");
		ret = PTR_ERR(adxl_class);
		goto fail_class;
	}

	ret = spi_register_driver(&adxl_driver);
	if (ret < 0) {
		pr_err("Failed to register platform driver\n");
		goto fail_platform;
	}

	pr_info("Driver loaded successfully with major number %d\n",
		major_number);

	return 0;

fail_platform:
	class_destroy(adxl_class);
fail_class:
	unregister_chrdev_region(dev, ADXL_MAX_DEVICES);
	return ret;
}

static void __exit adxl_exit(void)
{
	spi_unregister_driver(&adxl_driver);
	class_destroy(adxl_class);
	unregister_chrdev_region(MKDEV(major_number, 0), ADXL_MAX_DEVICES);

	pr_info("Driver unloaded\n");
}

module_init(adxl_init);
module_exit(adxl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ali Nasrolahi <A.Nasrolahi01@gmail.com>");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.1");
