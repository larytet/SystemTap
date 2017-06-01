/* COVERAGE: seccomp */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#ifdef __NR_seccomp

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <linux/signal.h>
#include <sys/ptrace.h>

// Some versions of <linux/seccomp.h> don't define SECCOMP_SET_MODE_FILTER.
#ifndef SECCOMP_SET_MODE_FILTER
#define SECCOMP_SET_MODE_FILTER 1
#endif

struct sock_filter filter[] = {
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
};

struct sock_fprog prog = {
   .len = (unsigned short) (sizeof(filter) / sizeof(filter[0])),
   .filter = filter,
};

static inline int __seccomp(unsigned int operation, unsigned int flags, void *args)
{
    return syscall(__NR_seccomp, operation, flags, args);
}

int main()
{
    __seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog);
    //staptest// [[[[seccomp (SECCOMP_SET_MODE_FILTER, 0x0, XXXX)!!!!ni_syscall ()]]]] = NNNN

    // Limit testing

    __seccomp(-1, 0, NULL);
    //staptest// [[[[seccomp (0x[f]+, 0x0, 0x0)!!!!ni_syscall ()]]]] = -NNNN

    __seccomp(SECCOMP_SET_MODE_FILTER, -1, NULL);
    //staptest// [[[[seccomp (SECCOMP_SET_MODE_FILTER, 0x[f]+, 0x0)!!!!ni_syscall ()]]]] = -NNNN

    __seccomp(SECCOMP_SET_MODE_FILTER, 0, (void *)-1);
#ifdef __s390__
    //staptest// [[[[seccomp (SECCOMP_SET_MODE_FILTER, 0x0, 0x[7]?[f]+)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[seccomp (SECCOMP_SET_MODE_FILTER, 0x0, 0x[f]+)!!!!ni_syscall ()]]]] = -NNNN
#endif

    return 0;
}
#else
int main()
{
    return 0;
}
#endif
