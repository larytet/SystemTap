/* COVERAGE: prlimit64 */

// Why wasn't this testing just added to getrlimit.c? If you want to
// call prlimit(), you have to have _FILE_OFFSET_BITS defined as
// 64. But, when you do that, glibc redefines calls to
// getrlimit()/setrlimit() as prlimit64() calls. So, we'll have to
// test them separately.

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <features.h>

int main()
{
// glibc support for prlimit() was added in glibc 2.13.
#if __GLIBC_PREREQ(2, 13)
    struct rlimit rlim;

    // Get current cpu limits.
    prlimit(0, RLIMIT_CPU, NULL, &rlim);
    //staptest// prlimit64 (0, RLIMIT_CPU, NULL, XXXX) = 0

    // Set new limits (to the same thing).
    prlimit(0, RLIMIT_CPU, &rlim, NULL);
    //staptest// prlimit64 (0, RLIMIT_CPU, \[NNNN,NNNN\], 0x0) = 0

    // Limit testing.

    prlimit(-1, RLIMIT_CPU, &rlim, NULL);
    //staptest// prlimit64 (-1, RLIMIT_CPU, \[NNNN,NNNN\], 0x0) = -NNNN

    prlimit(0, -1, &rlim, NULL);
    //staptest// prlimit64 (0, RLIM_INFINITY, \[NNNN,NNNN\], 0x0) = -NNNN

    prlimit(0, RLIMIT_CPU, (struct rlimit *)-1, NULL);
#ifdef __s390__
    //staptest// prlimit64 (0, RLIMIT_CPU, 0x[7]?[f]+, 0x0) = -NNNN
#else
    //staptest// prlimit64 (0, RLIMIT_CPU, 0x[f]+, 0x0) = -NNNN
#endif

    prlimit(0, RLIMIT_CPU, &rlim, (struct rlimit *)-1);
#ifdef __s390__
    //staptest// prlimit64 (0, RLIMIT_CPU, \[NNNN,NNNN\], 0x[7]?[f]+) = -NNNN
#else
    //staptest// prlimit64 (0, RLIMIT_CPU, \[NNNN,NNNN\], 0x[f]+) = -NNNN
#endif
#endif

    return 0;
}
