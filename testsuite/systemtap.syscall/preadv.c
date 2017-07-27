/* COVERAGE: preadv preadv2 */
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

#ifdef __NR_preadv2
#define LO_HI_LONG(val) \
  (unsigned long) val, \
  (unsigned long) ((((u_int64_t) (val)) >> (sizeof (long) * 4)) >> (sizeof (long) * 4))

static inline ssize_t
__preadv2(int fd, const struct iovec *iov, int iovcnt, loff_t offset, int flags)
{
    return syscall(__NR_preadv2, fd, iov, iovcnt,
		   LO_HI_LONG(offset), flags);
}
#endif

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
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"dir1", O_RDONLY[[[[|O_LARGEFILE]]]]?) = NNNN
  
  preadv(fd, rd_iovec, 1, 0);
  //staptest// preadv (NNNN, XXXX, 1, 0x0) = -NNNN (EISDIR)

  close (fd);
  //staptest// close (NNNN) = 0

  preadv(-1, rd_iovec, 1, -1);
  //staptest// preadv (-1, XXXX, 1, 0xffffffffffffffff) = -NNNN

  preadv(-1, rd_iovec, 1, 0x12345678deadbeefLL);
  //staptest// preadv (-1, XXXX, 1, 0x12345678deadbeef) = -NNNN

  preadv(-1, rd_iovec, 1, LLONG_MAX);
  //staptest// preadv (-1, XXXX, 1, 0x7fffffffffffffff) = -NNNN

#ifdef __NR_preadv2
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

  __preadv2(fd, rd_iovec, 0, 0, 0);
  //staptest// preadv2 (NNNN, XXXX, 0, 0x0, 0x0) = 0

  memset(buf, 0x0, sizeof(buf));
  __preadv2(fd, rd_iovec, 3, 0, 0);
  //staptest// preadv2 (NNNN, XXXX, 3, 0x0, 0x0) = 64
  
  memset(buf, 0x0, sizeof(buf));
  __preadv2(fd, rd_iovec, 3, 32, 0);
  //staptest// preadv2 (NNNN, XXXX, 3, 0x20, 0x0) = 32

  rd_iovec[0].iov_len = -1;
  __preadv2(fd, rd_iovec, 1, 0, RWF_HIPRI);
  //staptest// preadv2 (NNNN, XXXX, 1, 0x0, RWF_HIPRI) = -NNNN (EINVAL)

  rd_iovec[0].iov_base = (char *)-1;
  rd_iovec[0].iov_len = sizeof(buf);
  __preadv2(fd, rd_iovec, 1, 0, 0);
  // On 64-bit platforms, we get a EFAULT. On 32-on-64 bits, we
  // typically get a 0.
  //staptest// preadv2 (NNNN, XXXX, 1, 0x0, 0x0) = [[[[0!!!!-NNNN (EFAULT)]]]]

  rd_iovec[0].iov_base = buf;
  rd_iovec[0].iov_len = sizeof(buf);
  __preadv2(-1, rd_iovec, 1, 0, 0);
  //staptest// preadv2 (-1, XXXX, 1, 0x0, 0x0) = -NNNN (EBADF)

  __preadv2(fd, rd_iovec, -1, 0, 0);
  //staptest// preadv2 (NNNN, XXXX, -1, 0x0, 0x0) = -NNNN (EINVAL)

  __preadv2(fd, rd_iovec, 1, 0, -1);
  //staptest// preadv2 (NNNN, XXXX, 1, 0x0, RWF_[^ ]+|XXXX) = -NNNN

  close (fd);
  //staptest// close (NNNN) = 0

  __preadv2(-1, rd_iovec, 1, -1, 0);
  //staptest// preadv2 (-1, XXXX, 1, 0xffffffffffffffff, 0x0) = -NNNN

  __preadv2(-1, rd_iovec, 1, 0x12345678deadbeefLL, 0);
  //staptest// preadv2 (-1, XXXX, 1, 0x12345678deadbeef, 0x0) = -NNNN

  __preadv2(-1, rd_iovec, 1, LLONG_MAX, 0);
  //staptest// preadv2 (-1, XXXX, 1, 0x7fffffffffffffff, 0x0) = -NNNN
#endif
#endif

  return 0;
}
