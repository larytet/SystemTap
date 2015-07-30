/* COVERAGE: signal tgkill tkill sigprocmask sigaction */
/* COVERAGE: sigpending sigsuspend */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>

// For signal()/sigprocmask()/sigpending()/sigsuspend(), glibc
// substitutes the 'rt_*' versions.  To ensure we're calling the right
// syscalls, we have to create our own syscall wrappers. For other
// syscalls tested here (like tgkill), there are no glibc syscall
// wrappers.

typedef void (*sighandler_t)(int);

#ifdef SYS_signal
static inline sighandler_t __signal(int signum, sighandler_t handler)
{
    return (sighandler_t)syscall(SYS_signal, signum, handler);
}
#endif

#ifdef SYS_sigprocmask
static inline int __sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    return syscall(SYS_sigprocmask, how, set, oldset);
}
#endif

#ifdef SYS_sigaction
static inline int
__sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    return syscall(SYS_sigaction, signum, act, oldact);
}
#endif

#ifdef SYS_tgkill
static inline int __tgkill(int tgid, int tid, int sig)
{
    return syscall(SYS_tgkill, tgid, tid, sig);
}
#endif

#ifdef SYS_tkill
static inline int __tkill(int tid, int sig)
{
    return syscall(SYS_tkill, tid, sig);
}
#endif

#ifdef SYS_sigpending
static inline int __sigpending(sigset_t *set)
{
    return syscall(SYS_sigpending, set);
}
#endif

#ifdef SYS_sigsuspend
// Notice that the sigsuspend() syscall takes an unsigned long, not a
// pointer to a sigset_t. The rt_sigsuspend() syscall (which glibc
// uses) takes a pointer to a sigset_t.

// This gets more complicated by the fact that there are
// architecture-specific versions of sys_sigsuspend:
//
// #ifdef CONFIG_OLD_SIGSUSPEND
// asmlinkage long sys_sigsuspend(old_sigset_t mask);
// #endif
// #ifdef CONFIG_OLD_SIGSUSPEND3
// asmlinkage long sys_sigsuspend(int unused1, int unused2, old_sigset_t mask);
// #endif
#if defined(__arm__) || defined(__s390__) || defined(__i386__) \
    || defined(__aarch64__)
#define CONFIG_OLD_SIGSUSPEND3
#endif

static inline int __sigsuspend(unsigned long mask)
{
#if defined(CONFIG_OLD_SIGSUSPEND3)
    return syscall(SYS_sigsuspend, 0, 0, mask);
#else
    return syscall(SYS_sigsuspend, mask);
#endif
}
#endif

static void 
sig_act_handler(int signo)
{
}

int main()
{
  sigset_t mask, mask2;
  struct sigaction sa;
  pid_t pid;
  unsigned long old_mask;

#ifdef SYS_signal
  __signal(SIGUSR1, SIG_IGN);
  //staptest// signal (SIGUSR1, SIG_IGN)

  __signal(SIGUSR1, SIG_DFL);
  //staptest// signal (SIGUSR1, SIG_DFL) = 1

  __signal(SIGUSR1, sig_act_handler);
  //staptest// signal (SIGUSR1, XXXX) = 0

  __signal(-1, sig_act_handler);
  //staptest// signal (0x[f]+, XXXX) = NNNN

  __signal(SIGUSR1, (sighandler_t)-1);
#ifdef __s390__
  //staptest// signal (SIGUSR1, 0x[7]?[f]+) = NNNN
#else
  //staptest// signal (SIGUSR1, 0x[f]+) = NNNN
#endif
#endif

  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR2);

#ifdef SYS_sigprocmask
  __sigprocmask(SIG_BLOCK, &mask, NULL);
  /* sigprocmask is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigprocmask (SIG_BLOCK, XXXX, 0x0+) = 0
#endif

  __sigprocmask(SIG_UNBLOCK, &mask, NULL);
  /* sigprocmask is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigprocmask (SIG_UNBLOCK, XXXX, 0x0+) = 0
#endif

  __sigprocmask(-1, &mask, NULL);
  /* sigprocmask is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigprocmask (0x[f]+, XXXX, 0x0+) = -NNNN
#endif

  __sigprocmask(SIG_UNBLOCK, (sigset_t *)-1, NULL);
  /* sigprocmask is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
#ifdef __s390__
  //staptest// sigprocmask (SIG_UNBLOCK, 0x[7]?[f]+, 0x0+) = -NNNN (EFAULT)
#else
  //staptest// sigprocmask (SIG_UNBLOCK, 0x[f]+, 0x0+) = -NNNN (EFAULT)
#endif
#endif

  __sigprocmask(SIG_UNBLOCK, &mask, (sigset_t *)-1);
  /* sigprocmask is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
#ifdef __s390__
  //staptest// sigprocmask (SIG_UNBLOCK, XXXX, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// sigprocmask (SIG_UNBLOCK, XXXX, 0x[f]+) = -NNNN (EFAULT)
#endif
#endif

#endif

  memset(&sa, 0, sizeof(sa));
  sigfillset(&sa.sa_mask);
  sa.sa_handler = SIG_IGN;

#ifdef SYS_sigaction
  __sigaction(SIGUSR1, &sa, NULL);
  /* sigaction is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigaction (SIGUSR1, {SIG_IGN}, 0x0+) = 0
#endif

  __sigaction(-1, &sa, NULL);
  /* sigaction is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigaction (0x[f]+, {SIG_IGN}, 0x0+) = NNNN
#endif

  __sigaction(SIGUSR1, (struct sigaction *)-1, NULL);
  /* sigaction is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
#ifdef __s390__
  //staptest// sigaction (SIGUSR1, \{0x[7]?[f]+\}, 0x0+) = -NNNN (EFAULT)
#else
  //staptest// sigaction (SIGUSR1, \{0x[f]+\}, 0x0+) = -NNNN (EFAULT)
#endif
#endif

  __sigaction(SIGUSR1, &sa, (struct sigaction *)-1);
  /* sigaction is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
#ifdef __s390__
  //staptest// sigaction (SIGUSR1, {SIG_IGN}, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// sigaction (SIGUSR1, {SIG_IGN}, 0x[f]+) = -NNNN (EFAULT)
#endif
#endif
#endif

#ifdef SYS_tgkill
  __tgkill(1234, 5678, 0);
  //staptest// tgkill (1234, 5678, SIG_0) = NNNN

  __tgkill(-1, 5678, 0);
  //staptest// tgkill (-1, 5678, SIG_0) = NNNN

  __tgkill(1234, -1, 0);
  //staptest// tgkill (1234, -1, SIG_0) = NNNN

  __tgkill(1234, 5678, -1);
  //staptest// tgkill (1234, 5678, 0x[f]+) = NNNN
#endif

#ifdef SYS_tkill
  pid = getpid();

  __tkill(pid, 0);
  //staptest// tkill (NNNN, SIG_0) = 0

  __tkill(-1, SIGHUP);
  //staptest// tkill (-1, SIGHUP) = NNNN

  __tkill(-1, -1);
  //staptest// tkill (-1, 0x[f]+) = NNNN
#endif

#ifdef SYS_sigpending
  sigemptyset(&mask);
  __sigpending(&mask);
  //staptest// [[[[sigpending (XXXX)!!!!ni_syscall ()]]]] = NNNN

  __sigpending((sigset_t *) 0);
  //staptest// [[[[sigpending (0x0)!!!!ni_syscall ()]]]] = NNNN

  __sigpending((sigset_t *)-1);
#ifdef __s390__
  //staptest// sigpending (0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// [[[[sigpending (0x[f]+)!!!!ni_syscall ()]]]] = NNNN
#endif
#endif

  /* If we have SYS_sigaction (but not on powerpc where it isn't
   * implemented) and SYS_sigprocmask, we can do SYS_sigsuspend
   * tests. We set up a signal handler for SIGALRM, then send a
   * SIGALRM using alarm(). */
#if defined(SYS_sigsuspend) && defined(SYS_sigaction) \
    && !defined(__powerpc64__) && defined(SYS_sigprocmask)
  sigemptyset(&mask2);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_act_handler;
  __sigaction(SIGALRM, &sa, NULL);
  //staptest// sigaction (SIGALRM, {XXXX, 0x0, 0x0, \[EMPTY\]}, 0x0+) = 0

  __sigprocmask(SIG_UNBLOCK, &mask2, NULL);
  //staptest// sigprocmask (SIG_UNBLOCK, XXXX, 0x0) = 0

  // Create a sigsset_t with SIGUSR1 in it (just so we have something
  // to display), then convert it to an unsigned long, which is what
  // sigsuspend() wants.
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  memcpy(&old_mask, &mask, sizeof(old_mask));

  alarm(5);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,5.000000\], XXXX) = NNNN
#else
  //staptest// alarm (5) = NNNN
#endif

  __sigsuspend(old_mask);
  //staptest// sigsuspend (\[SIGUSR1\]) = NNNN

  alarm(0);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,0.000000\], XXXX) = NNNN
#else
  //staptest// alarm (0) = NNNN
#endif

  // Calling sigsuspend() with -1 waits forever, since that blocks all
  // signals. 
  //__sigsuspend((unsigned long)-1);
#endif

  return 0;
}
