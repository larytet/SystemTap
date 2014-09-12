/* -*- linux-c -*- 
 * Copy from user space functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_COPY_C_
#define _STAPDYN_COPY_C_

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/uio.h>

#include "stp_string.c"


static int _stp_mem_fd = -1;


static inline __must_check ssize_t __self_readv(const struct iovec *lvec,
						unsigned long liovcnt,
						const struct iovec *rvec,
						unsigned long riovcnt)
{
#if !__GLIBC_PREREQ(2, 15)
#ifdef __NR_process_vm_readv
#define process_vm_readv(...) \
	syscall(__NR_process_vm_readv, __VA_ARGS__)
#else
#define process_vm_readv(...) \
	({ (void)lvec; (void)liovcnt; \
	 (void)rvec; (void)riovcnt; \
	 errno = ENOSYS; -1 })
#endif
#endif
	return process_vm_readv(getpid(), lvec, liovcnt, rvec, riovcnt, 0UL);
}

static inline __must_check ssize_t __self_writev(const struct iovec *lvec,
						 unsigned long liovcnt,
						 const struct iovec *rvec,
						 unsigned long riovcnt)
{
#if !__GLIBC_PREREQ(2, 15)
#ifdef __NR_process_vm_writev
#define process_vm_writev(...) \
	syscall(__NR_process_vm_writev, __VA_ARGS__)
#else
#define process_vm_writev(...) \
	({ (void)lvec; (void)liovcnt; \
	 (void)rvec; (void)riovcnt; \
	 errno = ENOSYS; -1 })
#endif
#endif
	return process_vm_writev(getpid(), lvec, liovcnt, rvec, riovcnt, 0UL);
}


static int _stp_copy_init(void)
{
	/* Try a no-op process_vm_readv/writev to make sure they're available,
	 * esp. not ENOSYS, then we don't need to bother /proc/self/mem.  */
	if ((__self_readv(NULL, 0, NULL, 0) == 0) &&
			(__self_writev(NULL, 0, NULL, 0) == 0))
		return 0;

	_stp_mem_fd = open("/proc/self/mem", O_RDWR /*| O_LARGEFILE*/);
	if (_stp_mem_fd < 0)
		return -errno;
	fcntl(_stp_mem_fd, F_SETFD, FD_CLOEXEC);
	return 0;
}

static void _stp_copy_destroy(void)
{
	if (_stp_mem_fd >= 0) {
		close (_stp_mem_fd);
		_stp_mem_fd = -1;
	}
}


static inline __must_check long __copy_from_user(void *to,
		const void __user * from, unsigned long n)
{
	int rc = 0;

	if (_stp_mem_fd >= 0) {
		/* pread is like lseek+read, without racing other threads. */
		if (pread(_stp_mem_fd, to, n, (off_t)(uintptr_t)from) != n)
			rc = -EFAULT;
	} else {
		struct iovec lvec = { .iov_base = to, .iov_len = n };
		struct iovec rvec = { .iov_base = (void *)from, .iov_len = n };
		if (__self_readv(&lvec, 1, &rvec, 1) != n)
			rc = -EFAULT;
	}

	return rc;
}

static inline __must_check long __copy_to_user(void *to, const void *from,
					       unsigned long n)
{
	int rc = 0;

	if (_stp_mem_fd >= 0) {
		/* pwrite is like lseek+write, without racing other threads. */
		/* NB: some kernels will refuse to write /proc/self/mem  */
		if (pwrite(_stp_mem_fd, from, n, (off_t)(uintptr_t)to) != n)
			rc = -EFAULT;
	} else {
		struct iovec lvec = { .iov_base = (void *)from, .iov_len = n };
		struct iovec rvec = { .iov_base = to, .iov_len = n };
		if (__self_writev(&lvec, 1, &rvec, 1) != n)
			rc = -EFAULT;
	}

	return rc;
}

static long
_stp_strncpy_from_user(char *dst, const char *src, long count)
{
	if (count <= 0)
		return -EINVAL;

	/* Reads are batched on aligned 4k boundaries, approximately
	 * page size, to reduce the number of pread syscalls.  It will
	 * likely read past the terminating '\0', but shouldn't fault.
	 * NB: We shouldn't try to read the entire 'count' at once, in
	 * case some small string is already near the end of its page.
	 */
	long i = 0;
	while (i < count) {
		long n = 0x1000 - ((long)(src + i) & 0xFFF);
		n = min(n, count - i);
		if (__copy_from_user(dst + i, src + i, n))
			return -EFAULT;

		char *dst0 = memchr(dst + i, 0, n);
		if (dst0 != NULL)
			return (dst0 - dst);

		i += n;
	}

	dst[i - 1] = 0;
	return i - 1;
}

static unsigned long _stp_copy_from_user(char *dst, const char *src, unsigned long count)
{
	if (count && __copy_from_user(dst, src, count) == 0)
		return 0;
	return count;
}

#endif /* _STAPDYN_COPY_C_ */

