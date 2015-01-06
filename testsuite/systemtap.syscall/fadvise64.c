/* COVERAGE: fadvise64 fadvise64_64 */

#define _LARGEFILE64_SOURCE
#define _ISOC99_SOURCE		   /* Needed for LLONG_MAX on RHEL5 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

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

#if __WORDSIZE == 64
    posix_fadvise(fd, LLONG_MAX, 0, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 9223372036854775807, 0, POSIX_FADV_DONTNEED) = NNNN

    posix_fadvise(fd, 0, LLONG_MAX, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 0, 9223372036854775807, POSIX_FADV_DONTNEED) = NNNN
#else
    posix_fadvise(fd, LONG_MAX, 0, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 2147483647, 0, POSIX_FADV_DONTNEED) = NNNN

    posix_fadvise(fd, 0, LONG_MAX, POSIX_FADV_DONTNEED);
    //staptest// fadvise64 (NNNN, 0, 2147483647, POSIX_FADV_DONTNEED) = NNNN
#endif

    posix_fadvise(fd, 0, 1024, -1);
    //staptest// fadvise64 (NNNN, 0, 1024, 0x[f]+) = NNNN

    close(fd);
}
