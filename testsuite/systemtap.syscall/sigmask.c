/* COVERAGE: sgetmask ssetmask */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>

// For these obsolete syscalls, there are no glibc wrappers, so create
// our own.

#ifdef SYS_sgetmask
static inline long __sgetmask(void)
{
    return syscall(SYS_sgetmask);
}
#endif

#ifdef SYS_ssetmask
static inline long __ssetmask(long newmask)
{
    return syscall(SYS_ssetmask, newmask);
}
#endif

int main()
{
#if defined(SYS_sgetmask) && defined(SYS_ssetmask)
    long orig_mask;

    orig_mask = __sgetmask();
    //staptest// sgetmask () = NNNN

    __ssetmask(sigmask(SIGUSR1));
    //staptest// ssetmask (\[SIGUSR1\]) = NNNN

    /* Limit testing */
    __ssetmask(-1);
    //staptest// ssetmask (\[SIGHUP|SIG[^ ]+\]) = NNNN

    /* Restore original signal mask. */
    __ssetmask(orig_mask);
    //staptest// ssetmask (\[.*\]) = NNNN
#endif

    return 0;
}
