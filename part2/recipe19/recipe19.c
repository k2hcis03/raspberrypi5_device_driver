/*
 * recipe19.c
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

#include <linux/i2c.h>
#include "ioctl.h"

#define DEVICE_NAME 		"recipedev"
#define DATA_CNT			201
#define POWERMODE			0x00			//Normal Mode

struct recipe19{
	struct platform_device *pdev;
	struct i2c_client * i2c_client;
	struct miscdevice recipe_miscdevice;
	struct task_struct *recipe_thread;
	struct completion recipe_complete_ok;
	struct ioctl_info info;
	int cnt;
};

int recipe_thread(void *priv)
{
	struct recipe19 * recipe_private = (struct recipe19 *)priv;
	struct device *dev = &recipe_private->i2c_client->dev;
	int i = 0;

	dev_info(dev, "recipe_thread() called\n");

	while(!kthread_should_stop()) {
		wait_for_completion_interruptible(&recipe_private->recipe_complete_ok);
		for(i = 0; i < DATA_CNT; i++){
			dev_info(dev, "DAC data is %X %d\n", POWERMODE |
					(char)(recipe_private->info.data[i] >> 8), recipe_private->info.data[i] & 0x00ff);
			i2c_smbus_write_byte_data(recipe_private->i2c_client, POWERMODE |
					(char)(recipe_private->info.data[i] >> 8), (char) (recipe_private->info.data[i] & 0x00ff));
			udelay(100);
		}
		reinit_completion(&recipe_private->recipe_complete_ok);
    }
    return 0;
}

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe19 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe19, recipe_miscdevice);
	dev = &recipe_private->i2c_client->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe19 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe19, recipe_miscdevice);
	dev = &recipe_private->i2c_client->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

static ssize_t recipe_write(struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
	struct recipe19 * recipe_private;
	struct device *dev;
	int i;

	recipe_private = container_of(filp->private_data, struct recipe19, recipe_miscdevice);
	dev = &recipe_private->i2c_client->dev;

	dev_info(dev, "recipe_write() is called.\n");

	if(copy_from_user(&recipe_private->info, (void __user *)buff, sizeof(recipe_private->info))) {
		dev_info(dev, "Bad copied value\n");
		return -EFAULT;
	}
	for(i = 0; i < DATA_CNT; i++){
		dev_info(dev, "%d\n", recipe_private->info.data[i]);
	}
	complete(&recipe_private->recipe_complete_ok);
	return count;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
	.write = recipe_write,
	.release = recipe_close,
};

static int recipe_probe(struct i2c_client * client, const struct i2c_device_id * id){
	int ret;
	struct device *dev = &client->dev;
	struct recipe19 *recipe_private;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(dev, sizeof(struct recipe19), GFP_KERNEL);
	if(!recipe_private){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->i2c_client = client;
	recipe_private->cnt = 0;

	init_completion(&recipe_private->recipe_complete_ok);

	recipe_private->recipe_thread = kthread_run(recipe_thread,recipe_private,"recipe Thread");
	if(recipe_private->recipe_thread){
		dev_info(dev, "Kthread Created Successfully.\n");
	} else {
		dev_err(dev, "Cannot create kthread\n");
		return -ENOMEM;
	}

	/* Attach the i2c device to the private structure */
	i2c_set_clientdata(client, recipe_private);

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
static void __exit recipe_remove(struct i2c_client * client)
{
	struct device *dev = &client->dev;
	struct recipe19 *recipe_private = i2c_get_clientdata(client);

	dev_info(dev, "recipe_remove() function is called.\n");
	complete(&recipe_private->recipe_complete_ok);
	kthread_stop(recipe_private->recipe_thread);
	dev_info(dev, "recipe_remove() function is called2.\n");

	complete(&recipe_private->recipe_complete_ok);
	misc_deregister(&recipe_private->recipe_miscdevice);
}

static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe19", },
	{ }
};
MODULE_DEVICE_TABLE(of, recipe_of_ids);

static const struct i2c_device_id recipe_id[] = {
	{ .name = "MCP4725", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, recipe_id);

static struct i2c_driver recipe_platform_driver = {
	.driver = {
		.name = "recipe19",
		.owner = THIS_MODULE,
		.of_match_table = recipe_of_ids,
	},
	.probe   = recipe_probe,
	.remove  = recipe_remove,
	.id_table	= recipe_id,
};
/* Register our platform driver */
module_i2c_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & i2c. module ");
