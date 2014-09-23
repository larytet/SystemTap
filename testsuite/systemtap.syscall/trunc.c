/* COVERAGE: ftruncate truncate */

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
#if __WORDSIZE == 64
  //staptest// ftruncate (NNNN, 18446744073709551615) = NNNN
#else
  //staptest// ftruncate (NNNN, 4294967295) = NNNN
#endif

  close(fd);

  truncate("foobar", 2048);
  //staptest// truncate ("foobar", 2048) = 0

  truncate((char *)-1, 2048);
#ifdef __s390__
  //staptest// truncate ([7]?[f]+, 2048) = -NNNN (EFAULT)
#else
  //staptest// truncate ([f]+, 2048) = -NNNN (EFAULT)
#endif

  truncate("foobar", -1);
#if __WORDSIZE == 64
  //staptest// truncate ("foobar", 18446744073709551615) = NNNN
#else
  //staptest// truncate ("foobar", 4294967295) = NNNN
#endif

  return 0;
}
