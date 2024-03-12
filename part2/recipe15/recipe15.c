/*
 * recipe15.c
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
#include <linux/gpio/consumer.h>
#include <linux/property.h>
#include <linux/interrupt.h>
#include <linux/timer.h>

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sysfs.h>
#include <linux/sched/signal.h>

#define DEVICE_NAME 		"recipedev"
#define US_TO_NS(x) 		(x * 1000L)
#define	SIGNUM				50

struct recipe15{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
};
static struct task_struct *task = NULL;

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe15 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe15, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe15 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe15, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	if (task != NULL){
		task = NULL;
	}
	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

static ssize_t recipe_write(struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
	struct recipe15 * recipe_private;
	struct device *dev;
	char buffer[100];
	int i;
	struct siginfo info;

	recipe_private = container_of(filp->private_data, struct recipe15, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	task = get_current();
	dev_info(dev, "recipe_write() is called. and task pid is %d \n", task->pid);

	if(copy_from_user(&buffer, buff, count)) {
		dev_info(dev, "Bad copied value\n");
		return -EFAULT;
	}
	for(i = 0; i < count; i++){
		dev_info(dev, "user data is %d\n", buffer[i]);
	}
	
	memset(&info, 0, sizeof(info));
	info.si_signo = SIGNUM;
	info.si_code = SI_QUEUE;

	if (send_sig_info(SIGNUM, (struct kernel_siginfo *) &info, task) < 0){
		dev_info(dev, "Error sending signal");
	}
	
	return count;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
//	.read = recipe_read,
//	.unlocked_ioctl = recipe_ioctl,
	.write = recipe_write,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct recipe15 *recipe_private;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe15), GFP_KERNEL);
	recipe_private->pdev = pdev;

	platform_set_drvdata(pdev, recipe_private);

	recipe_private->recipe_miscdevice.name = DEVICE_NAME;
	recipe_private->recipe_miscdevice.minor = MISC_DYNAMIC_MINOR;
	recipe_private->recipe_miscdevice.fops = &recipe_fops;
	recipe_private->recipe_miscdevice.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	ret = misc_register(&recipe_private->recipe_miscdevice);
	if (ret){
		return ret; /* misc_register returns 0 if success */
	}
	return 0;
}

/* Add remove() function */
static int __exit recipe_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct recipe15 *recipe_private = platform_get_drvdata(pdev);

	dev_info(dev, "recipe_remove() function is called.\n");

	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe15"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe15_asynch",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & async. module ");
