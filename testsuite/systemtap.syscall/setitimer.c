/* COVERAGE: setitimer */

#include <sys/time.h>

int main()
{
    struct itimerval value, ovalue;

    value.it_value.tv_sec = 30;
    value.it_value.tv_usec = 0;
    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &value, &ovalue);
    //staptest// setitimer (ITIMER_REAL, \[0\.000000,30\.000000\], XXXX) = 0

    setitimer(-1, &value, &ovalue);
    //staptest// setitimer (0xffffffff, \[0\.000000,30\.000000\], XXXX) = -NNNN (EINVAL)

    setitimer(ITIMER_VIRTUAL, (struct itimerval *)-1, &ovalue);
#ifdef __s390__
    //staptest// setitimer (ITIMER_VIRTUAL, 0x[7]?[f]+, XXXX) = -NNNN (EFAULT)
#else
    //staptest// setitimer (ITIMER_VIRTUAL, 0x[f]+, XXXX) = -NNNN (EFAULT)
#endif

    setitimer(ITIMER_PROF, &value, (struct itimerval *)-1);
#ifdef __s390__
    //staptest// setitimer (ITIMER_PROF, \[0\.000000,30\.000000\], 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// setitimer (ITIMER_PROF, \[0\.000000,30\.000000\], 0x[f]+) = -NNNN (EFAULT)
#endif

    return 0;
}
