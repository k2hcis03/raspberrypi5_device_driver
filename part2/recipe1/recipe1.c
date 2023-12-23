/*
 * recipe1.c
 *
 *  Created on: 2021. 1. 18.
 *      Author: k2h
 */

#include <linux/module.h>

static int __init recipe_init(void)
{
	pr_info("Hello world init\n");
	return 0;
}

static void __exit recipe_exit(void)
{
	pr_info("Hello world exit\n");
}

module_init(recipe_init);
module_exit(recipe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kwang Hyuk Ko");
MODULE_DESCRIPTION("This is a simple init/exit module test");

