// FIXME: We should eventually split mmv.c into mmv.c/mmv.h so that
// dyninst can share the .h file (and perhaps user programs could also
// use the .h file).
//
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
//#include "../uidgid_compatibility.h"

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

typedef enum mmv_stats_flags {
    MMV_FLAG_NOPREFIX	= 0x1,	/* Don't prefix metric names by filename */
    MMV_FLAG_PROCESS	= 0x2,	/* Indicates process check on PID needed */
} mmv_stats_flags_t;

typedef struct {
    char		magic[4];	/* MMV\0 */
    int32_t		version;	/* version */
    uint64_t		g1;		/* Generation numbers */
    uint64_t		g2;
    int32_t		tocs;		/* Number of toc entries */
    mmv_stats_flags_t	flags;
    int32_t		process;	/* client process identifier (flags) */
    int32_t		cluster;	/* preferred PMDA cluster identifier */
} mmv_disk_header_t;

typedef enum {
    MMV_TOC_INDOMS	= 1,	/* mmv_disk_indom_t */
    MMV_TOC_INSTANCES	= 2,	/* mmv_disk_instance_t */
    MMV_TOC_METRICS	= 3,	/* mmv_disk_metric_t */
    MMV_TOC_VALUES	= 4,	/* mmv_disk_value_t */
    MMV_TOC_STRINGS	= 5,	/* mmv_disk_string_t */
} mmv_toc_type_t;

typedef struct {
    mmv_toc_type_t	type;		/* What is it? */
    int32_t		count;		/* Number of entries */
    uint64_t		offset;		/* Offset of section from file start */
} mmv_disk_toc_t;

//
// 

#define MMV_NAMEMAX	64
#define MMV_STRINGMAX	256

typedef enum mmv_metric_type {
    MMV_TYPE_I64       = 2,	/* 64-bit signed integer */
    MMV_TYPE_STRING    = 6,	/* NULL-terminated string */
} mmv_metric_type_t;

typedef enum mmv_metric_sem {
    MMV_SEM_COUNTER    = 1,	/* cumulative counter (monotonic increasing) */
    MMV_SEM_INSTANT    = 3,	/* instantaneous value, continuous domain */
    MMV_SEM_DISCRETE   = 4,	/* instantaneous value, discrete domain */
} mmv_metric_sem_t;

typedef struct {
    uint64_t		indom;		/* Offset into files indom section */
    uint32_t		padding;	/* zero filled, alignment bits */
    int32_t		internal;	/* Internal instance ID */
    char		external[MMV_NAMEMAX];	/* External instance ID */
} mmv_disk_instances_t;

typedef struct {
    uint32_t		serial;		/* Unique identifier */
    uint32_t		count;		/* Number of instances */
    uint64_t		offset;		/* Offset of first instance */
    uint64_t		shorttext;	/* Offset of short help text string */
    uint64_t		helptext;	/* Offset of long help text string */
} mmv_disk_indom_t;

typedef struct {
#ifdef __BIG_ENDIAN_BITFIELD
    signed int		dimSpace : 4;	/* space dimension */
    signed int		dimTime : 4;	/* time dimension */
    signed int		dimCount : 4;	/* event dimension */
    unsigned int	scaleSpace : 4;	/* one of MMV_SPACE_* below */
    unsigned int	scaleTime : 4;	/* one of MMV_TIME_* below */
    signed int		scaleCount : 4;	/* one of MMV_COUNT_* below */
    unsigned int	pad : 8;
#else
    unsigned int	pad : 8;
    signed int		scaleCount : 4;	/* one of MMV_COUNT_* below */
    unsigned int	scaleTime : 4;	/* one of MMV_TIME_* below */
    unsigned int	scaleSpace : 4;	/* one of MMV_SPACE_* below */
    signed int		dimCount : 4;	/* event dimension */
    signed int		dimTime : 4;	/* time dimension */
    signed int		dimSpace : 4;	/* space dimension */
#endif
} mmv_units_t;			/* dimensional units and scale of value */

/* mmv_units_t.scaleSpace */
#define MMV_SPACE_BYTE	0	/* bytes */
#define MMV_SPACE_KBYTE	1	/* Kilobytes (1024) */
#define MMV_SPACE_MBYTE	2	/* Megabytes (1024^2) */
#define MMV_SPACE_GBYTE	3	/* Gigabytes (1024^3) */
#define MMV_SPACE_TBYTE	4	/* Terabytes (1024^4) */
#define MMV_SPACE_PBYTE	5	/* Petabytes (1024^5) */
#define MMV_SPACE_EBYTE	6	/* Exabytes  (1024^6) */

/* mmv_units_t.scaleTime */
#define MMV_TIME_NSEC	0	/* nanoseconds */
#define MMV_TIME_USEC	1	/* microseconds */
#define MMV_TIME_MSEC	2	/* milliseconds */
#define MMV_TIME_SEC	3	/* seconds */
#define MMV_TIME_MIN	4	/* minutes */
#define MMV_TIME_HOUR	5	/* hours */

/*
 * mmv_units_t.scaleCount (e.g. count events, syscalls, interrupts, etc.)
 * -- these are simply powers of 10, and not enumerated here,
 *    e.g. 6 for 10^6, or -3 for 10^-3
 */
#define MMV_COUNT_ONE	0	/* 1 */

typedef struct {
    uint64_t		indom;		/* Offset into files indom section */
    uint32_t		padding;	/* zero filled, alignment bits */
    int32_t		internal;	/* Internal instance ID */
    char		external[MMV_NAMEMAX];	/* External instance ID */
} mmv_disk_instance_t;

typedef struct {
    char		payload[MMV_STRINGMAX];	/* NULL terminated string */
} mmv_disk_string_t;

typedef struct {
    char		name[MMV_NAMEMAX];
    uint32_t		item;		/* Unique identifier */
    mmv_metric_type_t	type;
    mmv_metric_sem_t	semantics;
    mmv_units_t		dimension;
    uint32_t		indom;		/* Indom serial */
    uint32_t		padding;	/* zero filled, alignment bits */
    uint64_t		shorttext;	/* Offset of short help text string */
    uint64_t		helptext;	/* Offset of long help text string */
} mmv_disk_metric_t;

/* Generic Union for Value-Type conversions */
typedef union {
    int64_t		ll;	/* 64-bit signed */
    char		*cp;	/* char ptr */
} mmv_value_t;

typedef struct {
    mmv_value_t		value;		/* Union of all possible value types */
    int64_t		extra;		/* INTEGRAL(starttime)/STRING(offset) */
    uint64_t		metric;		/* Offset into the metric section */
    uint64_t		instance;	/* Offset into the instance section */
} mmv_disk_value_t;

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
