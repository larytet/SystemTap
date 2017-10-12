/* -*- linux-c -*- 
 * Systemtap Test Module
 * Copyright (C) 2017 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/compiler.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

/*
 * The purpose of this module is to provide a memory location that
 * will be modified from user context via a /proc file.  Systemtap
 * scripts set kernel.data probes on the memory location and run tests
 * to see if the expected output is received. This is better than
 * using the kernel's own memory location, since we can't determine
 * when they will get read/written.
 */

/************ Below are the functions to create this module ************/

static struct proc_dir_entry *stm_ctl = NULL;

// The memory location to probe/watch.
int stap_hwbkpt_data = -1;

static ssize_t stm_write_cmd (struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	char type;

	if (get_user(type, (char __user *)buf))
		return -EFAULT;

	switch (type) {
	case '0':
	case '1':
	case '2':
		stap_hwbkpt_data = __INT_MAX__;
		break;
	default:
		printk ("stap_hwbkpt_kmod: invalid command type %d\n",
			(int)type);
		return -EINVAL;
	}
  
	return count;
}

static ssize_t stm_read_cmd(struct file *file, char __user *buffer,
			    size_t buflen, loff_t *fpos)
{
	size_t bytes = sizeof(stap_hwbkpt_data);

	if (buflen == 0 || *fpos >= bytes)
		return 0;

	bytes = min(bytes - (size_t)*fpos, buflen);
	if (copy_to_user(buffer, &stap_hwbkpt_data + *fpos, bytes))
		return -EFAULT;
	*fpos += bytes;
	return bytes;
}

static struct file_operations stm_fops_cmd = {
	.owner = THIS_MODULE,
	.write = stm_write_cmd,
	.read = stm_read_cmd,
};

#define CMD_FILE "stap_hwbkpt_cmd"

int init_module(void)
{
	stm_ctl = proc_create (CMD_FILE, 0666, NULL, &stm_fops_cmd);
	if (stm_ctl == NULL) 
		return -1;
	return 0;
}

void cleanup_module(void)
{
	if (stm_ctl)
		remove_proc_entry (CMD_FILE, NULL);
}

MODULE_DESCRIPTION("systemtap test module");
MODULE_LICENSE("GPL");
