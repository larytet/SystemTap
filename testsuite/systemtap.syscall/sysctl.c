/* COVERAGE: sysctl */

#define _GNU_SOURCE
#include <unistd.h>
#include <linux/sysctl.h>
#include <sys/syscall.h>

int main()
{
    // Man page states that this syscall is deprecated
    // and is likely to disappear in future kernel versions.

#ifdef SYS__sysctl
    struct __sysctl_args args;

    syscall(SYS__sysctl, &args);
    //staptest// sysctl (XXXX) = -NNNN

    syscall(SYS__sysctl, (struct __sysctl_args *)-1);
#ifdef __s390__
    //staptest// sysctl (0x[7]?[f]+) = -NNNN
#else
    //staptest// sysctl (0x[f]+) = -NNNN
#endif
#endif

    return 0;
}
