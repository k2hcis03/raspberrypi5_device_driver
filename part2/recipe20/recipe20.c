
/*
 * recipe20.c
 *
 *  Created on: 2023. 12. 18.
 *      Author: k2h
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/kthread.h>             //kernel threads
#include <linux/delay.h>
#include <linux/gpio/consumer.h>

#include "ioctl.h"
#include <linux/pwm.h>
#include <linux/clk.h>
#include <linux/io.h>
//#include <linux/err.h>

#define DEVICE_NAME 		"recipedev"

struct recipe20{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	struct pwm_device *pwm0;
	struct ioctl_info info;
	u32 pwm_on_time_ns;
	u32 pwm_period_ns;
};

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe20 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe20, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe20 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe20, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

/* declare a ioctl_function */
static long recipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	struct recipe20 * recipe_private;
	struct device *dev;

	recipe_private = container_of(file->private_data, struct recipe20, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_ioctl() is called. cmd = %d, arg = %ld\n", cmd, arg);

	switch (cmd) {
		case SET_DATA:
			dev_info(dev, "SET_DATA\n");
			if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
				return -EFAULT;
			}
			if (info.pwm_on_time_ns >= 0){
				recipe_private->pwm_on_time_ns = info.pwm_on_time_ns;
			}else{
				return -EINVAL;			
			}
			if (info.pwm_period_ns >= 0){
				recipe_private->pwm_period_ns = info.pwm_period_ns;
			}else{
				return -EINVAL;	
			}
			pwm_config(recipe_private->pwm0, recipe_private->pwm_on_time_ns, recipe_private->pwm_period_ns);
			break;
		case GET_DATA:
			dev_info(dev, "GET_DATA\n");
			if (copy_to_user((void __user *)arg, &info, sizeof(info))) {
				return -EFAULT;
			}
			break;
		default:
			dev_info(dev, "invalid command %d\n", cmd);
		return -EFAULT;
	}
	return 0;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
	.release = recipe_close,
	.unlocked_ioctl 	= recipe_ioctl
};

static int recipe_probe(struct platform_device *pdev){
	int ret;
	struct device *dev = &pdev->dev;
	struct recipe20 *recipe_private;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(dev, sizeof(struct recipe20), GFP_KERNEL);
	if(!recipe_private){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->pdev = pdev;
	recipe_private->pwm_on_time_ns = 500000;			//0.5khz
	recipe_private->pwm_period_ns = 1000000;			//1khz
	recipe_private->pwm0 = devm_pwm_get(dev, NULL);
	if (IS_ERR(recipe_private->pwm0)){
		dev_err(dev, "Could not get PWM\n");
		return -ENOMEM;
	}

	ret = pwm_config(recipe_private->pwm0, recipe_private->pwm_on_time_ns, recipe_private->pwm_period_ns);
	if (ret < 0){
		dev_err(dev, "pwm_config pwm0 failed\n");
		return -ENOMEM;
	}
	ret = pwm_enable(recipe_private->pwm0);
	if (ret < 0){
		dev_err(dev, "pwm_enable pwm0 failed\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, recipe_private);
	
	recipe_private->recipe_miscdevice.name = DEVICE_NAME;
	recipe_private->recipe_miscdevice.minor = MISC_DYNAMIC_MINOR;
	recipe_private->recipe_miscdevice.fops = &recipe_fops;
	recipe_private->recipe_miscdevice.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret){
		return ret; /* misc_register returns 0 if success */
	}
	dev_info(dev, "recipe_probe() function is completed.\n");
	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct recipe20 *recipe_private = platform_get_drvdata(pdev);

	dev_info(dev, "recipe_remove() function is called.\n");
	pwm_disable(recipe_private->pwm0);
	pwm_free(recipe_private->pwm0);
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe20", },
	{ }
};
MODULE_DEVICE_TABLE(of, recipe_of_ids);

static struct platform_driver recipe_platform_driver = {
		.probe   = recipe_probe,
		.remove  = recipe_remove,
		.driver = {
			.name = "recipe20_pwm",
			.owner = THIS_MODULE,
			.of_match_table = recipe_of_ids,
		},
};
/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & pwm. module ");
