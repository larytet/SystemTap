/* COVERAGE: sched_getaffinity */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>

int main()
{
    cpu_set_t mask;

    sched_getaffinity(0, sizeof(mask), &mask);
    //staptest// sched_getaffinity (0, NNNN, XXXX) = NNNN

    sched_getaffinity(-1, sizeof(mask), &mask);
    //staptest// sched_getaffinity (-1, NNNN, XXXX) = -NNNN

    // glibc mangles this one, so use syscall().
    //sched_getaffinity(0, (size_t)-1, &mask);
#ifdef __NR_sched_setaffinity
    syscall(__NR_sched_getaffinity, (pid_t)0, (size_t)-1, &mask);
    //staptest// sched_getaffinity (0, 4294967295, XXXX) = NNNN
#endif

    sched_getaffinity(0, sizeof(mask), (cpu_set_t *)-1);
#ifdef __s390__
    //staptest// sched_getaffinity (0, XXXX, 0x[7]?[f]+) = -NNNN
#else
    //staptest// sched_getaffinity (0, XXXX, 0x[f]+) = -NNNN
#endif

    return 0;
}
