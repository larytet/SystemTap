/* COVERAGE: pread pread64 */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _LARGEFILE64_SOURCE
#define _ISOC99_SOURCE		   /* Needed for LLONG_MAX on RHEL5 */
#define _FILE_OFFSET_BITS 64
#define _XOPEN_SOURCE 500
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <linux/unistd.h>
#include <sys/uio.h>
#include <sys/syscall.h>

#define STRING1 "red"
#define STRING2 "green"
#define STRING3 "blue"
int main()
{
  int fd;
  loff_t res;
  char buf[64], buf1[32], buf2[32], buf3[32];

  fd = open("foobar1", O_WRONLY|O_CREAT, 0666);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar1", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?, 0666) = NNNN

  write(fd, "Hello world abcdefghijklmnopqrstuvwxyz 01234567890123456789", 59);
  //staptest// write (NNNN, "Hello world abcdefghijklmnopqrstuvwxyz 012345"..., 59) = 59

  close (fd);
  //staptest// close (NNNN) = 0

  fd = open("foobar1", O_RDONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar1", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN

  pread(fd, buf, 11, 10);
  //staptest// pread (NNNN, XXXX, 11, 10) = 11

  pread(-1, buf, 11, 10);
  //staptest// pread (-1, XXXX, 11, 10) = NNNN

  pread(fd, (void *)-1, 11, 10);
#ifdef __s390__
  //staptest// pread (NNNN, 0x[7]?[f]+, 11, 10) = NNNN
#else
  //staptest// pread (NNNN, 0x[f]+, 11, 10) = NNNN
#endif

  /* We need to be careful when reading -1 bytes. */
  pread(-1, NULL, -1, 10);
#if __WORDSIZE == 64
  //staptest// pread (-1, 0x0, 18446744073709551615, 10) = -NNNN
#else
  //staptest// pread (-1, 0x0, 4294967295, 10) = -NNNN
#endif

  pread(fd, buf, 11, -1);
  //staptest// pread (NNNN, XXXX, 11, -1) = NNNN

  pread(fd, buf, 11, 0x12345678deadbeefLL);
  //staptest// pread (NNNN, XXXX, 11, 1311768468603649775) = NNNN

  pread(fd, buf, 11, LLONG_MAX);
  //staptest// pread (NNNN, XXXX, 11, 9223372036854775807) = NNNN

  close (fd);
  //staptest// close (NNNN) = 0

  return 0;
}
