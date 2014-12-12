/* COVERAGE: ioperm */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

// ENOSYS expected on s390 (31-on-64) and on powerpc

int main() {

#ifdef __NR_ioperm
    syscall(__NR_ioperm, 1060, 1, 1);
#if defined(__powerpc__) || defined(__s390__)
    //staptest// ni_syscall () = -38 (ENOSYS)
#else
    //staptest// ioperm (0x424, 0x1, 0x1) = NNNN
#endif

    syscall(__NR_ioperm, (unsigned long)-1, 1, 1);
#if defined(__powerpc__) || defined(__s390__)
    //staptest// ni_syscall () = -38 (ENOSYS)
#else
#if __WORDSIZE == 64
    //staptest// ioperm (0xffffffffffffffff, 0x1, 0x1) = NNNN
#else
    //staptest// ioperm (0xffffffff, 0x1, 0x1) = NNNN
#endif
#endif

    syscall(__NR_ioperm, 1060, (unsigned long)-1, 1);
#if defined(__powerpc__) || defined(__s390__)
    //staptest// ni_syscall () = -38 (ENOSYS)
#else
#if __WORDSIZE == 64
    //staptest// ioperm (0x424, 0xffffffffffffffff, 0x1) = NNNN
#else
    //staptest// ioperm (0x424, 0xffffffff, 0x1) = NNNN
#endif
#endif

    syscall(__NR_ioperm, 1060, 1, (int)-1);
#if defined(__powerpc__) || defined(__s390__)
    //staptest// ni_syscall () = -38 (ENOSYS)
#else
    //staptest// ioperm (0x424, 0x1, 0xffffffff) = NNNN
#endif
#endif /* __NR_ioperm */

    return 0;
}
