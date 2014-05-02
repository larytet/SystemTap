/* COVERAGE: setpriority */

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

int main()
{
    setpriority(PRIO_PROCESS, 0, 2);
    //staptest// setpriority (PRIO_PROCESS, 0, 2) = NNNN

    setpriority(-1, 0, 2);
    //staptest// setpriority (0x[f]+, 0, 2) = -NNNN (EINVAL)

    setpriority(PRIO_PGRP, -1, 2);
    //staptest// setpriority (PRIO_PGRP, -1, 2) = -NNNN (ESRCH)

    setpriority(PRIO_USER, 0, 0);
    //staptest// setpriority (PRIO_USER, 0, 0) = NNNN

    // This call might or might not fail, depending on whether we're
    // root or not. So, ignore the return value.
    setpriority(PRIO_PROCESS, 0, -1);
    //staptest// setpriority (PRIO_PROCESS, 0, -1)
}
