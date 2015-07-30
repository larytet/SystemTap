/* COVERAGE: ioctl */

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/if_tun.h>
#include <linux/fs.h>

unsigned int fd, val;
char fname[] = "/dev/net/tun";

int main() {
    fd = open(fname, O_RDWR);
    if (fd < 0) {
        printf("ERROR (open %s)\n", fname);
    }

    // ----- try successful call -----

    ioctl(fd, TUNGETFEATURES, &val);
    //staptest/ ioctl (NNNN, NNNN, XXXX) = NNNN


    // ----- try ugly things -----

    ioctl(-1, TUNGETFEATURES, &val);
    //staptest// ioctl (-1, NNNN, XXXX) = NNNN

    ioctl(fd, -1, &val);
    //staptest// ioctl (NNNN, -1, XXXX) = NNNN

    ioctl(fd, TUNGETFEATURES, -1);
#ifdef __s390__
    //staptest// ioctl (NNNN, NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// ioctl (NNNN, NNNN, 0x[f]+) = NNNN
#endif

    close(fd);

    return 0;
}
