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

#ifndef STP_MMV_MAX_INDOMS
#define STP_MMV_MAX_INDOMS 2
#endif

#ifndef STP_MMV_MAX_INSTANCES
#define STP_MMV_MAX_INSTANCES 5
#endif

#ifndef STP_MMV_MAX_METRICS
#define STP_MMV_MAX_METRICS 10
#endif

#define STP_MMV_MAX_VALUES (STP_MMV_MAX_METRICS * STP_MMV_MAX_INSTANCES)


//
// Storage for global instances, indoms, and metrics.
//
// FIXME: For now, no locking.
//
static atomic_t __stp_mmv_instance_idx = ATOMIC_INIT(-1);
static atomic_t __stp_mmv_indom_idx = ATOMIC_INIT(-1);
static atomic_t __stp_mmv_metric_idx = ATOMIC_INIT(-1);
static atomic_t __stp_mmv_value_idx = ATOMIC_INIT(-1);
static atomic_t __stp_mmv_string_idx = ATOMIC_INIT(-1);
static int __stp_mmv_nstrings = 0;

static struct proc_dir_entry *_stp_mmv_proc_dentry = NULL;
static atomic_t _stp_mmv_attached = ATOMIC_INIT(0);

#define _STP_MMV_DISK_HEADER	0
#define _STP_MMV_DISK_TOC	1
#define _STP_MMV_DISK_INDOMS	2
#define _STP_MMV_DISK_INSTANCES 3
#define _STP_MMV_DISK_METRICS	4
#define _STP_MMV_DISK_VALUES	5
#define _STP_MMV_DISK_STRINGS	6

#define _STP_MMV_MAX_SECTIONS	7

struct __stp_mmv_data {
	void *data;			/* data pointer */
	size_t size;
	size_t offset[_STP_MMV_MAX_SECTIONS];
	void *ptr[_STP_MMV_MAX_SECTIONS];
};

/*
 * _stp_mmv_fault is called the first time a memory area is
 * accessed which is not in memory. It does the actual mapping
 * between kernel and user space memory.
 */
static int _stp_mmv_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct __stp_mmv_data *mdata = vma->vm_private_data;
	pgoff_t offset = vmf->pgoff;
	loff_t size;

	printk(KERN_ERR "%s:%d - mapping %ld\n", __FUNCTION__,
	       __LINE__, vmf->pgoff);
	if (!mdata) {
		printk("no data\n");
		return VM_FAULT_SIGBUS;
	}

	if (offset > mdata->size)
		return VM_FAULT_SIGBUS;

	/* Map the virtual page address to a physical page. */
	page = vmalloc_to_page(mdata->data + (vmf->pgoff * PAGE_SIZE));
	if (page == NULL)
		return VM_FAULT_SIGBUS;

	/* Increment the reference count of this page. Note that the
	 * kernel automatically decrements it. */
	get_page(page);
	vmf->page = page;
	return VM_FAULT_MAJOR | VM_FAULT_LOCKED;
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
	struct __stp_mmv_data *data = PDE_DATA(inode);
	if (atomic_inc_return (&_stp_mmv_attached) > 1) {
		atomic_dec (&_stp_mmv_attached);
		return -EBUSY;
	}
	file->private_data = data;

	// Update file size.
	if (i_size_read(inode) != data->size) {
		mutex_lock(&inode->i_mutex);
		i_size_write(inode, data->size);
		mutex_unlock(&inode->i_mutex);
	}
	return 0;
}

static int _stp_mmv_close_fops_cmd(struct inode *inode, struct file *file)
{
	if (atomic_dec_return (&_stp_mmv_attached) > 0) {
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
_stp_mmv_create_file(void *data)
{
	_stp_mkdir_proc_module();
	printk(KERN_ERR "%s:%d - entry\n", __FUNCTION__, __LINE__);
	if (_stp_proc_root == NULL) {
		//_stp_error("Could not create procfs base directory\n");
		printk(KERN_ERR "%s:%d - error exit\n", __FUNCTION__, __LINE__);
		return -1;
	}
	_stp_mmv_proc_dentry = proc_create_data("mmv", 0600, _stp_proc_root,
						&_stp_mmv_fops, data);
	if (_stp_mmv_proc_dentry == NULL) {
		printk(KERN_ERR "%s:%d - error exit\n", __FUNCTION__, __LINE__);
		_stp_error("Could not create procfs file \"mmv\"\n");
		return -1;
	}
#ifdef STAPCONF_PROCFS_OWNER
	_stp_mmv_proc_dentry->owner = THIS_MODULE;
#endif
	proc_set_user(_stp_mmv_proc_dentry, KUIDT_INIT(_stp_uid),
		      KGIDT_INIT(_stp_gid));
	printk(KERN_ERR "%s:%d - exit\n", __FUNCTION__, __LINE__);
	return 0;
}

static void _stp_mmv_delete_file(void)
{
	if (_stp_mmv_proc_dentry) {
		proc_remove(_stp_mmv_proc_dentry);
		_stp_mmv_proc_dentry = NULL;
	}
#if (STP_TRANSPORT_VERSION != 1)
	// If we're using the original transport, it uses the
	// '/proc/systemtap/{module_name}' directory to store control
	// files. Let the transport layer clean up that directory.
	_stp_rmdir_proc_module();
#endif
}

static struct __stp_mmv_data _stp_mmv_data = { NULL, 0 };

static void _stp_mmv_exit(void);

// Called from systemtap_kernel_module_init()
static int _stp_mmv_init(void)
{
	size_t i;
	int32_t ntocs;

	printk(KERN_ERR "%s:%d - entry\n", __FUNCTION__, __LINE__);

	// We could think about allocating a __stp_mmv_data structure
	// here, instead of having a static one. But, right now we'll
	// only ever have one of these files.
	_stp_mmv_data.size = 0;
	_stp_mmv_data.offset[_STP_MMV_DISK_HEADER] = _stp_mmv_data.size;
	_stp_mmv_data.size += sizeof(mmv_disk_header_t);

	// TOC (Table Of Contents) section. Always assume a TOC for
	// metrics, values, and strings.
	ntocs = 3;
	// Sigh. This happens too early for the user to have specified
	// any indoms yet, so we have to assume the max.
	if (STP_MMV_MAX_INDOMS > 0) {
		ntocs += 2;
		// shorttext/helptext
		__stp_mmv_nstrings += (2 * STP_MMV_MAX_INDOMS);
	}
	_stp_mmv_data.offset[_STP_MMV_DISK_TOC] = _stp_mmv_data.size;
	_stp_mmv_data.size += (sizeof(mmv_disk_toc_t) * ntocs);

	// Indoms section
	//
	// This happens too early for the user to have added any
	// indoms yet, so we have to assume the max.
	if (STP_MMV_MAX_INDOMS > 0) {
		// shorttext/helptext
		__stp_mmv_nstrings += (2 * STP_MMV_MAX_INDOMS);
		_stp_mmv_data.offset[_STP_MMV_DISK_INDOMS] = _stp_mmv_data.size;
		_stp_mmv_data.size += sizeof(mmv_disk_indom_t) * STP_MMV_MAX_INDOMS;
	}

	// Instances section
	//
	// Here once again this happens too early for the user to have
	// added any instances yet, so we have to assume the max.
	if (STP_MMV_MAX_INSTANCES > 0) {
		_stp_mmv_data.offset[_STP_MMV_DISK_INSTANCES] = _stp_mmv_data.size;
		_stp_mmv_data.size += sizeof(mmv_disk_instance_t) * STP_MMV_MAX_INSTANCES;
	}

	// Metrics section
	//
	// Here once again this happens too early for the user to have
	// added any metrics yet, so we have to assume the max.
	if (STP_MMV_MAX_METRICS > 0) {
		// shorttext/helptext
		__stp_mmv_nstrings += (2 * STP_MMV_MAX_METRICS);
		_stp_mmv_data.offset[_STP_MMV_DISK_METRICS] = _stp_mmv_data.size;
		_stp_mmv_data.size += sizeof(mmv_disk_metric_t) * STP_MMV_MAX_METRICS;
	}

	// Values section
	if (STP_MMV_MAX_VALUES > 0) {
		// We have to assume that all the values are string
		// values, so increase the number of strings...
		__stp_mmv_nstrings += STP_MMV_MAX_VALUES;
		_stp_mmv_data.offset[_STP_MMV_DISK_VALUES] = _stp_mmv_data.size;
		_stp_mmv_data.size += sizeof(mmv_disk_value_t) * STP_MMV_MAX_VALUES;
	}

	// Strings section.
	_stp_mmv_data.offset[_STP_MMV_DISK_STRINGS] = _stp_mmv_data.size;
	_stp_mmv_data.size += sizeof(mmv_disk_string_t) * __stp_mmv_nstrings;

	// Allocate space.
	printk(KERN_ERR "%s:%d - allocating %u bytes\n",
	       __FUNCTION__, __LINE__, (unsigned int)_stp_mmv_data.size);
	_stp_mmv_data.data = _stp_vzalloc(_stp_mmv_data.size);
	if (_stp_mmv_data.data == NULL)
		return -ENOMEM;

	// Create pointers from the values we stored when determining
	// how many bytes we need.
	for (i = 0; i < _STP_MMV_MAX_SECTIONS; i++) {
		_stp_mmv_data.ptr[i] = _stp_mmv_data.data
			+ _stp_mmv_data.offset[i];
	}

	// Now that we've got memory to map, create the memory map
	// file.
	if (_stp_mmv_create_file(&_stp_mmv_data) != 0)
		goto err;

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
	if (_stp_mmv_data.data) {
		_stp_vfree(_stp_mmv_data.data);
		_stp_mmv_data.data = NULL;
	}
	printk(KERN_ERR "%s:%d - exit\n", __FUNCTION__, __LINE__);
}


static int
__stp_mmv_add_instance(int32_t internal, char *external)
{
	mmv_disk_instance_t *instance = (mmv_disk_instance_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_INSTANCES];
	int idx = atomic_add_return(1, &__stp_mmv_instance_idx);
	if (idx > STP_MMV_MAX_INSTANCES) {
		// Error message provided by caller.
		return -1;
	}

	// Fill in 'instance[idx].indom' later when the instance gets
	// added to the indom.
	instance[idx].padding = 0;
	instance[idx].internal = internal;
	strncpy (instance[idx].external, external, MMV_NAMEMAX);
	return idx;
}

static int
__stp_mmv_add_string(char *str)
{
	mmv_disk_string_t *string = (mmv_disk_string_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_STRINGS];
	int offset = 0;
	int stridx;

	if (str != NULL && strlen(str)) {
		stridx = atomic_add_return(1, &__stp_mmv_string_idx);
		if (stridx > __stp_mmv_nstrings) {
			// Error message provided by caller.
			return -3;
		}
		offset = _stp_mmv_data.offset[_STP_MMV_DISK_STRINGS]
			+ (stridx * sizeof(mmv_disk_string_t));
		strncpy(string[stridx].payload, str, MMV_STRINGMAX);
		string[stridx].payload[MMV_STRINGMAX-1] = '\0';
	}
	return offset;
}

static int
__stp_mmv_add_indom(uint32_t serial, char *shorttext, char *helptext)
{
	mmv_disk_indom_t *indom = (mmv_disk_indom_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_INDOMS];
	int idx = atomic_add_return(1, &__stp_mmv_indom_idx);

	if (idx > STP_MMV_MAX_INDOMS) {
		// Error message provided by caller.
		return -1;
	}
	indom[idx].serial = serial;
	indom[idx].count = 0;
	indom[idx].offset = 0;
	indom[idx].shorttext = __stp_mmv_add_string(shorttext);
	indom[idx].helptext = __stp_mmv_add_string(helptext);
	return idx;
}

// FIXME: I think we have a problem here. All the instances added to
// an indom must be consecutive. We'll either have to error if they
// are not or rearrange things behind the scene.

static int
__stp_mmv_add_indom_instance(int indom_idx, int instance_idx)
{
	mmv_disk_indom_t *indom = (mmv_disk_indom_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_INDOMS];
	mmv_disk_instance_t *instance = (mmv_disk_instance_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_INSTANCES];
	int nindoms = atomic_read(&__stp_mmv_indom_idx) + 1;
	int ninstances = atomic_read(&__stp_mmv_instance_idx) + 1;
	uint64_t instance_offset;

	if (indom_idx > nindoms || instance_idx > ninstances) {
		// Error message provided by caller.
		return -1;
	}

	indom[indom_idx].count++;
	instance_offset = _stp_mmv_data.offset[_STP_MMV_DISK_INSTANCES]
		+ (instance_idx * sizeof(mmv_disk_instance_t));
	if (indom[indom_idx].offset == 0
	    || indom[indom_idx].offset > instance_offset)
		indom[indom_idx].offset = instance_offset;
	instance[instance_idx].indom = _stp_mmv_data.offset[_STP_MMV_DISK_INDOMS]
		+ (indom_idx * sizeof(mmv_disk_indom_t));
		
	return 0;
}

static mmv_disk_indom_t *
__stp_mmv_lookup_disk_indom(int serial)
{
	mmv_disk_indom_t *indom = (mmv_disk_indom_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_INDOMS];
	int nindoms = atomic_read(&__stp_mmv_indom_idx) + 1;
	int i;

	for (i = 0; i < nindoms; i++) {
		if (indom[i].serial == serial)
		   return &indom[i];
	}
	return NULL;
}

static int
__stp_mmv_add_metric(char *name, uint32_t item, mmv_metric_type_t type,
		     mmv_metric_sem_t semantics, mmv_units_t dimension,
		     uint32_t indom_serial, char *shorttext, char *helptext)
{
	mmv_disk_metric_t *metric = (mmv_disk_metric_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_METRICS];
	mmv_disk_value_t *value = (mmv_disk_value_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_VALUES];
	int idx = atomic_add_return(1, &__stp_mmv_metric_idx);
	int value_idx;
	uint64_t metric_offset;

	if (idx > STP_MMV_MAX_METRICS) {
		// Error message provided by caller.
		return -1;
	}

	strncpy(metric[idx].name, name, MMV_NAMEMAX);
	metric[idx].item = item;
	metric[idx].type = type;
	metric[idx].semantics = semantics;
	metric[idx].dimension = dimension;
	metric[idx].indom = indom_serial;
	metric[idx].shorttext = __stp_mmv_add_string(shorttext);
	metric[idx].helptext = __stp_mmv_add_string(helptext);

	metric_offset = _stp_mmv_data.offset[_STP_MMV_DISK_METRICS]
		+ (idx * sizeof(mmv_disk_metric_t));
	if (metric[idx].indom == 0) {
		value_idx = atomic_add_return(1, &__stp_mmv_value_idx);
		if (value_idx > STP_MMV_MAX_VALUES) {
			// Error message provided by caller.
			return -4;
		}
		value[value_idx].metric = metric_offset;
	}
	else {
		mmv_disk_indom_t *indom = __stp_mmv_lookup_disk_indom(indom_serial);
		int i;
		
		if (indom == NULL) {
			// Error message provided by caller.
			return -5;
		}
		
		for (i = 0; i < indom->count; i++) {
			value_idx = atomic_add_return(1, &__stp_mmv_value_idx);
			if (value_idx > STP_MMV_MAX_VALUES) {
				// Error message provided by caller.
				return -4;
			}
			value[value_idx].metric = metric_offset;
			value[value_idx].instance = indom->offset +
				(sizeof(mmv_disk_instance_t) * i);
		}
	}
	return idx;
}

static int
__stp_mmv_stats_init(int cluster, mmv_stats_flags_t flags)
{
	mmv_disk_header_t *hdr = (mmv_disk_header_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_HEADER];
	mmv_disk_toc_t *toc = (mmv_disk_toc_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_TOC];
	int tocidx = 0;
	int nindoms = atomic_read(&__stp_mmv_indom_idx) + 1;

	strcpy(hdr->magic, "MMV");
	hdr->version = 1;
	hdr->g1 = _stp_random_u(999999);
	hdr->g2 = 0;
	hdr->tocs = 2;
	if (nindoms > 0)
		hdr->tocs += 2;
	if (__stp_mmv_nstrings)
		hdr->tocs++;

	hdr->flags = flags;
	hdr->process = 0; //?
	hdr->cluster = cluster;

	if (nindoms) {
		toc[tocidx].type = MMV_TOC_INDOMS;
		toc[tocidx].count = nindoms;
		toc[tocidx].offset = _stp_mmv_data.offset[_STP_MMV_DISK_INDOMS];
		tocidx++;
		toc[tocidx].type = MMV_TOC_INSTANCES;
		toc[tocidx].count = atomic_read(&__stp_mmv_instance_idx) + 1;
		toc[tocidx].offset = _stp_mmv_data.offset[_STP_MMV_DISK_INSTANCES];
		tocidx++;
	}
	toc[tocidx].type = MMV_TOC_METRICS;
	toc[tocidx].count = atomic_read(&__stp_mmv_metric_idx) + 1;
	toc[tocidx].offset = _stp_mmv_data.offset[_STP_MMV_DISK_METRICS];
	tocidx++;
	toc[tocidx].type = MMV_TOC_VALUES;
	toc[tocidx].count = atomic_read(&__stp_mmv_value_idx) + 1;
	toc[tocidx].offset = _stp_mmv_data.offset[_STP_MMV_DISK_VALUES];
	tocidx++;
	if (__stp_mmv_nstrings) {
		toc[tocidx].type = MMV_TOC_STRINGS;
		toc[tocidx].count = atomic_read(&__stp_mmv_string_idx) + 1;
		toc[tocidx].offset = _stp_mmv_data.offset[_STP_MMV_DISK_STRINGS];
		tocidx++;
	}

	// Setting g2 to the same value as g1 lets clients know that
	// we are finished initializing.
	hdr->g2 = hdr->g1;
	return 0;
}

static int
__stp_mmv_stats_stop(void)
{
	return 0;
}

static int
__stp_mmv_lookup_value(int metric_idx, int instance_idx)
{
	mmv_disk_value_t *value = (mmv_disk_value_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_VALUES];
	mmv_disk_metric_t *metric = (mmv_disk_metric_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_METRICS];
	uint64_t metric_offset = _stp_mmv_data.offset[_STP_MMV_DISK_METRICS]
		+ (sizeof(mmv_disk_metric_t) * metric_idx);
	uint64_t instance_offset;
	int i;

	if (metric_idx < 0 || metric_idx > atomic_read(&__stp_mmv_metric_idx))
		return -1;
	if (instance_idx != 0 && (instance_idx < 0
				  || instance_idx > atomic_read(&__stp_mmv_instance_idx)))
		return -1;
	instance_offset = _stp_mmv_data.offset[_STP_MMV_DISK_INSTANCES]
		+ (sizeof(mmv_disk_instance_t) * instance_idx);
	for (i = 0; i < (atomic_read(&__stp_mmv_value_idx) +1); i++) {
		if (value[i].metric == metric_offset) {
			/* Singular metric */
			if (metric[metric_idx].indom == 0) {
				return i;
			}
			else if (value[i].instance == instance_offset) {
				return i;
			}
		}
	}
	return -1;
}

static int
__stp_mmv_set_value(int value_idx, uint64_t value)
{
	mmv_disk_value_t *v = (mmv_disk_value_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_VALUES];
	mmv_disk_metric_t *m;

	if (value_idx < 0 || value_idx > atomic_read(&__stp_mmv_metric_idx)) {
		return -1;
	}

	m = (mmv_disk_metric_t *)((char *)_stp_mmv_data.data + v[value_idx].metric);
	if (m->type != MMV_TYPE_I64) {
		return -2;
	}
	v[value_idx].value.ll = value;
	return 0;
}

static int
__stp_mmv_inc_value(int value_idx, uint64_t inc)
{
	mmv_disk_value_t *v = (mmv_disk_value_t *)_stp_mmv_data.ptr[_STP_MMV_DISK_VALUES];
	mmv_disk_metric_t *m;

	if (value_idx < 0 || value_idx > atomic_read(&__stp_mmv_metric_idx)) {
		return -1;
	}

	m = (mmv_disk_metric_t *)((char *)_stp_mmv_data.data + v[value_idx].metric);
	if (m->type != MMV_TYPE_I64) {
		return -2;
	}
	v[value_idx].value.ll += inc;
	return 0;
}

#endif /* _LINUX_MMV_C_ */
