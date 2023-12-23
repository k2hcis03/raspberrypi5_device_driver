/*
 * recipe4.c
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
#include "ioctl.h"

#define  DEVICE_NAME "recipedev"
#define  CLASS_NAME  "recipe_class"

static struct class*  recipeClass;
static struct cdev recipe_dev;
dev_t dev;

/* declare a uevent call back_function */
static int recipe_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int recipe_open(struct inode *inode, struct file *file)
{
	pr_info("recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	pr_info("recipe_dev_close() is called.\n");
	return 0;
}
/* declare ioctl_function */
static long recipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	int i;
	pr_info("recipe_dev_ioctl() is called. cmd = %d, arg = %ld\n", cmd, arg);

	switch (cmd) {
		case SET_DATA:
			pr_info("SET_DATA\n");
			if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
				return -EFAULT;
			}
			for(i = 0; i < info.size; i++){
				pr_info("ioctl size : %d, ioctl buffer : %d",info.size, info.buffer[i]);
			}
			break;
		case GET_DATA:
			pr_info("GET_DATA\n");
			pr_info("ioctl size : %d, ioctl buf : %d\n", info.size, info.buffer[0]);
			memset(info.buffer, 0xAA, sizeof(info.buffer));
			info.size = sizeof(info.buffer);
			if (copy_to_user((void __user *)arg, &info, sizeof(info))) {
				return -EFAULT;
			}
			break;
		default:
			pr_info("invalid command %d\n", cmd);
		return -EFAULT;
	}
	return 0;
}
/* declare a file_operations structure */
static const struct file_operations recipe_dev_fops = {
	.owner			= THIS_MODULE,
	.open 		 	= recipe_open,
	.release 	 	= recipe_close,
	.unlocked_ioctl 	= recipe_ioctl,
};

static int __init recipe_init(void)
{
	int ret;
	dev_t dev_no;
	int Major;

	struct device* recipeDevice;

	pr_info("Hello world init\n");

	/* Allocate dynamically device numbers */
	ret = alloc_chrdev_region(&dev_no, 0, 1, DEVICE_NAME);
	if (ret < 0){
		pr_info("Unable to allocate Mayor number \n");
		return ret;
	}

	/* Get the device identifiers */
	Major = MAJOR(dev_no);
	dev = MKDEV(Major,0);

	pr_info("Allocated correctly with major number %d\n", Major);

	/* Initialize the cdev structure and add it to the kernel space */
	cdev_init(&recipe_dev, &recipe_dev_fops);
	ret = cdev_add(&recipe_dev, dev, 1);
	if (ret < 0){
		unregister_chrdev_region(dev, 1);
		pr_info("Unable to add cdev\n");
		return ret;
	}
	/* Register the device class */
	recipeClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(recipeClass)){
		unregister_chrdev_region(dev, 1);
		cdev_del(&recipe_dev);
	    pr_info("Failed to register device class\n");
	    return PTR_ERR(recipeClass);
	}
	pr_info("device class registered correctly\n");

	/* Create a device node named DEVICE_NAME associated a dev */
	recipeClass->dev_uevent = recipe_uevent;
	recipeDevice = device_create(recipeClass, NULL, dev, NULL, DEVICE_NAME);
	if (IS_ERR(recipeDevice)){
	    class_destroy(recipeClass);
	    cdev_del(&recipe_dev);
	    unregister_chrdev_region(dev, 1);
	    pr_info("Failed to create the device\n");
	    return PTR_ERR(recipeDevice);
	}
	pr_info("The device is created correctly\n");

	return 0;
}

static void __exit recipe_exit(void)
{
	device_destroy(recipeClass, dev);     /* remove the device */
	class_destroy(recipeClass);           /* remove the device class */
	cdev_del(&recipe_dev);
	unregister_chrdev_region(dev, 1);    /* unregister the device numbers */
	pr_info("recipe_exit exit\n");
}

module_init(recipe_init);
module_exit(recipe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a character & ioctl module ");

