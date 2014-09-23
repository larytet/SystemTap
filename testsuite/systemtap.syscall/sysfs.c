/* COVERAGE: sysfs */

#include <stdio.h>
#include <unistd.h>
#include <syscall.h>

int main()
{
#ifdef SYS_sysfs
    char buf[1024];

    int idx = syscall(SYS_sysfs, 1, "proc");
    //staptest// sysfs (1, "proc") = NNNN

    syscall(SYS_sysfs, 2, idx, buf);
    //staptest// sysfs (2, NNNN, XXXX) = 0

    syscall(SYS_sysfs, 3);
    //staptest// sysfs (3) = NNNN

    syscall(SYS_sysfs, -1, 0, buf);
    //staptest// sysfs (-1, 0x0, XXXX) = -NNNN (EINVAL)
    
    syscall(SYS_sysfs, 4, -1, buf);
    //staptest// sysfs (4, 0x[f]+, XXXX) = -NNNN (EINVAL)

    syscall(SYS_sysfs, 2, idx, (char *)-1);
    //staptest// sysfs (2, NNNN, 0x[f]+) = -NNNN (EFAULT)
#endif    
}
