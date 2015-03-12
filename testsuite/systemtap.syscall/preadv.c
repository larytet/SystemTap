/* COVERAGE: preadv */
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
#ifdef __NR_preadv
  int fd;
  struct iovec rd_iovec[3];
  char buf[64];

  fd = open("foobar1", O_WRONLY|O_CREAT, 0666);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar1", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?, 0666) = NNNN
  memset(buf, (int)'B', sizeof(buf));
  write(fd, buf, sizeof(buf));
  //staptest// write (NNNN, "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"..., 64) = 64

  close(fd);
  //staptest// close (NNNN) = 0

  fd = open("foobar1", O_RDONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar1", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN

  rd_iovec[0].iov_base = buf;
  rd_iovec[0].iov_len = sizeof(buf);
  rd_iovec[1].iov_base = NULL;
  rd_iovec[1].iov_len = 0;
  rd_iovec[2].iov_base = NULL;
  rd_iovec[2].iov_len = 0;

  preadv(fd, rd_iovec, 0, 0);
  //staptest// preadv (NNNN, XXXX, 0, 0x0) = 0

  memset(buf, 0x0, sizeof(buf));
  preadv(fd, rd_iovec, 3, 0);
  //staptest// preadv (NNNN, XXXX, 3, 0x0) = 64
  
  memset(buf, 0x0, sizeof(buf));
  preadv(fd, rd_iovec, 3, 32);
  //staptest// preadv (NNNN, XXXX, 3, 0x20) = 32

  rd_iovec[0].iov_len = -1;
  preadv(fd, rd_iovec, 1, 0);
  //staptest// preadv (NNNN, XXXX, 1, 0x0) = -NNNN (EINVAL)

  rd_iovec[0].iov_base = (char *)-1;
  rd_iovec[0].iov_len = sizeof(buf);
  preadv(fd, rd_iovec, 1, 0);
  // On 64-bit platforms, we get a EFAULT. On 32-on-64 bits, we
  // typically get a 0.
  //staptest// preadv (NNNN, XXXX, 1, 0x0) = [[[[0!!!!-NNNN (EFAULT)]]]]

  rd_iovec[0].iov_base = buf;
  rd_iovec[0].iov_len = sizeof(buf);
  preadv(-1, rd_iovec, 1, 0);
  //staptest// preadv (-1, XXXX, 1, 0x0) = -NNNN (EBADF)

  preadv(fd, rd_iovec, -1, 0);
  //staptest// preadv (NNNN, XXXX, -1, 0x0) = -NNNN (EINVAL)

  close (fd);
  //staptest// close (NNNN) = 0

  mkdir("dir1", S_IREAD|S_IWRITE|S_IEXEC);
  //staptest// [[[[mkdir (!!!!mkdirat (AT_FDCWD, ]]]]"dir1", 0700) = 0

  fd = open("dir1", O_RDONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"dir1", O_RDONLY) = NNNN
  
  preadv(fd, rd_iovec, 1, 0);
  //staptest// preadv (NNNN, XXXX, 1, 0x0) = -NNNN (EISDIR)

  close (fd);
  //staptest// close (NNNN) = 0
#endif

  return 0;
}
