/* COVERAGE: readahead */

#define _GNU_SOURCE
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

int main() {
    int fd;
    char tempname[17] = "readahead_XXXXXX";

    fd = mkstemp(tempname);

    readahead(fd, 0, 0);
    //staptest// readahead (NNNN, 0, 0) = 0

    readahead(5, 0, 0);
    //staptest// readahead (5, 0, 0)

    // Before glibc 2.8, ppc (not ppc64) didn't pass the extra
    // argument to compat_sys_readahead(). So, we'll need to skip
    // calls with anything other than 0 passed as the offset or
    // count.
#if !(defined(__powerpc__) && !defined(__powerpc64__)) || __GLIBC_PREREQ(2, 8) 
    // Before glibc 2.8, glibc passed 64-bit values backwards on
    // several systems.
#if __GLIBC_PREREQ(2, 8) 
    readahead(0, 0x12345678abcdefabLL, 0);
    //staptest// readahead (0, 1311768467750121387, 0)
#endif

    readahead(0, 0, 5);
    //staptest// readahead (0, 0, 5)

    readahead(-1, 0, 0);
    //staptest// readahead (-1, 0, 0) = NNNN

    readahead(fd, -1LL, 0);
    //staptest// readahead (NNNN, -1, 0) = 0

    readahead(fd, 0, -1);
#if __WORDSIZE == 64
    //staptest// readahead (NNNN, 0, 18446744073709551615) = 0
#else
    //staptest// readahead (NNNN, 0, 4294967295) = 0
#endif
#endif

    close(fd);
    unlink(tempname);

    return 0;
}
