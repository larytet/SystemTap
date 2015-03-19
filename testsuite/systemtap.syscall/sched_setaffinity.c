/* COVERAGE: sched_setaffinity */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>

// glibc mangles some calls, so define our own sched_setaffinity()
// using syscall().
#if defined(__NR_sched_setaffinity)
static inline int
__sched_setaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
{
	return syscall(__NR_sched_setaffinity, pid, cpusetsize, mask);
}
#endif

int main()
{
    cpu_set_t mask;

    sched_getaffinity(0, sizeof(mask), &mask);
    //staptest// sched_getaffinity (0, XXXX, XXXX) = NNNN

    sched_setaffinity(0, sizeof(mask), &mask);
    //staptest// sched_setaffinity (0, XXXX, XXXX) = NNNN

    /* Limits testing */

    sched_setaffinity(-1, sizeof(mask), &mask);
    //staptest// sched_setaffinity (-1, XXXX, XXXX) = NNNN

#ifdef __NR_sched_setaffinity
    // glibc mangles this one
    __sched_setaffinity(0, (size_t)-1, &mask);
    //staptest// sched_setaffinity (0, 4294967295, XXXX) = NNNN

    // glibc mangles this one also
    __sched_setaffinity(0, sizeof(mask), (cpu_set_t *)-1);
#ifdef __s390__
    //staptest// sched_setaffinity (0, XXXX, 0x[7]?[f]+) = NNNN
#else
    //staptest// sched_setaffinity (0, XXXX, 0x[f]+) = NNNN
#endif
#endif

    return 0;
}
