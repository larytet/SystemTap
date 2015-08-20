/* COVERAGE: signalfd signalfd4 */

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#if defined(__NR_signalfd) || defined(__NR_signalfd4)
#include <sys/signalfd.h>

int main()
{
  sigset_t mask;
  int sfd;

  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGQUIT);
  sfd = signalfd(-1, &mask, 0);
  //staptest// signalfd (-1, XXXX, NNNN) = NNNN

  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  sfd = signalfd(sfd, &mask, 0);
  //staptest// signalfd (NNNN, XXXX, NNNN) = NNNN

#if defined(SFD_NONBLOCK) && defined(SFD_CLOEXEC)
  sfd = signalfd(-1, &mask, SFD_NONBLOCK);
  //staptest// signalfd4 (-1, XXXX, NNNN, SFD_NONBLOCK) = NNNN
  sfd = signalfd(-1, &mask, SFD_CLOEXEC);
  //staptest// signalfd4 (-1, XXXX, NNNN, SFD_CLOEXEC) = NNNN
  sfd = signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
  //staptest// signalfd4 (-1, XXXX, NNNN, SFD_NONBLOCK|SFD_CLOEXEC) = NNNN
#endif

  signalfd(sfd, (sigset_t *)-1, 0);
#ifdef __s390__
  //staptest// signalfd (NNNN, 0x[7]?[f]+, NNNN) = NNNN
#else
  //staptest// signalfd (NNNN, 0x[f]+, NNNN) = NNNN
#endif

  signalfd(-1, &mask, -1);
  //staptest// signalfd4 (-1, XXXX, NNNN, SFD_[^ ]+|XXXX) = NNNN

  return 0;
}
#else
int main()
{
  return 0;
}
#endif
