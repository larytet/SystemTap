/* COVERAGE: read write */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
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

  write(fd, "Hello world", 11);
  //staptest// write (NNNN, "Hello world", 11) = 11

  write(fd, "Hello world abcdefghijklmnopqrstuvwxyz 01234567890123456789", 59);
  //staptest// write (NNNN, "Hello world abcdefghijklmnopqrstuvwxyz 012345"..., 59) = 59

  write(-1, "Hello world", 11);
  //staptest// write (-1, "Hello world", 11) = NNNN

  write(fd, (void *)-1, 11);
#ifdef __s390__
  //staptest// write (NNNN, 0x[7]?[f]+, 11) = NNNN
#else
  //staptest// write (NNNN, 0x[f]+, 11) = NNNN
#endif

  /* We need to be careful when writing -1 bytes. */
  write(-1, NULL, -1);
#if __WORDSIZE == 64
  //staptest// write (-1, *0x0, 18446744073709551615) = -NNNN
#else
  //staptest// write (-1, *0x0, 4294967295) = -NNNN
#endif

  close (fd);
  //staptest// close (NNNN) = 0

  fd = open("foobar1", O_RDONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar1", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN

  read(fd, buf, 11);
  //staptest// read (NNNN, XXXX, 11) = 11

  read(fd, buf, 50);
  //staptest// read (NNNN, XXXX, 50) = 50

  read(-1, buf, 50);
  //staptest// read (-1, XXXX, 50) = NNNN

  read(fd, (void *)-1, 50);
#ifdef __s390__
  //staptest// read (NNNN, 0x[7]?[f]+, 50) = NNNN
#else
  //staptest// read (NNNN, 0x[f]+, 50) = NNNN
#endif

  /* We need to be careful when reading -1 bytes. */
  read(-1, NULL, -1);
#if __WORDSIZE == 64
  //staptest// read (-1, 0x0, 18446744073709551615) = -NNNN
#else
  //staptest// read (-1, 0x0, 4294967295) = -NNNN
#endif

  close (fd);
  //staptest// close (NNNN) = 0

  return 0;
}
