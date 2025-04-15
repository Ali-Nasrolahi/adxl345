#include "adxl.h"

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

struct file_operations adxl_fops = {
	.owner = THIS_MODULE,
	.open = adxl_open,
	.release = adxl_release,
	.read = adxl_read,
	.unlocked_ioctl = adxl_ioctl,
};
