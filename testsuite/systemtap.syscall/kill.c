/* COVERAGE: kill */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>

// To test for glibc support for kill:
//           _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _POSIX_SOURCE

#define GLIBC_SUPPORT \
    (_POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _POSIX_SOURCE)

int main()
{
  sigset_t mask;
  struct sigaction sa;
  pid_t pid;

  // Ignore SIGUSR1
  signal(SIGUSR1, SIG_IGN);

  pid = getpid();

  // Be sure we're in our own process group.
  setpgid(0, 0);

#ifdef GLIBC_SUPPORT
  kill(pid, SIGUSR1);
  //staptest// kill (NNNN, SIGUSR1) = 0

  // Normally We couldn't call kill() with -1 for a pid, since that
  // would send a signal to the make process and kill it. However,
  // since we also use an invalid signal number, this shouldn't cause
  // that problem.
  kill(-1, -1);
  //staptest// kill (-1, 0xffffffff) = -NNNN

  kill(pid, -1);
  //staptest// kill (NNNN, 0xffffffff) = -NNNN
#endif
  return 0;
}
