/* COVERAGE: signal tgkill tkill sigprocmask sigaction */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>

// For signal() and sigprocmask(), glibc substitutes rt_signal() and
// rt_sigprocmask(). To ensure we're calling the right syscalls, we
// have to create our own syscall wrappers. For other syscalls tested
// here (like tgkill), there are no glibc syscall wrappers.

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

static void 
sig_act_handler(int signo)
{
}

int main()
{
  sigset_t mask;
  struct sigaction sa;
  pid_t pid;

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
  //staptest// sigaction (SIGUSR1, {UNKNOWN}, 0x0+) = -NNNN (EFAULT)
#else
  //staptest// sigaction (SIGUSR1, {UNKNOWN}, 0x0+) = -NNNN (EFAULT)
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

  return 0;
}
