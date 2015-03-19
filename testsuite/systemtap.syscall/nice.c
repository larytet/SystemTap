/* COVERAGE: nice */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#ifdef __NR_nice
int __nice(int inc)
{
    /* glibc wrapper sysdeps/posix/nice.c uses get/setpriority
     * syscalls so we do need syscall() here */
    return syscall(__NR_nice, inc);
}

int main()
{
    __nice(1);
    //staptest// nice (1) = NNNN

    __nice(0);
    //staptest// nice (0) = NNNN

    __nice(-1);
    //staptest// nice (-1) = NNNN

    return 0;
}
#else
int main()
{
    return 0;
}
#endif
