#include "adxl.h"

/* Global Driver Data */
static int major_number;
static atomic_t device_count = ATOMIC_INIT(0);
static struct class *adxl_class;
extern struct file_operations adxl_fops;

static const struct of_device_id adxl_of_match[] = {
	{ .compatible = ADXL_OF_COMPAT_DEVICE,
	  .data = (void *)ADXL_OF_COMPAT_ID },
	{}
};

MODULE_DEVICE_TABLE(of, adxl_of_match);

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

	/* 4. Enable sysfs access to the driver */
	dev_set_drvdata(adxl_device->device, adxl_device);
	adxl345_sysfs_init(adxl_device);

	dev_info(&c->dev, "Device probed!\n");

	return 0;
}

static void adxl_remove(struct spi_device *c)
{
	struct adxl_device *adxl_device = spi_get_drvdata(c);
	dev_t devno = adxl_device->cdev.dev;
	adxl345_sysfs_deinit(adxl_device);
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
MODULE_DESCRIPTION("ADXL345 Accelerometer SPI driver");
MODULE_VERSION("0.2");
