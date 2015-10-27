/* COVERAGE: open openat close creat */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

// To test for glibc support for openat():
//
// Since glibc 2.10:
//	_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L
// Before glibc 2.10:
//	_ATFILE_SOURCE

#define GLIBC_SUPPORT \
  (_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L \
   || defined(_ATFILE_SOURCE))

int main()
{
  int fd1, fd2;

  fd2 = creat("foobar1",S_IREAD|S_IWRITE);
  //staptest// [[[[[[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar1", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?|O_TRUNC!!!!creat ("foobar1"]]]], 0600) = NNNN

  creat((char *)-1, S_IREAD|S_IWRITE);
#ifdef __s390__
  //staptest// [[[[open (0x[7]?[f]+, O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?|O_TRUNC!!!!creat (0x[7]?[f]+]]]], 0600) = NNNN
#else
  //staptest// [[[[[[[[open (!!!!openat (AT_FDCWD, ]]]]0x[f]+, O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?|O_TRUNC!!!!creat (0x[f]+]]]], 0600) = NNNN
#endif

  creat("foobarX", -1);
  //staptest// [[[[[[[[open (!!!!openat (AT_FDCWD, ]]]]"foobarX", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?|O_TRUNC!!!!creat ("foobarX"]]]], 037777777777) =

  fd1 = open("foobar2",O_WRONLY|O_CREAT, S_IRWXU);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar2", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?, 0700) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0

#if GLIBC_SUPPORT
  fd1 = openat(AT_FDCWD, "foobar2", O_WRONLY|O_CREAT, S_IRWXU);
  //staptest// openat (AT_FDCWD, "foobar2", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?, 0700) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0
#endif

  fd1 = open("foobar2",O_RDONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar2", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0

#if GLIBC_SUPPORT
  fd1 = openat(AT_FDCWD, "foobar2", O_RDONLY);
  //staptest// openat (AT_FDCWD, "foobar2", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0
#endif

  fd1 = open("foobar2",O_RDWR);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar2", O_RDWR[[[[.O_LARGEFILE]]]]?) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0

#if GLIBC_SUPPORT
  fd1 = openat(AT_FDCWD, "foobar2", O_RDWR);
  //staptest// openat (AT_FDCWD, "foobar2", O_RDWR[[[[.O_LARGEFILE]]]]?) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0
#endif

  fd1 = open("foobar2",O_APPEND|O_WRONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar2", O_WRONLY|O_APPEND[[[[.O_LARGEFILE]]]]?) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0

#if GLIBC_SUPPORT
  fd1 = openat(AT_FDCWD, "foobar2", O_APPEND|O_WRONLY);
  //staptest// openat (AT_FDCWD, "foobar2", O_WRONLY|O_APPEND[[[[.O_LARGEFILE]]]]?) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0
#endif

  fd1 = open("foobar2",O_DIRECT|O_RDWR);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar2", O_RDWR|O_DIRECT[[[[.O_LARGEFILE]]]]?) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0

#if GLIBC_SUPPORT
  fd1 = openat(AT_FDCWD, "foobar2", O_DIRECT|O_RDWR);
  //staptest// openat (AT_FDCWD, "foobar2", O_RDWR|O_DIRECT[[[[.O_LARGEFILE]]]]?) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0
#endif

  fd1 = open("foobar2",O_NOATIME|O_SYNC|O_RDWR);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar2", O_RDWR[[[[.O_LARGEFILE]]]]?|O_SYNC|O_NOATIME) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0

#if GLIBC_SUPPORT
  fd1 = openat(AT_FDCWD, "foobar2", O_NOATIME|O_SYNC|O_RDWR);
  //staptest// openat (AT_FDCWD, "foobar2", O_RDWR[[[[.O_LARGEFILE]]]]?|O_SYNC|O_NOATIME) = NNNN
  close(fd1);
  //staptest// close (NNNN) = 0
#endif

  /* Now test some bad opens */
  fd1 = open("/",O_WRONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/", O_WRONLY[[[[.O_LARGEFILE]]]]?) = -NNNN (EISDIR)
  close (fd1);
  //staptest// close (NNNN) = -NNNN (EBADF)

#if GLIBC_SUPPORT
  fd1 = openat(AT_FDCWD, "/", O_WRONLY);
  //staptest// openat (AT_FDCWD, "/", O_WRONLY[[[[.O_LARGEFILE]]]]?) = -NNNN (EISDIR)
  close (fd1);
  //staptest// close (NNNN) = -NNNN (EBADF)
#endif

  fd1 = open("foobar2",O_WRONLY|O_CREAT|O_EXCL, S_IRWXU);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar2", O_WRONLY|O_CREAT|O_EXCL[[[[.O_LARGEFILE]]]]?, 0700) = -NNNN (EEXIST)

#if GLIBC_SUPPORT
  fd1 = openat(AT_FDCWD, "foobar2",O_WRONLY|O_CREAT|O_EXCL, S_IRWXU);
  //staptest// openat (AT_FDCWD, "foobar2", O_WRONLY|O_CREAT|O_EXCL[[[[.O_LARGEFILE]]]]?, 0700) = -NNNN (EEXIST)
#endif

  open((char *)-1, O_WRONLY|O_CREAT|O_EXCL, S_IRWXU);
#ifdef __s390__
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]0x[7]?[f]+, O_WRONLY|O_CREAT|O_EXCL[[[[.O_LARGEFILE]]]]?, 0700) =
#else
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]0x[f]+, O_WRONLY|O_CREAT|O_EXCL[[[[.O_LARGEFILE]]]]?, 0700) =
#endif

#if GLIBC_SUPPORT
  openat(-1, "foobarX", O_WRONLY|O_CREAT|O_EXCL, S_IRWXU);
  //staptest// openat (-1, "foobarX", O_WRONLY|O_CREAT|O_EXCL[[[[.O_LARGEFILE]]]]?, 0700) =

  openat(AT_FDCWD, (char *)-1, O_WRONLY|O_CREAT|O_EXCL, S_IRWXU);
#ifdef __s390__
  //staptest// openat (AT_FDCWD, 0x[7]?[f]+, O_WRONLY|O_CREAT|O_EXCL[[[[.O_LARGEFILE]]]]?, 0700) =
#else
  //staptest// openat (AT_FDCWD, 0x[f]+, O_WRONLY|O_CREAT|O_EXCL[[[[.O_LARGEFILE]]]]?, 0700) =
#endif
#endif

  open("foobarX", -1, S_IRWXU);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobarX", O_[^ ]+|XXXX, 0700) =

  fd1 = open("foobarY", O_WRONLY|O_CREAT, -1);
  //staptest// [[[open (!!!!openat (AT_FDCWD, ]]]]"foobarY", O_WRONLY|O_CREAT[[[[|O_LARGEFILE]]]]?, 037777777777) = NNNN

  close(fd1);
  //staptest// close (NNNN) = 0

#if GLIBC_SUPPORT
  openat(AT_FDCWD, "foobarX", -1, S_IRWXU);
  //staptest// openat (AT_FDCWD, "foobarX", O_[^ ]+|XXXX, 0700) =

  fd1 = openat(AT_FDCWD, "foobarZ", O_WRONLY|O_CREAT, -1);
  //staptest// openat (AT_FDCWD, "foobarZ", O_WRONLY|O_CREAT[[[[|O_LARGEFILE]]]]?, 037777777777) = NNNN

  close(fd1);
  //staptest// close (NNNN) = 0
#endif

  close(-1);
  //staptest// close (-1) = -NNNN (EBADF)

  return 0;
}
