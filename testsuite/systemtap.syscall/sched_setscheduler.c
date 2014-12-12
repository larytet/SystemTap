/* COVERAGE: sched_setscheduler */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sched.h>

int main()
{
    struct sched_param param;

    sched_setscheduler(999999, SCHED_OTHER, &param);
    //staptest// sched_setscheduler (999999, SCHED_OTHER, XXXX) = -NNNN (ESRCH)

    sched_setscheduler(-1, SCHED_FIFO, &param);
    //staptest// sched_setscheduler (-1, SCHED_FIFO, XXXX) = -NNNN (EINVAL)

#ifdef SCHED_RESET_ON_FORK
    sched_setscheduler(0, SCHED_RESET_ON_FORK|SCHED_FIFO, &param);
    //staptest// sched_setscheduler (0, SCHED_RESET_ON_FORK|SCHED_FIFO, XXXX) = NNNN
#endif

    sched_setscheduler(0, -1, &param);
    //staptest// sched_setscheduler (0, 0xffffffff, XXXX) = -NNNN (EINVAL)

    sched_setscheduler(0, SCHED_OTHER, (struct sched_param *)-1);
#ifdef __s390__
    //staptest// sched_setscheduler (0, SCHED_OTHER, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// sched_setscheduler (0, SCHED_OTHER, 0x[f]+) = -NNNN (EFAULT)
#endif

    return 0;
}
