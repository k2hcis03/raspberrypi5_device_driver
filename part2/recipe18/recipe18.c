/*
 * recipe18.c
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

#include <linux/kthread.h>             //kernel threads
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/sched/signal.h>
#include "ioctl.h"

#define DEVICE_NAME 		"recipedev"
#define US_TO_NS(x) 		(x * 1000L)
#define MMAP_SIZE			512 * 4		//256 * 4 byte
#define MMAP_CNT			512
#define BUFSIZE				3

#define	SIGNUM				50
struct recipe18{
	struct platform_device *pdev;
	struct spi_device *spi;
	struct spi_message msg;
	struct spi_transfer transfer;
	struct miscdevice recipe_miscdevice;
	struct hrtimer recipe_hr_timer;
	int timer_period;
	struct task_struct *recipe_thread;
	struct completion recipe_complete_ok;
	struct task_struct *task;
	struct siginfo info;
	char *mmap_buf;
	int cnt;
};

int recipe_thread(void *priv)
{
	struct recipe18 * recipe_private = (struct recipe18 *)priv;
	struct device *dev = &recipe_private->spi->dev;
	int index = 0;
	int status;
	int start_bit = 1;
	int differential = 0;
	int channel = 0;
	int receive[2];

	dev_info(dev, "recipe_thread() called\n");

	while(!kthread_should_stop()) {
    	if(recipe_private->cnt == MMAP_CNT){
    		dev_info(dev, "data count is %d.\n", recipe_private->cnt);
    		recipe_private->cnt = 0;

    		if (send_sig_info(SIGNUM, (struct kernel_siginfo *) &recipe_private->info, 
				recipe_private->task) < 0){
				dev_info(dev, "Error sending signal");
			}
    	}else{
    		wait_for_completion_interruptible(&recipe_private->recipe_complete_ok);
			*(uint8_t *)(recipe_private->transfer.tx_buf) = ((start_bit << 6) | (!differential << 5) | (channel << 2));
			spi_message_init_with_transfers(&recipe_private->msg, &recipe_private->transfer, 1);
			
			status = spi_sync(recipe_private->spi, &recipe_private->msg);
			if (status < 0) {
				dev_err(dev, "SPI Read Error %d\n", status);
			}
  
			reinit_completion(&recipe_private->recipe_complete_ok);
			index = recipe_private->cnt++;

			receive[1] = *(uint8_t *)(recipe_private->transfer.rx_buf+1) << 4;
			receive[0] = *(uint8_t *)(recipe_private->transfer.rx_buf+2) >> 4;
			dev_info(dev, "recipe_thread is wakened from hrtimer %d and %d  %d\n", index,
					receive[1] << 4 , receive[2] >> 4);
			*((int *)(recipe_private->mmap_buf)+index) = receive[1] | receive[0];
    	}
    }
    return 0;
}

enum hrtimer_restart recipe_hr_timer(struct hrtimer *t)
{
	struct recipe18 *recipe_private = from_timer(recipe_private, t, recipe_hr_timer);
	struct device *dev;
	ktime_t ktime;

	dev = &recipe_private->spi->dev;
	complete(&recipe_private->recipe_complete_ok);

	ktime = ktime_set(0, US_TO_NS(recipe_private->timer_period));
	hrtimer_forward(&recipe_private->recipe_hr_timer,
		hrtimer_cb_get_time(&recipe_private->recipe_hr_timer), ktime);

	return HRTIMER_RESTART;
//	return HRTIMER_NORESTART;
}

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe18 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe18, recipe_miscdevice);
	dev = &recipe_private->spi->dev;
	recipe_private->task = get_current();

	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe18 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe18, recipe_miscdevice);
	dev = &recipe_private->spi->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

static int recipe_mmap(struct file *file, struct vm_area_struct *vma) {

	struct recipe18 * recipe_private;
	struct device *dev;

	recipe_private = container_of(file->private_data,  struct recipe18,
			recipe_miscdevice);
	dev = &recipe_private->spi->dev;

	dev_info(dev, "recipe_mmap() called\n");
	if(remap_pfn_range(vma, vma->vm_start, virt_to_phys(recipe_private->mmap_buf) >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)){
		return -EAGAIN;
	}
	return 0;
}

/* declare a ioctl_function */
static long recipe_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static struct ioctl_info info;
	struct recipe18 * recipe_private;
	struct device *dev;
	ktime_t ktime;
	int ret;

	recipe_private = container_of(file->private_data, struct recipe18, recipe_miscdevice);
	dev = &recipe_private->spi->dev;

	dev_info(dev, "recipe_dev_ioctl() is called. cmd = %d, arg = %ld\n", cmd, arg);

	switch (cmd) {
		case SET_DATA:
			dev_info(dev, "SET_DATA\n");
			if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
				return -EFAULT;
			}
			dev_info(dev, "User data is %d\n", info.data);
			if(info.data == 0){
				ret = hrtimer_cancel(&recipe_private->recipe_hr_timer);
				if(ret){
					dev_info(dev, "The timer was still in use.");
				}
			}else if(info.data == 1){
				recipe_private->cnt = 0;
				ktime = ktime_set( 0, US_TO_NS(recipe_private->timer_period));
				hrtimer_start(&recipe_private->recipe_hr_timer, ktime, HRTIMER_MODE_REL );
			}
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
	.unlocked_ioctl = recipe_ioctl,
	.mmap = recipe_mmap,
	.release = recipe_close,
};

static int recipe_probe(struct spi_device *spi)
{
	int ret;
	struct device *dev = &spi->dev;
	struct recipe18 *recipe_private;
	ktime_t ktime;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&spi->dev, sizeof(struct recipe18), GFP_KERNEL);
	if(!recipe_private){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	spi_setup(spi);
	
	memset(&recipe_private->info, 0, sizeof(recipe_private->info));
	recipe_private->info.si_signo = SIGNUM;
	recipe_private->info.si_code = SI_QUEUE;

	recipe_private->spi = spi;
	recipe_private->timer_period = 1000;		//1ms
	recipe_private->cnt = 0;
	recipe_private->transfer.tx_buf = devm_kzalloc(&spi->dev, BUFSIZE, GFP_KERNEL);
	recipe_private->transfer.len = BUFSIZE;
	recipe_private->transfer.rx_buf = devm_kzalloc(&spi->dev, BUFSIZE, GFP_KERNEL);

	recipe_private->mmap_buf = kzalloc(MMAP_SIZE, GFP_DMA);
	if(!recipe_private->mmap_buf){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	init_completion(&recipe_private->recipe_complete_ok);

	recipe_private->recipe_thread = kthread_run(recipe_thread,recipe_private,"recipe Thread");
	if(recipe_private->recipe_thread){
		dev_info(dev, "Kthread Created Successfully.\n");
	} else {
		dev_err(dev, "Cannot create kthread\n");
		return -ENOMEM;
	}

	ktime = ktime_set( 0, US_TO_NS(recipe_private->timer_period));
	hrtimer_init(&recipe_private->recipe_hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	recipe_private->recipe_hr_timer.function = &recipe_hr_timer;

	/* Attach the SPI device to the private structure */
	spi_set_drvdata(spi, recipe_private);

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
static void __exit recipe_remove(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct recipe18 *recipe_private = spi_get_drvdata(spi);
	int ret;

	dev_info(dev, "recipe_remove() function is called.\n");

	kfree(recipe_private->mmap_buf);
	kthread_stop(recipe_private->recipe_thread);
	complete(&recipe_private->recipe_complete_ok);

	ret = hrtimer_cancel(&recipe_private->recipe_hr_timer);
	if(ret){
		dev_info(dev, "The timer was still in use.");
	}
	misc_deregister(&recipe_private->recipe_miscdevice);
}

static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe18", },
	{ }
};
MODULE_DEVICE_TABLE(of, recipe_of_ids);

static const struct spi_device_id recipe_id[] = {
	{ .name = "recipe18"},
	{ },
};
MODULE_DEVICE_TABLE(spi, recipe_id);

static struct spi_driver recipe_platform_driver = {
	.driver = {
		.name = "recipe18",
		.owner = THIS_MODULE,
		.of_match_table = recipe_of_ids,
	},
	.probe   = recipe_probe,
	.remove  = recipe_remove,
	.id_table	= recipe_id,
};
/* Register our platform driver */
module_spi_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & spi. module ");
