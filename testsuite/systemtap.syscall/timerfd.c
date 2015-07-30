/* COVERAGE: timerfd_create timerfd_gettime timerfd_settime */
#include <unistd.h>
#include <sys/syscall.h>
#ifdef __NR_timerfd_create
#include <sys/timerfd.h>
#endif

int main()
{
#ifdef __NR_timerfd_create
    int fd;
    struct itimerspec val, oval;

    fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
    //staptest// timerfd_create (CLOCK_REALTIME, TFD_NONBLOCK) = NNNN

    timerfd_gettime(fd, &val);
    //staptest// timerfd_gettime (NNNN, XXXX) = 0

    val.it_value.tv_sec = 0;
    val.it_value.tv_nsec = 0;
    val.it_interval.tv_sec = 0;
    val.it_interval.tv_nsec = 0;
    timerfd_settime(fd, TFD_TIMER_ABSTIME, &val, &oval);
    //staptest// timerfd_settime (NNNN, TFD_TIMER_ABSTIME, \[0.000000,0.000000\], XXXX) = 0

    close(fd);
    //staptest// close (NNNN) = NNNN
    
    /* Limit testing. */

    fd = timerfd_create(-1, 0);
    //staptest// timerfd_create (0xffffffff, 0x0) = NNNN

    close(fd);
    //staptest// close (NNNN) = NNNN

    fd = timerfd_create(CLOCK_REALTIME, -1);
    //staptest// timerfd_create (CLOCK_REALTIME, TFD_[^ ]+|XXXX) = NNNN

    close(fd);
    //staptest// close (NNNN) = NNNN

    timerfd_gettime(-1, &val);
    //staptest// timerfd_gettime (-1, XXXX) = NNNN

    timerfd_gettime(-1, (struct itimerspec *)-1);
#ifdef __s390__
    //staptest// timerfd_gettime (-1, 0x[7]?[f]+) = NNNN
#else
    //staptest// timerfd_gettime (-1, 0x[f]+) = NNNN
#endif

    val.it_value.tv_sec = 0;
    val.it_value.tv_nsec = 0;
    val.it_interval.tv_sec = 0;
    val.it_interval.tv_nsec = 0;
    timerfd_settime(-1, TFD_TIMER_ABSTIME, &val, &oval);
    //staptest// timerfd_settime (-1, TFD_TIMER_ABSTIME, \[0.000000,0.000000\], XXXX) = NNNN

    timerfd_settime(-1, -1, &val, &oval);
    //staptest// timerfd_settime (-1, TFD_TIMER_[^ ]+|XXXX, \[0.000000,0.000000\], XXXX) = NNNN

    timerfd_settime(-1, TFD_TIMER_ABSTIME, (struct itimerspec *)-1, &oval);
#ifdef __s390__
    //staptest// timerfd_settime (-1, TFD_TIMER_ABSTIME, 0x[7]?[f]+, XXXX) = NNNN
#else
    //staptest// timerfd_settime (-1, TFD_TIMER_ABSTIME, 0x[f]+, XXXX) = NNNN
#endif

    timerfd_settime(-1, TFD_TIMER_ABSTIME, &val, (struct itimerspec *)-1);
#ifdef __s390__
    //staptest// timerfd_settime (-1, TFD_TIMER_ABSTIME, \[0.000000,0.000000\], 0x[7]?[f]+) = NNNN
#else
    //staptest// timerfd_settime (-1, TFD_TIMER_ABSTIME, \[0.000000,0.000000\], 0x[f]+) = NNNN
#endif
#endif
    return 0;
}

