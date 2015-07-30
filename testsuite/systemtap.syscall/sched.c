/* COVERAGE: sched_getparam sched_setparam sched_get_priority_max sched_get_priority_min sched_yield */

#define _GNU_SOURCE
#include <sched.h>

int main() {
    struct sched_param sp;

    // test normal operation ------------------------------------

    sched_getparam(0, &sp);
    //staptest// sched_getparam (0, XXXX) = 0

    sched_setparam(0, &sp);
    //staptest// sched_setparam (0, XXXX) = 0

    sched_get_priority_max(SCHED_OTHER);
    //staptest// sched_get_priority_max (SCHED_OTHER) = 0

    sched_get_priority_max(SCHED_BATCH);
    //staptest// sched_get_priority_max (SCHED_BATCH) = 0

#ifdef SCHED_IDLE
    sched_get_priority_max(SCHED_IDLE);
    //staptest// sched_get_priority_max (SCHED_IDLE) = 0
#endif

    sched_get_priority_max(SCHED_FIFO);
    //staptest// sched_get_priority_max (SCHED_FIFO) = NNNN

    sched_get_priority_max(SCHED_RR);
    //staptest// sched_get_priority_max (SCHED_RR) = NNNN

    sched_get_priority_min(SCHED_OTHER);
    //staptest// sched_get_priority_min (SCHED_OTHER) = 0

    sched_yield();
    //staptest// sched_yield () = 0

    // test nasty operation -------------------------------------

    sched_getparam(-1, &sp);
    //staptest// sched_getparam (-1, XXXX) = NNNN

    sched_getparam(0, (struct sched_param *)-1);
#ifdef __s390__
    //staptest// sched_getparam (0, 0x[7]?[f]+) = NNNN
#else
    //staptest// sched_getparam (0, 0x[f]+) = NNNN
#endif

    sched_setparam(-1, &sp);
    //staptest// sched_setparam (-1, XXXX) = NNNN

    sched_setparam(0, (struct sched_param *)-1);
#ifdef __s390__
    //staptest// sched_setparam (0, 0x[7]?[f]+) = NNNN
#else
    //staptest// sched_setparam (0, 0x[f]+) = NNNN
#endif

    sched_get_priority_max(-1);
    //staptest// sched_get_priority_max (0xffffffff) = NNNN

    sched_get_priority_min(-1);
    //staptest// sched_get_priority_min (0xffffffff) = NNNN

    return 0;
}
