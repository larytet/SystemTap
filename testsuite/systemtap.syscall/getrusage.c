/* COVERAGE: getrusage */

#define _GNU_SOURCE
#include <sys/time.h>
#include <sys/resource.h>

#ifndef RUSAGE_BOTH
#define RUSAGE_BOTH (-2)
#endif

int main()
{
    struct rusage usage;

    getrusage(RUSAGE_SELF, &usage);
    //staptest// getrusage (RUSAGE_SELF, XXXX) = 0

    getrusage(RUSAGE_CHILDREN, &usage);
    //staptest// getrusage (RUSAGE_CHILDREN, XXXX) = 0

    getrusage(RUSAGE_THREAD, &usage);
    //staptest// getrusage (RUSAGE_THREAD, XXXX) = 0

    // RUSAGE_BOTH isn't valid for getrusage(), but we don't care. If
    // we pass it in, we should get it back.
    getrusage(RUSAGE_BOTH, &usage);
    //staptest// getrusage (RUSAGE_BOTH, XXXX) = -NNNN (EINVAL)

    getrusage(-15, &usage);
    //staptest// getrusage (0xfffffff1, XXXX) = -NNNN

    getrusage(RUSAGE_SELF, (struct rusage *)-1);
#ifdef __s390__
    //staptest// getrusage (RUSAGE_SELF, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// getrusage (RUSAGE_SELF, 0x[f]+) = -NNNN (EFAULT)
#endif

    return 0;
}
