/* COVERAGE: timer_create timer_gettime timer_settime timer_getoverrun timer_delete */
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>

int main()
{
    timer_t tid=0;
    struct itimerspec val, oval;


    // SYSCALL timer_create -----

    syscall(__NR_timer_create, -1, NULL, &tid);
    //staptest// timer_create (0x[f]+, 0x0, XXXX) = NNNN (EINVAL)

    syscall(__NR_timer_create, CLOCK_REALTIME, -1, &tid);
#ifdef __s390__
    //staptest// timer_create (CLOCK_REALTIME, 0x[7]?[f]+, XXXX) = NNNN (EFAULT)
#else
    //staptest// timer_create (CLOCK_REALTIME, 0x[f]+, XXXX) = NNNN (EFAULT)
#endif

    syscall(__NR_timer_create, CLOCK_REALTIME, NULL, -1);
#ifdef __s390__
    //staptest// timer_create (CLOCK_REALTIME, 0x0, 0x[7]?[f]+) = NNNN (EFAULT)
#else
    //staptest// timer_create (CLOCK_REALTIME, 0x0, 0x[f]+) = NNNN (EFAULT)
#endif

    timer_create(CLOCK_REALTIME, NULL, &tid);
    //staptest// timer_create (CLOCK_REALTIME, XXXX, XXXX) = 0


    // SYSCALL timer_gettime -----

    syscall(__NR_timer_gettime, -1, &val);
    //staptest// timer_gettime (-1, XXXX) = NNNN (EINVAL)

    syscall(__NR_timer_gettime, tid, -1);
#ifdef __s390__
    //staptest// timer_gettime (NNNN, 0x[7]?[f]+) = NNNN (EINVAL)
#else
    //staptest// timer_gettime (NNNN, 0x[f]+) = NNNN (EINVAL)
#endif

    timer_gettime(tid, &val);
    //staptest// timer_gettime (NNNN, XXXX) = 0


    // SYSCALL timer_settime -----

    val.it_value.tv_sec = 0;
    val.it_value.tv_nsec = 0;
    val.it_interval.tv_sec = 0;
    val.it_interval.tv_nsec = 0;

    syscall(__NR_timer_settime, -1, 0, &val, &oval);
    //staptest// timer_settime (-1, 0, \[0.000000,0.000000\], XXXX) = NNNN (EINVAL)

    syscall(__NR_timer_settime, 0, -1, &val, &oval);
    //staptest// timer_settime (0, -1, \[0.000000,0.000000\], XXXX) = NNNN

    syscall(__NR_timer_settime, tid, 0, -1, &oval);
#ifdef __s390__
    //staptest// timer_settime (NNNN, 0, 0x[7]?[f]+, XXXX) = NNNN (EFAULT)
#else
    //staptest// timer_settime (NNNN, 0, 0x[f]+, XXXX) = NNNN (EFAULT)
#endif

    syscall(__NR_timer_settime, tid, 0, &val, -1);
#ifdef __s390__
    //staptest// timer_settime (NNNN, 0, \[0.000000,0.000000\], 0x[7]?[f]+) = NNNN (EINVAL)
#else
    //staptest// timer_settime (NNNN, 0, \[0.000000,0.000000\], 0x[f]+) = NNNN (EINVAL)
#endif

    timer_settime(tid, -1, &val, &oval);
    //staptest// timer_settime (NNNN, -1, \[0.000000,0.000000\], XXXX) = 0


    // SYSCALL timer_getoverrun -----

    syscall(__NR_timer_getoverrun, -1);
    //staptest// timer_getoverrun (-1) = NNNN (EINVAL)

    timer_getoverrun(tid);
    //staptest// timer_getoverrun (NNNN) = 0


    // SYSCALL timer_delete -----

    syscall(__NR_timer_delete, -1);
    //staptest// timer_delete (-1) = NNNN (EINVAL)

    timer_delete(tid);
    //staptest// timer_delete (NNNN) = 0

    return 0;
}

