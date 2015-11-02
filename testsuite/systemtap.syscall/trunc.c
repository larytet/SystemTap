/* COVERAGE: ftruncate ftruncate64 truncate truncate64 */

#define _LARGEFILE64_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main()
{
  int fd;

  fd = creat("foobar", S_IREAD|S_IWRITE);

  ftruncate(fd, 1024);
  //staptest// ftruncate (NNNN, 1024) = 0

  ftruncate(-1, 1024);
  //staptest// ftruncate (-1, 1024) = -NNNN (EBADF)

  ftruncate(fd, -1);
  //staptest// ftruncate (NNNN, -1) = NNNN

  /* Force a 64-bit number here. */
  ftruncate64(fd, (off64_t)0x1FFFFFFF2LL);
  //staptest// ftruncate (NNNN, 8589934578) = 0

  ftruncate64(-1, (off64_t)0x7FFFFFFFFFFFFFFFLL);
  //staptest// ftruncate (-1, 9223372036854775807) = NNNN

  ftruncate64(fd, (off64_t)-1LL);
  //staptest// ftruncate (NNNN, -1) = NNNN

  close(fd);

  truncate("foobar", 2048);
  //staptest// truncate ("foobar", 2048) = 0

  truncate((char *)-1, 2048);
#ifdef __s390__
  //staptest// truncate (0x[7]?[f]+, 2048) = -NNNN (EFAULT)
#else
  //staptest// truncate (0x[f]+, 2048) = -NNNN (EFAULT)
#endif

  truncate("foobar", -1);
  //staptest// truncate ("foobar", -1) = NNNN

  /* Force a 64-bit number here. */
  truncate64("foobar", (off64_t)0x1FFFFFFF2LL);
  //staptest// truncate ("foobar", 8589934578) = 0

  truncate64((char *)-1, (off64_t)0x7FFFFFFFFFFFFFFFLL);
#ifdef __s390__
  //staptest// truncate (0x[7]?[f]+, 9223372036854775807) = NNNN
#else
  //staptest// truncate (0x[f]+, 9223372036854775807) = NNNN
#endif

  truncate64("foobar", (off64_t)-1LL);
  //staptest// truncate ("foobar", -1) = NNNN

  return 0;
}
