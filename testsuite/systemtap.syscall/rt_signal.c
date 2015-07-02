/* COVERAGE:  rt_sigprocmask rt_sigaction rt_sigpending rt_sigqueueinfo */
/* COVERAGE:  rt_sigsuspend rt_tgsigqueueinfo rt_sigtimedwait */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <string.h>
#include <signal.h>
#include <linux/version.h>

// Calling rt_tgsigqueueinfo() from a 32-bit executable on RHEL6 s390x
// (2.6.32-554.el6.s390x) causes a kernel crash. So, don't test
// rt_tgsigqueueinfo() on kernels less than version 3.0.

#if defined(__NR_rt_tgsigqueueinfo) && defined(__s390__) \
    && LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
#undef __NR_rt_tgsigqueueinfo
#endif

static void 
sig_act_handler(int signo)
{
}

static inline int
__rt_sigqueueinfo(pid_t tgid, int sig, siginfo_t *siginfo)
{
  return syscall(__NR_rt_sigqueueinfo, tgid, sig, siginfo);
}

// rt_tgsigqueueinfo() was added in kernel 2.6.31.
#ifdef __NR_rt_tgsigqueueinfo
static inline int
__rt_tgsigqueueinfo(pid_t tgid, pid_t tid, int sig, siginfo_t *siginfo)
{
  return syscall(__NR_rt_tgsigqueueinfo, tgid, tid, sig, siginfo);
}
#endif

int main()
{
  sigset_t mask, mask2, omask, omask2;
  siginfo_t siginfo;
  struct sigaction sa;
  struct timespec timeout;

  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  sigprocmask(SIG_BLOCK, &mask, &omask);
  //staptest// rt_sigprocmask (SIG_BLOCK, \[SIGUSR1|SIGUSR2\], XXXX, 8) = 0

  sigdelset(&mask, SIGUSR2);
  sigprocmask(SIG_UNBLOCK, &mask, &omask2);
  //staptest// rt_sigprocmask (SIG_UNBLOCK, \[SIGUSR1\], XXXX, 8) = 0

  // glibc handles this one
  //sigprocmask(-1, &mask, NULL);

  // Causes a SIGSEGV
  //sigprocmask(SIG_BLOCK, (sigset_t *)-1, NULL);

  sigprocmask(SIG_BLOCK, &mask, (sigset_t *)-1);
#ifdef __s390__
  //staptest// rt_sigprocmask (SIG_BLOCK, \[SIGUSR1\], 0x[7]?[f]+, 8) = -NNNN (EFAULT)
#else
  //staptest// rt_sigprocmask (SIG_BLOCK, \[SIGUSR1\], 0x[f]+, 8) = -NNNN (EFAULT)
#endif

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGALRM);
  sa.sa_flags = 0;
  sigaction(SIGUSR1, &sa, NULL);
  //staptest// rt_sigaction (SIGUSR1, {SIG_IGN}, 0x[0]+, 8) = 0

  sa.sa_handler = SIG_DFL;
  sigaction(SIGUSR1, &sa, NULL);
  //staptest// rt_sigaction (SIGUSR1, {SIG_DFL}, 0x[0]+, 8) = 0
  
  sigaction(-1, &sa, NULL);
  //staptest// rt_sigaction (0x[f]+, {SIG_DFL}, 0x[0]+, 8) = -NNNN

  // Causes a SIGSEGV
  //sigaction(SIGUSR1, (struct sigaction *)-1, NULL);

  // Causes a SIGSEGV
  //sigaction(SIGUSR1, &sa, (struct sigaction *)-1);

  sa.sa_handler = sig_act_handler;
  sigaction(SIGUSR1, &sa, NULL);
#ifdef __ia64__
  //staptest// rt_sigaction (SIGUSR1, {XXXX, [^,]+, \[SIGALRM\]}, 0x[0]+, 8) = 0
#else
  //staptest// rt_sigaction (SIGUSR1, {XXXX, [^,]+, XXXX, \[SIGALRM\]}, 0x[0]+, 8) = 0
#endif

  sigpending(&mask);
  //staptest// rt_sigpending (XXXX, 8) = 0

  sigpending((sigset_t *) 0);
  //staptest// rt_sigpending (0x0, 8) = -NNNN (EFAULT)

  sigpending((sigset_t *)-1);
#ifdef __s390__
  //staptest// rt_sigpending (0x[7]?[f]+, 8) = -NNNN (EFAULT)
#else
  //staptest// rt_sigpending (0x[f]+, 8) = -NNNN (EFAULT)
#endif

  siginfo.si_signo = SIGUSR1;
  siginfo.si_code = SI_USER;
  siginfo.si_pid = getpid();
  siginfo.si_uid = getuid();
  siginfo.si_value.sival_int = 1;

  __rt_sigqueueinfo(getpid(), SIGUSR1, &siginfo);
  //staptest// rt_sigqueueinfo (NNNN, SIGUSR1, {si_signo=SIGUSR1, si_code=SI_USER, si_pid=NNNN, si_uid=NNNN}) = NNNN

  __rt_sigqueueinfo(-1, SIGUSR1, &siginfo);
  //staptest// rt_sigqueueinfo (-1, SIGUSR1, {si_signo=SIGUSR1, si_code=SI_USER, si_pid=NNNN, si_uid=NNNN}) = NNNN

  __rt_sigqueueinfo(getpid(), -1, &siginfo);
  //staptest// rt_sigqueueinfo (NNNN, 0x[f]+, {si_signo=SIGUSR1, si_code=SI_USER, si_pid=NNNN, si_uid=NNNN}) = NNNN

  __rt_sigqueueinfo(getpid(), SIGUSR1, (siginfo_t *)-1);
#ifdef __s390__
  //staptest// rt_sigqueueinfo (NNNN, SIGUSR1, 0x[7]?[f]+) = NNNN
#else
  //staptest// rt_sigqueueinfo (NNNN, SIGUSR1, 0x[f]+) = NNNN
#endif

#ifdef __NR_rt_tgsigqueueinfo
  memset(&siginfo, 0, sizeof(siginfo));
  siginfo.si_signo = SIGUSR1;
  siginfo.si_code = SI_USER;
  siginfo.si_pid = getpid();
  siginfo.si_uid = getuid();
  siginfo.si_value.sival_int = 1;
  __rt_tgsigqueueinfo(getpid(), getpid(), SIGUSR1, &siginfo);
  //staptest// [[[[rt_tgsigqueueinfo (NNNN, NNNN, SIGUSR1, {si_signo=SIGUSR1, si_code=SI_USER, si_pid=NNNN, si_uid=NNNN})!!!!ni_syscall ()]]]] = NNNN

  __rt_tgsigqueueinfo(-1, getpid(), SIGUSR1, &siginfo);
  //staptest// [[[[rt_tgsigqueueinfo (-1, NNNN, SIGUSR1, {si_signo=SIGUSR1, si_code=SI_USER, si_pid=NNNN, si_uid=NNNN})!!!!ni_syscall ()]]]] = NNNN

  __rt_tgsigqueueinfo(getpid(), -1, SIGUSR1, &siginfo);
  //staptest// [[[[rt_tgsigqueueinfo (NNNN, -1, SIGUSR1, {si_signo=SIGUSR1, si_code=SI_USER, si_pid=NNNN, si_uid=NNNN})!!!!ni_syscall ()]]]] = NNNN

  __rt_tgsigqueueinfo(getpid(), getpid(), -1, &siginfo);
  //staptest// [[[[rt_tgsigqueueinfo (NNNN, NNNN, 0x[f]+, {si_signo=SIGUSR1, si_code=SI_USER, si_pid=NNNN, si_uid=NNNN})!!!!ni_syscall ()]]]] = NNNN

  __rt_tgsigqueueinfo(getpid(), getpid(), SIGUSR1, (siginfo_t *)-1);
#ifdef __s390__
  //staptest// [[[[rt_tgsigqueueinfo (NNNN, NNNN, SIGUSR1, 0x[7]?[f]+)!!!!ni_syscall ()]]]] = NNNN
#else
  //staptest// [[[[rt_tgsigqueueinfo (NNNN, NNNN, SIGUSR1, 0x[f]+)!!!!ni_syscall ()]]]] = NNNN
#endif
#endif

  // Restore the original signal masks.
  sigprocmask(SIG_BLOCK, &omask, NULL);
  //staptest// rt_sigprocmask (SIG_BLOCK, \[EMPTY\], 0x0, 8) = NNNN

  sigprocmask(SIG_UNBLOCK, &omask2, NULL);
  //staptest// rt_sigprocmask (SIG_UNBLOCK, \[SIGUSR1|SIGUSR2\], 0x0, 8) = NNNN

  sigemptyset(&mask);
  sigemptyset(&mask2);
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_act_handler;
  sigaction(SIGALRM, &sa, NULL);
#ifdef __ia64__
  //staptest// rt_sigaction (SIGALRM, {XXXX, 0x0, \[EMPTY\]}, 0x[0]+, 8) = 0
#else
  //staptest// rt_sigaction (SIGALRM, {XXXX, [^,]+, XXXX, \[EMPTY\]}, 0x[0]+, 8) = 0
#endif

  sigprocmask(SIG_UNBLOCK, &mask2, NULL);
  //staptest// rt_sigprocmask (SIG_UNBLOCK, \[EMPTY\], 0x[0]+, 8) = 0

  alarm(5);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,5.000000\], XXXX) = NNNN
#else
  //staptest// alarm (5) = NNNN
#endif

  sigsuspend(&mask);
  //staptest// rt_sigsuspend (\[EMPTY\], 8) = NNNN

  alarm(0);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,0.000000\], XXXX) = NNNN
#else
  //staptest// alarm (0) = NNNN
#endif

  sigsuspend((sigset_t *)-1);
#ifdef __s390__
  //staptest// rt_sigsuspend (0x[7]?[f]+, 8) = NNNN
#else
  //staptest// rt_sigsuspend (0x[f]+, 8) = NNNN
#endif

  alarm(5);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,5.000000\], XXXX) = NNNN
#else
  //staptest// alarm (5) = NNNN
#endif

  timeout.tv_sec = 6;
  timeout.tv_nsec = 0;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  memset(&siginfo, 0, sizeof(siginfo));
  siginfo.si_signo = SIGALRM;
  siginfo.si_code = SI_USER;
  siginfo.si_pid = getpid();
  siginfo.si_uid = getuid();
  siginfo.si_value.sival_int = 1;

  sigtimedwait(&mask, &siginfo, &timeout);
  //staptest// rt_sigtimedwait (\[SIGALRM\], {si_signo=SIGALRM, si_code=SI_USER, si_pid=NNNN, si_uid=NNNN}, \[6.000000000\], 8) = NNNN

  alarm(0);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,0.000000\], XXXX) = NNNN
#else
  //staptest// alarm (0) = NNNN
#endif

  // Causes a SIGSEGV...
  //sigtimedwait((sigset_t *)-1, &siginfo, &timeout);

  sigtimedwait(&mask, (siginfo_t *)-1, &timeout);
#ifdef __s390__
  //staptest// rt_sigtimedwait (\[SIGALRM\], 0x[7]?[f]+, \[6.000000000\], 8) = NNNN
#else
  //staptest// rt_sigtimedwait (\[SIGALRM\], 0x[f]+, \[6.000000000\], 8) = NNNN
#endif

  sigtimedwait(&mask, &siginfo, (struct timespec *)-1);
#ifdef __s390__
  //staptest// rt_sigtimedwait (\[SIGALRM\], {si_signo=SIGALRM, si_code=SI_KERNEL}, 0x[7]?[f]+, 8) = NNNN
#else
  //staptest// rt_sigtimedwait (\[SIGALRM\], {si_signo=SIGALRM, si_code=SI_KERNEL}, 0x[f]+, 8) = NNNN
#endif

  return 0;
}

