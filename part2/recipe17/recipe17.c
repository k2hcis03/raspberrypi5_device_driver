/*
 * recipe17.c
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

#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/kthread.h>             //kernel threads

#define DEVICE_NAME 		"recipedev"
#define US_TO_NS(x) 		(x * 1000L)
#define DMA_SIZE			4096
struct recipe16{
	struct platform_device *pdev;
	struct miscdevice recipe_miscdevice;
	struct hrtimer recipe_hr_timer;
	int timer_period;
	struct task_struct *recipe_thread;
	struct dma_chan *recipe_dma_m2m_chan;
	wait_queue_head_t wait_queue;
	char *dma_src_buffer;
	char *dma_dst_buffer;
	int dma_cnt;
	int flag;
};

static void recipe_dma_m2m_callback(void *priv)
{
	struct recipe16 * recipe_private = (struct recipe16 *)priv;
	struct device *dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dma_m2m_callback() called\n");
	
	recipe_private->flag = 1;
	wake_up_interruptible(&recipe_private->wait_queue);
	if (*(recipe_private->dma_src_buffer) != *(recipe_private->dma_dst_buffer)) {
		dev_err(dev, "buffer copy failed!\n");
	}
	dev_info(dev, "buffer copy passed!\n");
}

int recipe_thread_(void *priv)
{
	struct recipe16 * recipe_private = (struct recipe16 *)priv;
	struct device *dev = &recipe_private->pdev->dev;
	int ret;
	struct dma_device *dma_dev;
	struct dma_async_tx_descriptor *dma_m2m_desc;
	dma_cookie_t cookie;
	dma_addr_t dma_src;
	dma_addr_t dma_dst;

	dev_info(dev, "recipe_thread() called\n");
    while(!kthread_should_stop()) {
    	ret = wait_event_interruptible(recipe_private->wait_queue,
    			recipe_private->dma_cnt == DMA_SIZE-1);
    	if(ret){
    		return ret;
    	}
    	recipe_private->dma_cnt = 0;
    	dev_info(dev, "recipe_thread() is wakened from hrtimer\n");
    	dma_dev = recipe_private->recipe_dma_m2m_chan->device;
    	dma_src = dma_map_single(dev, recipe_private->dma_src_buffer,
    								 DMA_SIZE, DMA_TO_DEVICE);
    	dma_dst = dma_map_single(dev, recipe_private->dma_dst_buffer,
    								 DMA_SIZE, DMA_FROM_DEVICE);
    	dev_info(dev, "dma src&dst map obtained");
    	dma_m2m_desc = dma_dev->device_prep_dma_memcpy(recipe_private->recipe_dma_m2m_chan,
    								dma_dst,
    								dma_src,
									DMA_SIZE,
    								DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
    	dma_m2m_desc->callback = recipe_dma_m2m_callback;
		dma_m2m_desc->callback_param = recipe_private;
		cookie = dmaengine_submit(dma_m2m_desc);
		if (dma_submit_error(cookie)){
			dev_err(dev, "Failed to submit DMA\n");
			return -EINVAL;
		};
		dma_async_issue_pending(recipe_private->recipe_dma_m2m_chan);

    	ret = wait_event_interruptible(recipe_private->wait_queue,
    			recipe_private->flag != 0);
    	if(ret){
    		return ret;
    	}
    	recipe_private->flag = 0;
    	dev_info(dev, "recipe_thread() is wakened from dma callback\n");
    	dma_async_is_tx_complete(recipe_private->recipe_dma_m2m_chan, cookie, NULL, NULL);
		dma_unmap_single(dev, dma_src, DMA_SIZE, DMA_TO_DEVICE);
		dma_unmap_single(dev, dma_dst, DMA_SIZE, DMA_FROM_DEVICE);
    }
	ret = hrtimer_cancel(&recipe_private->recipe_hr_timer);
	if(ret){
		dev_info(dev, "The timer was still in use.");
	}
    return 0;
}

enum hrtimer_restart recipe_hr_timer(struct hrtimer *t)
{
	int index = 0;

	struct recipe16 *recipe_private = from_timer(recipe_private, t, recipe_hr_timer);
	struct device *dev;
	ktime_t ktime;

	dev = &recipe_private->pdev->dev;

	index = recipe_private->dma_cnt++;
	recipe_private->dma_src_buffer[index] = index;
	if(recipe_private->dma_cnt == DMA_SIZE-1){
		wake_up_interruptible(&recipe_private->wait_queue);
		dev_info(dev, "recipe12_hr_timer completed.\n");

		return HRTIMER_NORESTART;
	}
	ktime = ktime_set(0, MS_TO_NS(recipe_private->timer_period));
	hrtimer_forward(&recipe_private->recipe_hr_timer,
			hrtimer_cb_get_time(&recipe_private->recipe_hr_timer), ktime);

	return HRTIMER_RESTART;
//	return HRTIMER_NORESTART;
}

static int recipe_open(struct inode *inode, struct file *file)
{
	struct recipe16 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe16, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;
	dev_info(dev, "recipe_dev_open() is called.\n");
	return 0;
}

static int recipe_close(struct inode *inode, struct file *file)
{
	struct recipe16 * recipe_private;
	struct device *dev;
	recipe_private = container_of(file->private_data, struct recipe16, recipe_miscdevice);
	dev = &recipe_private->pdev->dev;

	dev_info(dev, "recipe_dev_close() is called.\n");
	return 0;
}

static const struct file_operations recipe_fops = {
	.owner = THIS_MODULE,
	.open = recipe_open,
	.release = recipe_close,
};

static int recipe_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct recipe16 *recipe_private;
	ktime_t ktime;
	dma_cap_mask_t dma_m2m_mask;

	dev_info(dev, "recipe_probe() function is called.\n");

	recipe_private = devm_kzalloc(&pdev->dev, sizeof(struct recipe16), GFP_KERNEL);
	if(!recipe_private){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->pdev = pdev;
	recipe_private->timer_period = 1;
	recipe_private->dma_cnt = 0;
	recipe_private->flag = 0;
	recipe_private->dma_src_buffer = devm_kzalloc(&pdev->dev, DMA_SIZE, GFP_KERNEL);
	
	if(!recipe_private->dma_src_buffer){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	recipe_private->dma_dst_buffer = devm_kzalloc(&pdev->dev, DMA_SIZE, GFP_KERNEL);
	
	if(!recipe_private->dma_dst_buffer){
		dev_err(dev, "failed memory allocation");
		return -ENOMEM;
	}
	init_waitqueue_head(&recipe_private->wait_queue);
	dma_cap_zero(dma_m2m_mask);
	dma_cap_set(DMA_MEMCPY | DMA_SLAVE, dma_m2m_mask);
	recipe_private->recipe_dma_m2m_chan = dma_request_channel(dma_m2m_mask, NULL, NULL);
	
	if (!recipe_private->recipe_dma_m2m_chan) {
		dev_err(&pdev->dev, "Error DMA memory to memory channel\n");
		return -EINVAL;
	}
	recipe_private->recipe_thread = kthread_run(recipe_thread_,recipe_private,"recipe Thread");
	
	if(recipe_private->recipe_thread){
		dev_info(dev, "Kthread Created Successfully.\n");
	} else {
		dev_err(dev, "Cannot create kthread\n");
		return -ENOMEM;
	}

	ktime = ktime_set( 0, MS_TO_NS(recipe_private->timer_period));
	hrtimer_init(&recipe_private->recipe_hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	recipe_private->recipe_hr_timer.function = &recipe_hr_timer;
	hrtimer_start(&recipe_private->recipe_hr_timer, ktime, HRTIMER_MODE_REL );

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
	struct recipe16 *recipe_private = platform_get_drvdata(pdev);
	int ret;

	dev_info(dev, "recipe_remove() function is called.\n");
	ret = hrtimer_cancel(&recipe_private->recipe_hr_timer);
	
	if(ret){
		dev_info(dev, "The timer was still in use.");
	}

	kthread_stop(recipe_private->recipe_thread);
	dma_release_channel(recipe_private->recipe_dma_m2m_chan);
	dev_info(dev, "kthread_stop\n");
	misc_deregister(&recipe_private->recipe_miscdevice);
	return 0;
}

/* Declare a list of devices supported by the driver */
static const struct of_device_id recipe_of_ids[] = {
	{ .compatible = "brcm,recipe17"},
	{},
};

MODULE_DEVICE_TABLE(of, recipe_of_ids);

/* Define platform driver structure */
static struct platform_driver recipe_platform_driver = {
	.probe = recipe_probe,
	.remove = recipe_remove,
	.driver = {
		.name = "recipe17",
		.of_match_table = recipe_of_ids,
		.owner = THIS_MODULE,
	}
};

/* Register our platform driver */
module_platform_driver(recipe_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a platform & async. module ");
