/* COVERAGE: pwritev pwritev2 */
#define _GNU_SOURCE
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _LARGEFILE64_SOURCE
#define _ISOC99_SOURCE		   /* Needed for LLONG_MAX on RHEL5 */
#define _FILE_OFFSET_BITS 64
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/unistd.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <endian.h>
#include <linux/fs.h>

#ifdef __NR_pwritev2
#define LO_HI_LONG(val) \
  (unsigned long) val, \
  (unsigned long) ((((u_int64_t) (val)) >> (sizeof (long) * 4)) >> (sizeof (long) * 4))

static inline ssize_t
__pwritev2(int fd, const struct iovec *iov, int iovcnt, loff_t offset, int flags)
{
    return syscall(__NR_pwritev2, fd, iov, iovcnt,
		   LO_HI_LONG(offset), flags);
}
#endif

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

  pwritev(-1, wr_iovec, 3, -1);
  //staptest// pwritev (-1, XXXX, 3, 0xffffffffffffffff) = -NNNN

  pwritev(-1, wr_iovec, 3, 0x12345678deadbeefLL);
  //staptest// pwritev (-1, XXXX, 3, 0x12345678deadbeef) = -NNNN

  pwritev(-1, wr_iovec, 3, LLONG_MAX);
  //staptest// pwritev (-1, XXXX, 3, 0x7fffffffffffffff) = -NNNN

#ifdef __NR_pwritev2
  fd = open("foobar1", O_WRONLY|O_CREAT, 0666);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar1", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?, 0666) = NNNN

  memset(buf, (int)'B', sizeof(buf));
  wr_iovec[0].iov_base = buf;
  wr_iovec[0].iov_len = -1;
  wr_iovec[1].iov_base = NULL;
  wr_iovec[1].iov_len = 0;
  wr_iovec[2].iov_base = NULL;
  wr_iovec[2].iov_len = 0;
  __pwritev2(fd, wr_iovec, 1, 0, 0);
  //staptest// pwritev2 (NNNN, XXXX, 1, 0x0, 0x0) = -NNNN (EINVAL)

  __pwritev2(-1, wr_iovec, 1, 0, RWF_HIPRI);
  //staptest// pwritev2 (-1, XXXX, 1, 0x0, RWF_HIPRI) = -NNNN (EBADF)

  __pwritev2(fd, wr_iovec, -1, 0, 0);
  //staptest// pwritev2 (NNNN, XXXX, -1, 0x0, 0x0) = -NNNN (EINVAL)

  __pwritev2(fd, wr_iovec, 1, 0, -1);
  //staptest// pwritev2 (NNNN, XXXX, 1, 0x0, RWF_[^ ]+|XXXX) = -NNNN

  __pwritev2(fd, wr_iovec, 0, 0, 0);
  //staptest// pwritev2 (NNNN, XXXX, 0, 0x0, 0x0) = 0

  wr_iovec[0].iov_base = buf;
  wr_iovec[0].iov_len = sizeof(buf);
  wr_iovec[1].iov_base = buf;
  wr_iovec[1].iov_len = 0;
  wr_iovec[2].iov_base = NULL;
  wr_iovec[2].iov_len = 0;
  __pwritev2(fd, wr_iovec, 3, 0, 0);
  //staptest// pwritev2 (NNNN, XXXX, 3, 0x0, 0x0) = 64

  __pwritev2(fd, wr_iovec, 3, 64, 0);
  //staptest// pwritev2 (NNNN, XXXX, 3, 0x40, 0x0) = 64

  close (fd);
  //staptest// close (NNNN) = 0

  __pwritev2(-1, wr_iovec, 3, -1, 0);
  //staptest// pwritev2 (-1, XXXX, 3, 0xffffffffffffffff, 0x0) = -NNNN

  __pwritev2(-1, wr_iovec, 3, 0x12345678deadbeefLL, 0);
  //staptest// pwritev2 (-1, XXXX, 3, 0x12345678deadbeef, 0x0) = -NNNN

  __pwritev2(-1, wr_iovec, 3, LLONG_MAX, 0);
  //staptest// pwritev2 (-1, XXXX, 3, 0x7fffffffffffffff, 0x0) = -NNNN
#endif
#endif
  return 0;
}
