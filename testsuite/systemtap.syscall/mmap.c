/* COVERAGE: mmap mmap2 mmap_pgoff munmap msync */
/* COVERAGE: mlock mlock2 mlockall munlock munlockall */
/* COVERAGE: mprotect mremap */
#include <sys/types.h>
#include <sys/stat.h>
#define __USE_GNU
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

#ifdef __NR_mlock2

#ifndef MLOCK_ONFAULT
#define MLOCK_ONFAULT 0x01
#endif

static inline int __mlock2(const void *start, size_t len, int flags)
{
	return syscall(__NR_mlock2, start, len, flags);
}
#endif

int main()
{
	int fd, ret;
	struct stat fs;
	void * r;

	/* create a file with something in it */
	fd = open("foobar",O_WRONLY|O_CREAT|O_TRUNC, 0600);
	//staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?|O_TRUNC, 0600) = NNNN

	// Why 64k? ppc64 has 64K pages. ia64 has 16k
	// pages. x86_64/i686 has 4k pages. When we specify an offset
	// to mmap(), it must be a multiple of the page size, so we
	// use the biggest.
	lseek(fd, 65536, SEEK_SET);
	write(fd, "abcdef", 6);
	close(fd);
	//staptest// close (NNNN) = 0

	fd = open("foobar", O_RDONLY);
	//staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN

	/* stat for file size */
	ret = fstat(fd, &fs);
	//staptest// fstat (NNNN, XXXX) = 0

	r = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
	//staptest// mmap[2]* (0x0, 4096, PROT_READ, MAP_SHARED, NNNN, 0) = XXXX

	mlock(r, 4096);
	//staptest// mlock (XXXX, 4096) = 0

	mlock((void *)-1, 4096);
	//staptest// mlock (0x[f]+, 4096) = NNNN

	mlock(0, -1);
#if __WORDSIZE == 64
	//staptest// mlock (0x[0]+, 18446744073709551615) = NNNN
#else
	//staptest// mlock (0x[0]+, 4294967295) = NNNN
#endif

#ifdef __NR_mlock2
	{
		void *r2 = mmap(NULL, 12288, PROT_READ|PROT_WRITE,
				MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		//staptest// mmap[2]* (0x0, 12288, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = XXXX

		__mlock2(r2, 4096, MLOCK_ONFAULT);
		//staptest// mlock2 (XXXX, 4096, MLOCK_ONFAULT) = 0

		__mlock2((void *)-1, 4096, 0);
		//staptest// mlock2 (0x[f]+, 4096, 0x0) = NNNN

		__mlock2(0, -1, 0);
#if __WORDSIZE == 64
		//staptest// mlock2 (0x[0]+, 18446744073709551615, 0x0) = NNNN
#else
		//staptest// mlock2 (0x[0]+, 4294967295, 0x0) = NNNN
#endif

		__mlock2(0, 4096, -1);
		//staptest// mlock2 (0x[0]+, 4096, MLOCK_ONFAULT|XXXX) = NNNN

		munlock(r2, 4096);
		//staptest// munlock (XXXX, 4096) = 0
	}
#endif

	msync(r, 4096, MS_SYNC);	
	//staptest// msync (XXXX, 4096, MS_SYNC) = 0

	msync((void *)-1, 4096, MS_SYNC);	
	//staptest// msync (0x[f]+, 4096, MS_SYNC) = NNNN

	msync(r, -1, MS_SYNC);	
#if __WORDSIZE == 64
	//staptest// msync (XXXX, 18446744073709551615, MS_SYNC) = NNNN
#else
	//staptest// msync (XXXX, 4294967295, MS_SYNC) = NNNN
#endif

	msync(r, 4096, -1);	
	//staptest// msync (XXXX, 4096, MS_[^ ]+|XXXX) = NNNN

	munlock(r, 4096);
	//staptest// munlock (XXXX, 4096) = 0

	mlockall(MCL_CURRENT);
	//staptest// mlockall (MCL_CURRENT) = 

	mlockall(-1);
	//staptest// mlockall (MCL_[^ ]+|XXXX) = NNNN

	munlockall();
	//staptest// munlockall () = 0

	munmap(r, 4096);
	//staptest// munmap (XXXX, 4096) = 0

	// Ensure the 6th argument is handled correctly..
	r = mmap(NULL, 6, PROT_READ, MAP_PRIVATE, fd, 65536);
	//staptest// mmap[2]* (0x0, 6, PROT_READ, MAP_PRIVATE, NNNN, 65536) = XXXX

	munmap(r, 6);
	//staptest// munmap (XXXX, 6) = 0

	close(fd);

	r = mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	//staptest// mmap[2]* (0x0, 12288, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = XXXX

	mprotect(r, 4096, PROT_READ|PROT_EXEC);
	//staptest// mprotect (XXXX, 4096, PROT_READ|PROT_EXEC) = 0

	mprotect(r, 4096, PROT_GROWSDOWN|PROT_GROWSUP);
	//staptest// mprotect (XXXX, 4096, PROT_GROWSDOWN|PROT_GROWSUP) = -XXXX (EINVAL)

#ifdef PROT_SAO
	mprotect(r, 4096, PROT_SAO);
	//staptest// mprotect (XXXX, 4096, PROT_SAO) = 
#endif

	munmap(r, 12288);
	//staptest// munmap (XXXX, 12288) = 0

	r = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	//staptest// mmap[2]* (0x0, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = XXXX

	r = mremap(r, 8192, 4096, 0);
	//staptest// mremap (XXXX, 8192, 4096, 0x0, XXXX) = XXXX

	munmap(r, 4096);
	//staptest// munmap (XXXX, 4096) = 0

	// powerpc64's glibc rejects this one. On ia64, the call
	// fails, while on most other architectures it works. So,
	// ignore the return values.
#ifndef __powerpc64__
	r = mmap((void *)-1, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	//staptest// mmap[2]* (0x[f]+, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 

	munmap(r, 8192);
	//staptest// munmap (XXXX, 8192) = 
#endif

	r = mmap(NULL, -1, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#if __WORDSIZE == 64
	//staptest// mmap[2]* (0x0, 18446744073709551615, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = -XXXX (ENOMEM)
#else
	//staptest// mmap[2]* (0x0, 4294967295, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = -XXXX (ENOMEM)
#endif

	// powerpc's glibc (both 32-bit and 64-bit) rejects this one.
#ifndef __powerpc__
	r = mmap(NULL, 8192, -1, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	//staptest// mmap[2]* (0x0, 8192, PROT_[^ ]+|XXXX, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = XXXX

	munmap(r, 8192);
	//staptest// munmap (XXXX, 8192) = 0
#endif

	r = mmap(NULL, 8192, PROT_READ|PROT_WRITE, -1, -1, 0);
	//staptest// mmap[2]* (0x0, 8192, PROT_READ|PROT_WRITE, MAP_[^ ]+|XXXX, -1, 0) = -XXXX

	// Unfortunately, glibc and/or the syscall wrappers will
	// reject a -1 offset, especially on a 32-bit exe on a 64-bit
	// OS. So, we can't really test this one.
	//
	// r = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, -1);

	munmap((void *)-1, 8192);
	//staptest// munmap (0x[f]+, 8192) = NNNN

	munmap(r, -1);
#if __WORDSIZE == 64
	//staptest// munmap (XXXX, 18446744073709551615) = NNNN
#else
	//staptest// munmap (XXXX, 4294967295) = NNNN
#endif

#ifdef MAP_STACK
	r = mmap(NULL, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);
	//staptest// mmap[2]* (0x0, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0) = -XXXX
#endif

#ifdef MAP_HUGETLB
	r = mmap(NULL, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
	//staptest// mmap[2]* (0x0, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0) = -XXXX

#ifdef MAP_HUGE_SHIFT
	r = mmap(NULL, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|(21 << MAP_HUGE_SHIFT), -1, 0);
	//staptest// mmap[2]* (0x0, 0, PROT_READ|PROT_WRITE, MAP_HUGE_2MB|MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0) = -XXXX
#endif
#endif

#ifdef MAP_UNINITIALIZED
	r = mmap(NULL, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_UNINITIALIZED, -1, 0);
	//staptest// mmap[2]* (0x0, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_UNINITIALIZED, -1, 0) = -XXXX
#endif

#ifdef MAP_32BIT
	r = mmap(NULL, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
	//staptest// mmap[2]* (0x0, 0, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0) = -XXXX
#endif

	return 0;
}
