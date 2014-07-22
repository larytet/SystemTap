/*
 * Copyright (C) 2014 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _LINUX_MMV_C_
#define _LINUX_MMV_C_

// FIXME: The code to create/delete the mmap file should be called
// from systemtap_kernel_module_init()/systemtap_kernel_module_exit(),
// which will require translator changes. For now I've hacked it into
// translate.cxx, but it probably needs a better place/mechanism.

// Defining STP_MMV turns on code in
// systemtap_kernel_module_init()/systemtap_kernel_module_exit() that
// calls _stp_mmv_init()/_stp_mmv_exit().
#define STP_MMV

#include "procfs.c"
#include <linux/mm.h>
#include "uidgid_compatibility.h"
#include "mmv.h"

/*
 * The following kernel commit, which first appeared in kernel 2.6.23,
 * added the 'fault' handler to vm_operations_struct.
 *
 * ====
 * commit 54cb8821de07f2ffcd28c380ce9b93d5784b40d7
 * Author: Nick Piggin <npiggin@suse.de>
 * Date:   Thu Jul 19 01:46:59 2007 -0700
 *
 *     mm: merge populate and nopage into fault (fixes nonlinear)
 * ====
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#error "Kernel must be at least 2.6.23 to use memory mapped values."
#endif

/* Note that this size gets rounded up to the nearest page. */
#ifndef STP_MMV_DATA_SIZE
/* Start with 8k. */
#define STP_MMV_DATA_SIZE (8 * 1024)
#endif

struct __stp_mmv_file_data {
	struct proc_dir_entry *proc_dentry; /* procfs dentry */
	atomic_t proc_attached;	       /* Has the file been opened? */
	void *data;		       /* data pointer */
	size_t nbytes;		       /* number of bytes */
};

static struct __stp_mmv_file_data _stp_mmv_file_data = {
	NULL, ATOMIC_INIT(0), NULL,
};

/*
 * _stp_mmv_fault is called the first time a memory area is
 * accessed which is not in memory. It does the actual mapping
 * between kernel and user space memory.
 */
static int _stp_mmv_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct __stp_mmv_file_data *mdata = vma->vm_private_data;
	pgoff_t offset = vmf->pgoff;
	loff_t size;

	printk(KERN_ERR "%s:%d - mapping %ld\n", __FUNCTION__,
	       __LINE__, vmf->pgoff);
	if (!mdata) {
		printk("no data\n");
		return VM_FAULT_SIGBUS;
	}

	if (offset > mdata->nbytes)
		return VM_FAULT_SIGBUS;

	/* Map the virtual page address to a physical page. */
	page = vmalloc_to_page(mdata->data + (vmf->pgoff * PAGE_SIZE));
	if (page == NULL)
		return VM_FAULT_SIGBUS;

	/* Increment the reference count of this page. Note that the
	 * kernel automatically decrements it. */
	get_page(page);
	vmf->page = page;
	return VM_FAULT_MAJOR;
}

struct vm_operations_struct _stp_mmv_vm_ops = {
	.fault = _stp_mmv_fault,
};

static int _stp_mmv_mmap_fops_cmd(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &_stp_mmv_vm_ops;
#ifdef VM_DONTDUMP
	vma->vm_flags |= VM_DONTCOPY|VM_DONTEXPAND|VM_DONTDUMP;
#else
	vma->vm_flags |= VM_DONTCOPY|VM_DONTEXPAND;
#endif
	/* assign the file private data to the vm private data */
	vma->vm_private_data = filp->private_data;
	return 0;
}

static int _stp_mmv_open_fops_cmd(struct inode *inode, struct file *file)
{
	struct __stp_mmv_file_data *data = PDE_DATA(inode);

	/* Notice we enforce exclusive opens - only one open at a time. */
	if (atomic_inc_return (&data->proc_attached) > 1) {
		atomic_dec (&data->proc_attached);
		return -EBUSY;
	}
	file->private_data = data;

	// Update file size.
	if (i_size_read(inode) != data->nbytes) {
		mutex_lock(&inode->i_mutex);
		i_size_write(inode, data->nbytes);
		mutex_unlock(&inode->i_mutex);
	}
	return 0;
}

static int _stp_mmv_close_fops_cmd(struct inode *inode, struct file *file)
{
	struct __stp_mmv_file_data *data = file->private_data;
	if (atomic_dec_return (&data->proc_attached) > 0) {
		BUG();
		return -EINVAL;
	}
	return 0;
}

static struct file_operations _stp_mmv_fops = {
	.owner = THIS_MODULE,
	.open = _stp_mmv_open_fops_cmd,
	.release = _stp_mmv_close_fops_cmd,
	.mmap = _stp_mmv_mmap_fops_cmd,
};

// FIXME: One of the things I hate about procfs is that it isn't a
// true filesystem. We're creating a file called
// '/proc/systemtap/{module_name}/mmv' here, but there isn't anything
// at the procfs layer that would prevent us from creating a
// procfs probe file with the same name. How to handle?

static int
_stp_mmv_create_file(void)
{
	_stp_mkdir_proc_module();
	printk(KERN_ERR "%s:%d - entry\n", __FUNCTION__, __LINE__);
	if (_stp_proc_root == NULL) {
		//_stp_error("Could not create procfs base directory\n");
		printk(KERN_ERR "%s:%d - error exit\n", __FUNCTION__, __LINE__);
		return -1;
	}
	_stp_mmv_file_data.proc_dentry = proc_create_data("mmv", 0600,
							  _stp_proc_root,
							  &_stp_mmv_fops,
							  &_stp_mmv_file_data);
	if (_stp_mmv_file_data.proc_dentry == NULL) {
		printk(KERN_ERR "%s:%d - error exit\n", __FUNCTION__, __LINE__);
		_stp_error("Could not create procfs file \"mmv\"\n");
		return -1;
	}
#ifdef STAPCONF_PROCFS_OWNER
	_stp_mmv_file_data.proc_dentry->owner = THIS_MODULE;
#endif
	proc_set_user(_stp_mmv_file_data.proc_dentry, KUIDT_INIT(_stp_uid),
		      KGIDT_INIT(_stp_gid));
	printk(KERN_ERR "%s:%d - exit\n", __FUNCTION__, __LINE__);
	return 0;
}

static void _stp_mmv_delete_file(void)
{
	if (_stp_mmv_file_data.proc_dentry) {
		proc_remove(_stp_mmv_file_data.proc_dentry);
		_stp_mmv_file_data.proc_dentry = NULL;
	}
#if (STP_TRANSPORT_VERSION != 1)
	// If we're using the original transport, it uses the
	// '/proc/systemtap/{module_name}' directory to store control
	// files. Let the transport layer clean up that directory.
	_stp_rmdir_proc_module();
#endif
}

static void _stp_mmv_exit(void);

// Called from systemtap_kernel_module_init()
static int _stp_mmv_init(void)
{
	printk(KERN_ERR "%s:%d - entry\n", __FUNCTION__, __LINE__);

	// We want to round up STP_MMV_DATA_SIZE to the next page
	// boundry since _stp_vzalloc() only allocates in multiples of
	// PAGE_SIZE. So, we convert STP_MMV_DATA_SIZE into a number
	// of pages, then multiply that by PAGE_SIZE.
	_stp_mmv_file_data.nbytes = (((STP_MMV_DATA_SIZE + PAGE_SIZE -1)/PAGE_SIZE) * PAGE_SIZE);

	// Allocate space.
	printk(KERN_ERR "%s:%d - allocating %u bytes\n",
	       __FUNCTION__, __LINE__, (unsigned int)_stp_mmv_file_data.nbytes);
	_stp_mmv_file_data.data = _stp_vzalloc(_stp_mmv_file_data.nbytes);
	if (_stp_mmv_file_data.data == NULL)
		return -ENOMEM;

	// Now that we've got memory to map, create the memory map
	// file.
	if (_stp_mmv_create_file() != 0)
		goto err;

	_stp_mmv_data_init(_stp_mmv_file_data.data, _stp_mmv_file_data.nbytes);
	printk(KERN_ERR "%s:%d - exit\n", __FUNCTION__, __LINE__);
	return 0;

err:
	_stp_mmv_exit();
	return -ENOMEM;
}

// Called from systemtap_kernel_module_exit()
static void _stp_mmv_exit(void)
{
	size_t i;
	printk(KERN_ERR "%s:%d - entry\n", __FUNCTION__, __LINE__);

	_stp_mmv_delete_file();
	if (_stp_mmv_file_data.data) {
		_stp_vfree(_stp_mmv_file_data.data);
		_stp_mmv_file_data.data = NULL;
	}
	printk(KERN_ERR "%s:%d - exit\n", __FUNCTION__, __LINE__);
}

#endif /* _LINUX_MMV_C_ */
