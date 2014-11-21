/* COVERAGE: pipe pipe2 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
  int pipefd[2];
  pipefd[0] = 0;
  pipefd[1] = 0;

  pipe (pipefd);
  //staptest// [[[[pipe (\[0, 0\]!!!!pipe2 (\[0, 0\], 0x0]]]]) = 0

#ifdef O_CLOEXEC
  /* Test calling pipe2() with 0, to make sure we don't confuse it
   * with pipe(). */
  pipe2 (pipefd, 0);
  //staptest// pipe2 (\[NNNN, NNNN\], 0x0) = 0

  pipe2 (pipefd, O_CLOEXEC);
  //staptest// pipe2 (\[NNNN, NNNN\], O_CLOEXEC) = 0

  pipe2 (pipefd, O_CLOEXEC|O_NONBLOCK);
  //staptest// pipe2 (\[NNNN, NNNN\], O_NONBLOCK|O_CLOEXEC) = 0

  pipe2 ((int *)-1, O_CLOEXEC|O_NONBLOCK);
  //staptest// pipe2 (\[0, 0\], O_NONBLOCK|O_CLOEXEC) = -NNNN

  pipe2 (pipefd, -1);
  //staptest// pipe2 (\[NNNN, NNNN\], O_[^ ]+|XXXX) = -NNNN
#endif

  return 0;
}
