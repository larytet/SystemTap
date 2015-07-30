/* COVERAGE: dup dup2 dup3 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
  dup(2);
  //staptest// dup (2) = NNNN
  
  dup(256);
  //staptest// dup (256) = -NNNN (EBADF)
  
  dup(-1);
  //staptest// dup (-1) = -NNNN (EBADF)

  dup2(3, 4);
  //staptest// [[[[dup2 (3, 4!!!!dup3 (3, 4, 0x0]]]]) = 4

  dup2(255, 256);
  //staptest// [[[[dup2 (255, 256!!!!dup3 (255, 256, 0x0]]]]) = -NNNN (EBADF)

  /* weird corner case oldfd == newfd */
  dup2(1, 1);
  //staptest// [[[[dup2 (1, 1!!!!fcntl (1, F_GETFL, 0x0]]]]) = 1

  dup2(-1, 4);
  //staptest// [[[[dup2 (-1, 4!!!!dup3 (-1, 4, 0x0]]]]) = -NNNN (EBADF)

  dup2(3, -1);
  //staptest// [[[[dup2 (3, -1!!!!dup3 (3, -1, 0x0]]]]) = -NNNN (EBADF)

#ifdef O_CLOEXEC
  dup3 (4, 5, O_CLOEXEC);
  //staptest// dup3 (4, 5, O_CLOEXEC) = 5

  dup3 (256, 255, O_CLOEXEC);
  //staptest// dup3 (256, 255, O_CLOEXEC) = -NNNN (EBADF)
  
  dup3 (5, 6, 666);
  //staptest// dup3 (5, 6, O_[^ ]+|XXXX) = -NNNN (EINVAL)

  /* corner case not valid for dup3 */
  dup3 (1, 1, O_CLOEXEC);
  //staptest// dup3 (1, 1, O_CLOEXEC) = -NNNN (EINVAL)

  dup3 (-1, 7, 0);
  //staptest// dup3 (-1, 7, 0x0) = -NNNN (EBADF)

  dup3 (3, -1, O_CLOEXEC);
  //staptest// dup3 (3, -1, O_CLOEXEC) = -NNNN (EBADF)

  dup3 (3, 7, -1);
  //staptest// dup3 (3, 7, O_[^ ]+|XXXX) = -NNNN (EINVAL)
#endif

  return 0;
}
