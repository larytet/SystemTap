/* COVERAGE: eventfd eventfd2 */
#include <sys/eventfd.h>
#include <sys/syscall.h>

int main()
{
  int fd = eventfd(0, 0);
  //staptest// eventfd[2]? (0[[[[, 0x0]]]]?) = NNNN

#ifdef EFD_NONBLOCK
  fd = eventfd(1, EFD_NONBLOCK);
  //staptest// eventfd2 (1, EFD_NONBLOCK) = NNNN

  fd = eventfd(2, EFD_CLOEXEC);
  //staptest// eventfd2 (2, EFD_CLOEXEC) = NNNN

  fd = eventfd(3, EFD_NONBLOCK|EFD_CLOEXEC);
  //staptest// eventfd2 (3, EFD_NONBLOCK|EFD_CLOEXEC) = NNNN
#endif

  fd = eventfd(-1, 0);
  //staptest// eventfd[2]? (4294967295[[[[, 0x0]]]]?) = NNNN

  fd = eventfd(4, -1);
  //staptest// eventfd2 (4, EFD_[^ ]+|XXXX) = -NNNN (EINVAL)

  // Try to force an eventfd() (as opposed to a eventfd2()) syscall.
#ifdef SYS_eventfd
  syscall(SYS_eventfd, 5);
  //staptest// eventfd (5) = NNNN

  syscall(SYS_eventfd, -1);
  //staptest// eventfd (4294967295) = NNNN
#endif
  return 0;
}
