/* COVERAGE: setns */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#ifndef CLONE_NEWIPC
#define CLONE_NEWIPC 0x08000000
#endif

int main()
{
// This testcase can crash the s390x kernel. Please, refer to rhbz1219586.
// Skipping it on that platform.
#ifndef __s390__
    char *fn;
    int fd;

    fn = malloc(1000);
    sprintf(fn, "/proc/%d/ns/pid\0", getpid());
    fd = open(fn, O_RDONLY);

#ifdef __NR_setns
    // using syscall() to avoid link time issues on rhel6
    syscall(__NR_setns, fd, 0);
    //staptest// setns (NNNN, 0x0) = NNNN

    syscall(__NR_setns, fd, CLONE_NEWIPC);
    //staptest// setns (NNNN, CLONE_NEWIPC) = NNNN

    // Limit testing

    syscall(__NR_setns, -1, CLONE_NEWIPC);
    //staptest// setns (-1, CLONE_NEWIPC) = -NNNN

    syscall(__NR_setns, fd, -1);
    //staptest// setns (NNNN, CLONE_VM|CLONE_FS|CLONE_FILES[^\)]+) = -NNNN
#endif

    close(fd);
    free(fn);
#endif

    return 0;
}
