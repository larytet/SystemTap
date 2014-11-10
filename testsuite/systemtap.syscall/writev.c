/* COVERAGE: writev */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/uio.h>
#include <string.h>

int main()
{
  int fd;
  struct iovec wr_iovec[3];
  char buf[64];

  fd = open("foobar1", O_WRONLY|O_CREAT, 0666);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar1", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?, 0666) = NNNN

  memset(buf, (int)'B', sizeof(buf));
  wr_iovec[0].iov_base = buf;
  wr_iovec[0].iov_len = -1;
  wr_iovec[1].iov_base = NULL;
  wr_iovec[1].iov_len = 0;
  wr_iovec[2].iov_base = NULL;
  wr_iovec[2].iov_len = 0;
  writev(fd, wr_iovec, 1);
  //staptest// writev (NNNN, XXXX, 1) = -NNNN (EINVAL)

  writev(-1, wr_iovec, 1);
  //staptest// writev (-1, XXXX, 1) = -NNNN (EBADF)

  writev(fd, wr_iovec, -1);
  //staptest// writev (NNNN, XXXX, -1) = -NNNN (EINVAL)

  writev(fd, wr_iovec, 0);
  //staptest// writev (NNNN, XXXX, 0) = 0

  wr_iovec[0].iov_base = buf;
  wr_iovec[0].iov_len = sizeof(buf);
  wr_iovec[1].iov_base = buf;
  wr_iovec[1].iov_len = 0;
  wr_iovec[2].iov_base = NULL;
  wr_iovec[2].iov_len = 0;
  writev(fd, wr_iovec, 3);
  //staptest// writev (NNNN, XXXX, 3) = 64

  close (fd);
  //staptest// close (NNNN) = 0

  return 0;
}
