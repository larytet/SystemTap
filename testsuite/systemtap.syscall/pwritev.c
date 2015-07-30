/* COVERAGE: pwritev */
#define _GNU_SOURCE
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
#include <sys/syscall.h>

int main()
{
#ifdef __NR_pwritev
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
  pwritev(fd, wr_iovec, 1, 0);
  //staptest// pwritev (NNNN, XXXX, 1, 0x0) = -NNNN (EINVAL)

  pwritev(-1, wr_iovec, 1, 0);
  //staptest// pwritev (-1, XXXX, 1, 0x0) = -NNNN (EBADF)

  pwritev(fd, wr_iovec, -1, 0);
  //staptest// pwritev (NNNN, XXXX, -1, 0x0) = -NNNN (EINVAL)

  pwritev(fd, wr_iovec, 0, 0);
  //staptest// pwritev (NNNN, XXXX, 0, 0x0) = 0

  wr_iovec[0].iov_base = buf;
  wr_iovec[0].iov_len = sizeof(buf);
  wr_iovec[1].iov_base = buf;
  wr_iovec[1].iov_len = 0;
  wr_iovec[2].iov_base = NULL;
  wr_iovec[2].iov_len = 0;
  pwritev(fd, wr_iovec, 3, 0);
  //staptest// pwritev (NNNN, XXXX, 3, 0x0) = 64

  pwritev(fd, wr_iovec, 3, 64);
  //staptest// pwritev (NNNN, XXXX, 3, 0x40) = 64

  close (fd);
  //staptest// close (NNNN) = 0
#endif

  return 0;
}
