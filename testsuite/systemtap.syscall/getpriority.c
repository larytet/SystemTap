/* COVERAGE: getpriority */

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

int main()
{
    getpriority(PRIO_PROCESS, 0);
    //staptest// getpriority (PRIO_PROCESS, 0) = NNNN

    getpriority(PRIO_PGRP, -1);
    //staptest// getpriority (PRIO_PGRP, -1) = -NNNN (ESRCH)

    getpriority(-1, 0);
    //staptest// getpriority (0x[f]+, 0) = -NNNN (EINVAL)

    getpriority(PRIO_USER, 0);
    //staptest// getpriority (PRIO_USER, 0) = NNNN
}
