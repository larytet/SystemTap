/* COVERAGE: userfaultfd */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

// If SYS_userfaultfd is defined, let's assume that linux/userfaultfd.h
// exists.
#ifdef SYS_userfaultfd
#include <linux/userfaultfd.h>

static inline int userfaultfd(int flags)
{
    return syscall(SYS_userfaultfd, flags);
}
#endif

int main()
{
#ifdef SYS_userfaultfd
    int fd;
    struct uffdio_api api;

    fd = userfaultfd(0);
    //staptest// [[[[userfaultfd (0x0)!!!!ni_syscall ()]]]] = NNNN

    // userfaulfd API sanity check
    memset(&api, 0, sizeof(api));
    api.api = UFFD_API;
    api.features = 0;
    ioctl(fd, UFFDIO_API, &api);
    //staptest// ioctl (NNNN, NNNN, XXXX) = NNNN

    close(fd);
    //staptest// close (NNNN) = NNNN

    fd = userfaultfd(-1);
    //staptest// [[[[userfaultfd (O_[^ ]+|XXXX)!!!!ni_syscall ()]]]] = -NNNN
#endif
    return 0;
}
