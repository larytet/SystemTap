/* COVERAGE: signal tgkill sigprocmask sigaction */
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>

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
  syscall(SYS_signal, SIGUSR1, SIG_IGN);
  //staptest// signal (SIGUSR1, SIG_IGN)

  syscall (SYS_signal, SIGUSR1, SIG_DFL);
  //staptest// signal (SIGUSR1, SIG_DFL) = 1

  syscall (SYS_signal, SIGUSR1, sig_act_handler);
  //staptest// signal (SIGUSR1, XXXX) = 0

  syscall (SYS_signal, -1, sig_act_handler);
  //staptest// signal (0x[f]+, XXXX) = NNNN

  syscall (SYS_signal, SIGUSR1, -1);
#ifdef __s390__
  //staptest// signal (SIGUSR1, 0x[7]?[f]+) = NNNN
#else
  //staptest// signal (SIGUSR1, 0x[f]+) = NNNN
#endif
#endif

  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR2);

#ifdef SYS_sigprocmask
  syscall (SYS_sigprocmask, SIG_BLOCK, &mask, NULL);
  /* sigprocmask is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigprocmask (SIG_BLOCK, XXXX, 0x0+) = 0
#endif

  syscall (SYS_sigprocmask, SIG_UNBLOCK, &mask, NULL);
  /* sigprocmask is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigprocmask (SIG_UNBLOCK, XXXX, 0x0+) = 0
#endif

  syscall (SYS_sigprocmask, -1, &mask, NULL);
  /* sigprocmask is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigprocmask (0x[f]+, XXXX, 0x0+) = -NNNN
#endif

  syscall (SYS_sigprocmask, SIG_UNBLOCK, -1, NULL);
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

  syscall (SYS_sigprocmask, SIG_UNBLOCK, &mask, -1);
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
  syscall (SYS_sigaction, SIGUSR1, &sa, NULL);
  /* sigaction is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigaction (SIGUSR1, {SIG_IGN}, 0x0+) = 0
#endif

  syscall (SYS_sigaction, -1, &sa, NULL);
  /* sigaction is unimplemented on powerpc64 */
#ifdef __powerpc64__
  //staptest// ni_syscall () = -38 (ENOSYS)
#else
  //staptest// sigaction (0x[f]+, {SIG_IGN}, 0x0+) = NNNN
#endif

  syscall (SYS_sigaction, SIGUSR1, -1, NULL);
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

  syscall (SYS_sigaction, SIGUSR1, &sa, -1);
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
  syscall(SYS_tgkill, 1234, 5678, 0);
  //staptest// tgkill (1234, 5678, SIG_0) = NNNN

  syscall(SYS_tgkill, -1, 5678, 0);
  //staptest// tgkill (-1, 5678, SIG_0) = NNNN

  syscall(SYS_tgkill, 1234, -1, 0);
  //staptest// tgkill (1234, -1, SIG_0) = NNNN

  syscall(SYS_tgkill, 1234, 5678, -1);
  //staptest// tgkill (1234, 5678, 0x[f]+) = NNNN
#endif

  return 0;
}
