/* COVERAGE: pwrite pwrite64 */
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

  pwrite(fd, "Hello Again", 11, 12);
  //staptest// pwrite (NNNN, "Hello Again", 11, 12) = 11

  pwrite(-1, "Hello Again", 11, 12);
  //staptest// pwrite (-1, "Hello Again", 11, 12) = NNNN

  pwrite(fd, (void *)-1, 11, 12);
#ifdef __s390__
  //staptest// pwrite (NNNN, 0x[7]?[f]+, 11, 12) = NNNN
#else
  //staptest// pwrite (NNNN, 0x[f]+, 11, 12) = NNNN
#endif

  /* We need to be careful when writing -1 bytes. */
  pwrite(-1, NULL, -1, 12);
#if __WORDSIZE == 64
  //staptest// pwrite (-1, *0x0, 18446744073709551615, 12) = NNNN
#else
  //staptest// pwrite (-1, *0x0, 4294967295, 12) = NNNN
#endif

  pwrite(fd, "Hello Again", 11, -1);
  //staptest// pwrite (NNNN, "Hello Again", 11, -1) = NNNN

  pwrite(-1, "Hello Again", 11, 0x12345678deadbeefLL);
  //staptest// pwrite (-1, "Hello Again", 11, 1311768468603649775) = NNNN

  pwrite(-1, "Hello Again", 11, LLONG_MAX);
  //staptest// pwrite (-1, "Hello Again", 11, 9223372036854775807) = NNNN

  close (fd);
  //staptest// close (NNNN) = 0

  return 0;
}
