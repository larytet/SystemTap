/* COVERAGE: sched_getaffinity */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sched.h>

int main()
{
    cpu_set_t mask;

    sched_getaffinity(0, sizeof(mask), &mask);
    //staptest// sched_getaffinity (0, XXXX, XXXX) = NNNN

    sched_getaffinity(-1, sizeof(mask), &mask);
    //staptest// sched_getaffinity (-1, XXXX, XXXX) = -NNNN

    sched_getaffinity(0, (size_t)-1, &mask);
    // glibc mangles this -1 to 0x7fffffff instead of 0xffffffff
    //staptest// sched_getaffinity (0, 0x7fffffff, XXXX) = NNNN

    sched_getaffinity(0, sizeof(mask), (cpu_set_t *)-1);
#ifdef __s390__
    //staptest// sched_getaffinity (0, XXXX, 0x[7]?[f]+) = -NNNN
#else
    //staptest// sched_getaffinity (0, XXXX, 0x[f]+) = -NNNN
#endif

    return 0;
}
