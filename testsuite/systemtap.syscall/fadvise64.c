/* COVERAGE: fadvise64 fadvise64_64 */

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#define _ISOC99_SOURCE		   /* Needed for LLONG_MAX on RHEL5 */
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/syscall.h>

/*
 * There are actually 2 related syscalls here:
 *
 * - fadvise64(int fd, loff_t offset, size_t len, int advice)
 * - fadvise64_64(int fd, loff_t offset, loff_t len, int advice)
 *
 * The difference is that on fadvise64(), 'offset' is 64-bit and 'len'
 * is 64-bits (on a 64-bit system) or 32-bits (on a 32-bit system),
 * while on fadvise64_64(), both 'offset' and 'len' are always
 * 64-bits. posix_fadvise() is a wrapper around fadvice64_64(), since
 * both 'offset' and 'len' are always 64-bits.
 *
 * So, let's define our own fadvice64() wrapper.
 */

#ifdef __NR_fadvise64
int __fadvise64(int fd, loff_t offset, size_t len, int advice)
{
#if defined(__powerpc__) && !defined(__powerpc64__)
    /* ppc (not ppc64) has an extra unused argument as arg 2 */
    return syscall(__NR_fadvise64, fd, 0, (unsigned int)(offset >> 32),
		   (unsigned int)offset, len, advice);
#else
    return syscall(__NR_fadvise64, fd, offset, len, advice);
#endif
}
#endif

int main()
{
    int fd;

    fd = open("foobar", O_WRONLY|O_CREAT, S_IRWXU);

    posix_fadvise(fd, 0, 1024, POSIX_FADV_NORMAL);
    //staptest// fadvise64 (NNNN, 0, 1024, POSIX_FADV_NORMAL) = 0

    /* Limit testing. */

    posix_fadvise(-1, 0, 1024, POSIX_FADV_RANDOM);
    //staptest// fadvise64 (-1, 0, 1024, POSIX_FADV_RANDOM) = NNNN

    posix_fadvise(fd, -1, 1024, POSIX_FADV_SEQUENTIAL);
    //staptest// fadvise64 (NNNN, -1, 1024, POSIX_FADV_SEQUENTIAL) = NNNN

    posix_fadvise(fd, 0, -1, POSIX_FADV_WILLNEED);
    //staptest// fadvise64 (NNNN, 0, -1, POSIX_FADV_WILLNEED) = NNNN

    posix_fadvise(fd, LLONG_MAX, 0, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 9223372036854775807, 0, POSIX_FADV_DONTNEED) = NNNN

    posix_fadvise(fd, 0x72345678deadbeefLL, 0, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 8229297496244731631, 0, POSIX_FADV_DONTNEED) = NNNN

    posix_fadvise(fd, 0, LLONG_MAX, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 0, 9223372036854775807, POSIX_FADV_DONTNEED) = NNNN
    posix_fadvise(fd, 0, 0x72345678deadbeefLL, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 0, 8229297496244731631, POSIX_FADV_DONTNEED) = NNNN

    posix_fadvise(fd, 0, 1024, -1);
    //staptest// fadvise64 (NNNN, 0, 1024, 0x[f]+) = NNNN

#ifdef __NR_fadvise64
    __fadvise64(fd, 0, 1024, POSIX_FADV_NORMAL);
    //staptest// fadvise64 (NNNN, 0, 1024, POSIX_FADV_NORMAL) = 0

    /* Limit testing. */

    __fadvise64(-1, 0, 1024, POSIX_FADV_RANDOM);
    //staptest// fadvise64 (-1, 0, 1024, POSIX_FADV_RANDOM) = NNNN

    __fadvise64(fd, -1, 1024, POSIX_FADV_SEQUENTIAL);
    //staptest// fadvise64 (NNNN, -1, 1024, POSIX_FADV_SEQUENTIAL) = NNNN

    __fadvise64(fd, 0, -1, POSIX_FADV_WILLNEED);
    //staptest// fadvise64 (NNNN, 0, -1, POSIX_FADV_WILLNEED) = NNNN

    __fadvise64(fd, LLONG_MAX, 0, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 9223372036854775807, 0, POSIX_FADV_DONTNEED) = NNNN

    __fadvise64(fd, 0x72345678deadbeefLL, 0, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 8229297496244731631, 0, POSIX_FADV_DONTNEED) = NNNN

    __fadvise64(fd, 0, LONG_MAX, POSIX_FADV_DONTNEED);
#if __WORDSIZE == 64
    //staptest// fadvise64 (NNNN, 0, 9223372036854775807, POSIX_FADV_DONTNEED) = NNNN
#else
    //staptest// fadvise64 (NNNN, 0, 2147483647, POSIX_FADV_DONTNEED) = NNNN
#endif
    
    __fadvise64(fd, 0, 1024, -1);
    //staptest// fadvise64 (NNNN, 0, 1024, 0x[f]+) = NNNN
#endif
    close(fd);
    return 0;
}
