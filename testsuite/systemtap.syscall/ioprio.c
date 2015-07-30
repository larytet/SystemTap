/* COVERAGE: ioprio_set ioprio_get */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#if !defined(__NR_ioprio_set) || !defined(__NR_ioprio_get)
#error "ioprio_[gs]et syscall numbers not defined"
#endif

// There aren't any glibc wrappers for these syscalls, so create our
// own.

static inline int __ioprio_set(int which, int who, int ioprio)
{
    return syscall(__NR_ioprio_set, which, who, ioprio);
}

static inline int __ioprio_get(int which, int who)
{
    return syscall(__NR_ioprio_get, which, who);
}

// These enums/defines were copied from the kernel's
// include/linux/ioprio.h file.
enum {
    IOPRIO_CLASS_NONE,
    IOPRIO_CLASS_RT,
    IOPRIO_CLASS_BE,
    IOPRIO_CLASS_IDLE,
};

enum {
    IOPRIO_WHO_PROCESS = 1,
    IOPRIO_WHO_PGRP,
    IOPRIO_WHO_USER,
};

#define IOPRIO_CLASS_SHIFT	(13)
#define IOPRIO_PRIO_VALUE(class, data)	(((class) << IOPRIO_CLASS_SHIFT) | data)

int main()
{
    __ioprio_get(IOPRIO_WHO_USER, 0);
    //staptest// ioprio_get (IOPRIO_WHO_USER, 0) = NNNN

    __ioprio_set(IOPRIO_WHO_PROCESS, 0, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, 7));
    //staptest// ioprio_set (IOPRIO_WHO_PROCESS, 0, IOPRIO_CLASS_BE|7) = NNNN

    /* Limits testing */

    __ioprio_get(-1, 0);
    //staptest// ioprio_get (-1, 0) = NNNN

    __ioprio_get(IOPRIO_WHO_PGRP, -1);
    //staptest// ioprio_get (IOPRIO_WHO_PGRP, -1) = NNNN

    __ioprio_set(-1, 0, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, 7));
    //staptest// ioprio_set (-1, 0, IOPRIO_CLASS_BE|7) = NNNN

    __ioprio_set(IOPRIO_WHO_PROCESS, -1, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, 7));
    //staptest// ioprio_set (IOPRIO_WHO_PROCESS, -1, IOPRIO_CLASS_BE|7) = NNNN

    __ioprio_set(IOPRIO_WHO_PROCESS, 0, -1);
    //staptest// ioprio_set (IOPRIO_WHO_PROCESS, 0, 0x7ffff|NNNN) = NNNN

    return 0;
}
