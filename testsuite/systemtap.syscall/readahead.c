/* COVERAGE: readahead */

#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

int main() {
    int fd;
    char tempname[17] = "readahead_XXXXXX";

    fd = mkstemp(tempname, O_RDWR);

    readahead(fd, 0, 0);
    //staptest// readahead (NNNN, 0, 0) = 0

    readahead(5, 0, 0);
    //staptest// readahead (5, 0, 0)

    readahead(0, 0x12345678abcdefabLL, 0);
    //staptest// readahead (0, 1311768467750121387, 0)

    readahead(0, 0, 5);
    //staptest// readahead (0, 0, 5)

    readahead(-1, 0, 0);
    //staptest// readahead (-1, 0, 0) = NNNN

    readahead(fd, -1, 0);
    //staptest// readahead (NNNN, -1, 0) = 0

    readahead(fd, 0, -1);
#if __WORDSIZE == 64
    //staptest// readahead (NNNN, 0, 18446744073709551615) = 0
#else
    //staptest// readahead (NNNN, 0, 4294967295) = 0
#endif

    close(fd);
    unlink(tempname);

    return 0;
}
