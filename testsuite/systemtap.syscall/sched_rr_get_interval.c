/* COVERAGE: sched_rr_get_interval */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sched.h>

int main()
{
    struct timespec tp;

    sched_rr_get_interval(0, &tp);
    //staptest// sched_rr_get_interval (0, XXXX) = 0

    sched_rr_get_interval(-1, &tp);
    //staptest// sched_rr_get_interval (-1, XXXX) = -NNNN (EINVAL)

    sched_rr_get_interval(999999, &tp);
    //staptest// sched_rr_get_interval (999999, XXXX) = -NNNN (ESRCH)

    sched_rr_get_interval(0, (struct timespec *)-1);
#ifdef __s390__
    //staptest// sched_rr_get_interval (0, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// sched_rr_get_interval (0, 0x[f]+) = -NNNN (EFAULT)
#endif

    return 0;
}
