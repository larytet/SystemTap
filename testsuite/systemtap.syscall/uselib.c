/* COVERAGE: uselib */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/version.h>

int main()
{
    // Some systems define __NR_uselib but do not implement uselib
    // syscall.  Examples are 3.10.0-229.el7.x86_64, or
    // 3.19.4-200.fc21.i686+PAE.
    //
    // On 2.6.18-398.el5 (x86_64), the 32-bit version of uselib() is
    // set up to call 'quiet_ni_syscall()', which is an assembly
    // language function that we can't probe. This was changed in
    // 2.6.22.
#if defined(__NR_uselib) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#define uselib(x) syscall(__NR_uselib,x)
    uselib("blah");
    //staptest// [[[[uselib ("blah") = -NNNN!!!!ni_syscall () = -NNNN (ENOSYS)]]]]

    uselib((const char *)-1);
#ifdef __s390__
    //staptest// [[[[uselib (0x[7]?[f]+)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[uselib (0x[f]+) = -NNNN!!!!ni_syscall () = -NNNN (ENOSYS)]]]]
#endif
#endif

    return 0;
}
