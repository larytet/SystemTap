/* COVERAGE: uselib */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

int main()
{
    // Some systems define __NR_uselib but do not implement uselib syscall.
    // Examples are 3.10.0-229.el7.x86_64, or 3.19.4-200.fc21.i686+PAE.
#ifdef __NR_uselib
    uselib("blah");
    //staptest// [[[[uselib ("blah") = -NNNN!!!!ni_syscall () = -NNNN (ENOSYS)]]]]

    uselib((const char *)-1);
#ifdef __s390__
    //staptest// uselib ([7]?[f]+) = -NNNN
#else
    //staptest// [[[[uselib ([f]+) = -NNNN!!!!ni_syscall () = -NNNN (ENOSYS)]]]]
#endif
#endif

    return 0;
}
