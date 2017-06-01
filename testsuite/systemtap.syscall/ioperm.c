/* COVERAGE: ioperm iopl */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#ifdef __NR_ioperm
static inline int __ioperm(unsigned long from, unsigned long num, int turn_on)
{
    return syscall(__NR_ioperm, from, num, turn_on);
}
#endif

#ifdef __NR_iopl
static inline int __iopl(int level)
{
    return syscall(__NR_iopl, level);
}
#endif

// ENOSYS expected on s390 (31-on-64) and on powerpc

int main() {

#ifdef __NR_ioperm
    __ioperm(1060, 1, 1);
#if defined(__powerpc__) || defined(__s390__)
    //staptest// ni_syscall () = -38 (ENOSYS)
#else
    //staptest// ioperm (0x424, 0x1, 0x1) = NNNN
#endif

    __ioperm((unsigned long)-1, 1, 1);
#if defined(__powerpc__) || defined(__s390__)
    //staptest// ni_syscall () = -38 (ENOSYS)
#else
#if __WORDSIZE == 64
    //staptest// ioperm (0xffffffffffffffff, 0x1, 0x1) = NNNN
#else
    //staptest// ioperm (0xffffffff, 0x1, 0x1) = NNNN
#endif
#endif

    __ioperm(1060, (unsigned long)-1, 1);
#if defined(__powerpc__) || defined(__s390__)
    //staptest// ni_syscall () = -38 (ENOSYS)
#else
#if __WORDSIZE == 64
    //staptest// ioperm (0x424, 0xffffffffffffffff, 0x1) = NNNN
#else
    //staptest// ioperm (0x424, 0xffffffff, 0x1) = NNNN
#endif
#endif

    __ioperm(1060, 1, -1);
#if defined(__powerpc__) || defined(__s390__)
    //staptest// ni_syscall () = -38 (ENOSYS)
#else
    //staptest// ioperm (0x424, 0x1, 0xffffffff) = NNNN
#endif
#endif /* __NR_ioperm */

#if defined(__i386__) || defined(__x86_64__)
    // NOTE. This function is only in i386 and x86_64 and 
    // its args vary between those two archs. Not all are
    // being addressed in the tapset.

    __iopl(3);
    //staptest// iopl (3) = NNNN

    __iopl(-1);
    //staptest// iopl (4294967295) = NNNN
#endif

    return 0;
}
