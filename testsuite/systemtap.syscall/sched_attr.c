/* COVERAGE: sched_getattr sched_setattr */

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

#if defined __NR_sched_getattr && defined __NR_sched_setattr

static inline int __sched_getattr(pid_t pid, void *attr, unsigned int size,
                                  unsigned int flags)
{
    return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

static inline int __sched_setattr(pid_t pid, void *attr, unsigned int flags)
{
    return syscall(__NR_sched_setattr, pid, attr, flags);
}

int main()
{
    int mypid = getpid();

    // Declaration of struct sched_attr not in available in 4.1.0-0 kernel headers.
    // Following avoids declaring the structure within this test program.
    int sas = 1024;
    int sal = 8; /* __alignof__ (struct sched_attr) */
    void *sa = memalign(sal, sas);

    __sched_getattr(mypid, sa, sas, 0);
    //staptest// [[[[sched_getattr (NNNN, {size=NNNN, sched_policy=NNNN, sched_flags=NNNN, sched_nice=NNNN, sched_priority=NNNN, sched_runtime=NNNN, sched_deadline=NNNN, sched_period=NNNN}, NNNN, NNNN)!!!!ni_syscall ()]]]] = NNNN

    __sched_setattr(mypid, sa, 0);
    //staptest// [[[[sched_setattr (NNNN, {size=NNNN, sched_policy=NNNN, sched_flags=NNNN, sched_nice=NNNN, sched_priority=NNNN, sched_runtime=NNNN, sched_deadline=NNNN, sched_period=NNNN}, 0)!!!!ni_syscall ()]]]] = NNNN

    // Limit testing

    __sched_getattr(-1, 0, 0, 0);
    //staptest// [[[[sched_getattr (-1, NULL, 0, 0)!!!!ni_syscall ()]]]] = -NNNN

    __sched_getattr(0, (void *)-1, 0, 0);
#ifdef __s390__
    //staptest// [[[[sched_getattr (0, 0x[7]?[f]+, 0, 0)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[sched_getattr (0, 0x[f]+, 0, 0)!!!!ni_syscall ()]]]] = -NNNN
#endif

    __sched_getattr(0, 0, -1, 0);
    //staptest// [[[[sched_getattr (0, NULL, 4294967295, 0)!!!!ni_syscall ()]]]] = -NNNN

    __sched_getattr(0, 0, 0, -1);
    //staptest// [[[[sched_getattr (0, NULL, 0, 4294967295)!!!!ni_syscall ()]]]] = -NNNN

    __sched_setattr(-1, NULL, 0);
    //staptest// [[[[sched_setattr (-1, NULL, 0)!!!!ni_syscall ()]]]] = -NNNN

    __sched_setattr(0, (void *)-1, 0);
#ifdef __s390__
    //staptest// [[[[sched_setattr (0, 0x[7]?[f]+, 0)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[sched_setattr (0, 0x[f]+, 0)!!!!ni_syscall ()]]]] = -NNNN
#endif

    __sched_setattr(0, NULL, -1);
    //staptest// [[[[sched_setattr (0, NULL, 4294967295)!!!!ni_syscall ()]]]] = -NNNN

    free(sa);

    return 0;
}
#else
int main()
{
    return 0;
}
#endif
