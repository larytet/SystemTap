/* COVERAGE: getrlimit */
#include <sys/time.h>
#include <sys/resource.h>

struct rlimit rlim;

int main() {

    // --- first try out normal operation ---

    getrlimit(RLIMIT_CPU, &rlim);
    //staptest// getrlimit (RLIMIT_CPU, XXXX) = 0

    // --- then check nasty calls ---

    getrlimit(-1, &rlim);
    //staptest// getrlimit (RLIM_INFINITY, XXXX) = NNNN (EINVAL)

    getrlimit(-15, &rlim);
    //staptest// getrlimit (UNKNOWN VALUE: -15, XXXX) = NNNN (EINVAL)

    getrlimit(RLIMIT_CPU, (struct rlimit *)-1);
#ifdef __s390__
    //staptest// getrlimit (RLIMIT_CPU, 0x[7]?[f]+) = NNNN (EFAULT)
#else
    //staptest// getrlimit (RLIMIT_CPU, 0x[f]+) = NNNN (EFAULT)
#endif

    return 0;
}
