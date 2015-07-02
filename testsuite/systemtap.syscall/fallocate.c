/* COVERAGE: fallocate */

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
#include <linux/falloc.h>
#include <unistd.h>
#include <sys/syscall.h>


// We'd like to use the gilbc wrapper here, but it only exists in
// glibc 2.10+. When it does exist, we want to be sure to use the
// 64-bit version.
#ifdef __NR_fallocate
static inline int __fallocate(int fd, int mode, off64_t offset, off64_t len)
{
#if __GLIBC_PREREQ(2, 10)
#ifdef __USE_LARGEFILE64
    return fallocate64(fd, mode, offset, len);
#else
    return fallocate(fd, mode, offset, len);
#endif
#else
#if defined(__powerpc__) && !defined(__powerpc64__)
    /* ppc (not ppc64) has the 64-bit arguments broken up. */
    return syscall(__NR_fallocate, fd, mode,
		   (unsigned int)(offset >> 32), (unsigned int)offset,
		   (unsigned int)(len >> 32), (unsigned int)len);
#else
    return syscall(__NR_fallocate, fd, mode, offset, len);
#endif
#endif
}
#endif

int main()
{
#ifdef __NR_fallocate
    int fd;

    fd = open("foo", O_RDWR | O_CREAT, 0700);

    __fallocate(fd, 0, 0LL, 4096LL);
    //staptest// fallocate (NNNN, 0x0, 0x0, 4096) = NNNN

#ifdef FALLOC_FL_KEEP_SIZE
    __fallocate(fd, FALLOC_FL_KEEP_SIZE, 0LL, 8192LL);
    //staptest// fallocate (NNNN, FALLOC_FL_KEEP_SIZE, 0x0, 8192) = NNNN
#endif

    close(fd);
    //staptest// close (NNNN) = 0

    /* Limit testing. */
    __fallocate(-1, 0, 0LL, 0LL);
    //staptest// fallocate (-1, 0x0, 0x0, 0) = NNNN

    __fallocate(fd, -1, 0LL, 0LL);
    //staptest// fallocate (NNNN, FALLOC_[^ ]+|XXXX, 0x0, 0) = NNNN

    __fallocate(fd, 0, -1LL, 0LL);
    //staptest// fallocate (NNNN, 0x0, 0xffffffffffffffff, 0) = NNNN

    __fallocate(fd, 0, 0xdeadbeef12345678LL, 0LL);
    //staptest// fallocate (NNNN, 0x0, 0xdeadbeef12345678, 0) = NNNN

    __fallocate(fd, 0, 0LL, -1LL);
    //staptest// fallocate (NNNN, 0x0, 0x0, 18446744073709551615) = NNNN

    __fallocate(fd, 0, 0LL, 0xdeadbeef12345678LL);
    //staptest// fallocate (NNNN, 0x0, 0x0, 16045690981402826360) = NNNN
#endif

    return 0;
}
