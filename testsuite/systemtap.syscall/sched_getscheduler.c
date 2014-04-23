/* COVERAGE: sched_getscheduler */

#include <stdio.h>
#include <unistd.h>
#include <sched.h>

int main()
{
    sched_getscheduler(0);
    //staptest// sched_getscheduler (0) = 0

    sched_getscheduler((pid_t)-1);
    //staptest// sched_getscheduler (-1) = -NNNN (EINVAL)

    sched_getscheduler(999999);
    //staptest// sched_getscheduler (999999) = -NNNN (ESRCH)

    return 0;
}
