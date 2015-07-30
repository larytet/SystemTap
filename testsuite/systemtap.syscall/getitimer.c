/* COVERAGE: getitimer */

#include <sys/time.h>

int main()
{
    struct itimerval value;

    getitimer(ITIMER_REAL, &value);
    //staptest// getitimer (ITIMER_REAL, XXXX) = 0

    getitimer(ITIMER_VIRTUAL, &value);
    //staptest// getitimer (ITIMER_VIRTUAL, XXXX) = 0

    getitimer(ITIMER_PROF, &value);
    //staptest// getitimer (ITIMER_PROF, XXXX) = 0

    getitimer(-1, &value);
    //staptest// getitimer (0xffffffff, XXXX) = -NNNN (EINVAL)

    getitimer(ITIMER_REAL, (struct itimerval *)-1);
#ifdef __s390__
    //staptest// getitimer (ITIMER_REAL, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// getitimer (ITIMER_REAL, 0x[f]+) = -NNNN (EFAULT)
#endif

    return 0;
}
