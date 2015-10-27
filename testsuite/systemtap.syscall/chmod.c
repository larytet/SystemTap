/* COVERAGE: chmod fchmod fchmodat chown fchown fchownat lchown */
/* COVERAGE: chown16 fchown16 lchown16 */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>

// To test for glibc support for fchmodat(), fchownat():
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
  int fd;

  fd = open("foobar",O_WRONLY|O_CREAT, 0666);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?, 0666) = NNNN

  chmod("foobar", 0644);
  //staptest// [[[[chmod (!!!!fchmodat (AT_FDCWD, ]]]]"foobar", 0644) = 0

  fchmod(fd, 0444);
  //staptest// fchmod (NNNN, 0444) = 0

#if GLIBC_SUPPORT
  fchmodat(AT_FDCWD, "foobar", 0644, 0);
  //staptest// fchmodat (AT_FDCWD, "foobar", 0644) = 0
#endif

  chmod((char *)-1, 0644);
#ifdef __s390__
  //staptest// chmod (0x[7]?[f]+, 0644) = -NNNN (EFAULT)
#else
  //staptest// [[[[chmod (!!!!fchmodat (AT_FDCWD, ]]]]0x[f]+, 0644[[[[, 0x0]]]]?) = -NNNN (EFAULT)
#endif

  fchmod(-1, 0644);
  //staptest// [[[[fchmod (!!!!fchmodat (AT_FDCWD, ]]]]-1, 0644[[[[, 0x0]]]]?) = -NNNN (EBADF)

#if GLIBC_SUPPORT
  fchmodat(AT_FDCWD, (char *)-1, 0644, 0);
#ifdef __s390__
  //staptest// fchmodat (AT_FDCWD, 0x[7]?[f]+, 0644) = -NNNN (EFAULT)
#else
  //staptest// fchmodat (AT_FDCWD, 0x[f]+, 0644) = -NNNN (EFAULT)
#endif
#endif

  chmod("foobar", -1);
  //staptest// [[[[chmod (!!!!fchmodat (AT_FDCWD, ]]]]"foobar", 037777777777) = NNNN

  fchmod(fd, -1);
  //staptest// fchmod (NNNN, 037777777777) = NNNN

#if GLIBC_SUPPORT
  fchmodat(AT_FDCWD, "foobar", -1, 0);
  //staptest// fchmodat (AT_FDCWD, "foobar", 037777777777) = NNNN

  fchmodat(-1, "foobar", 0644, 0);
  //staptest// fchmodat (-1, "foobar", 0644) = -NNNN (EBADF)
#endif


  chown((char *)-1, 5000, 5001);
#ifdef __s390__
  //staptest// chown (0x[7]?[f]+, 5000, 5001) = -NNNN (EFAULT)
#else
  //staptest// [[[[chown (!!!!fchownat (AT_FDCWD, ]]]]0x[f]+, 5000, 5001[[[[, 0x0]]]]?) = -NNNN (EFAULT)
#endif

  chown("foobar", 5000, -1);
  //staptest// [[[[chown (!!!!fchownat (AT_FDCWD, ]]]]"foobar", 5000, -1[[[[, 0x0]]]]?) = NNNN

  chown("foobar", -1, 5001);
  //staptest// [[[[chown (!!!!fchownat (AT_FDCWD, ]]]]"foobar", -1, 5001[[[[, 0x0]]]]?) = NNNN

  fchown(-1, 5002, 5003);
  //staptest// [[[[fchown (!!!!fchownat (AT_FDCWD, ]]]]-1, 5002, 5003[[[[, 0x0]]]]?) = -NNNN (EBADF)

  fchown(fd, 5002, -1);
  //staptest// fchown (NNNN, 5002, -1) = NNNN

#if GLIBC_SUPPORT
  fchownat(-1, "foobar", 5002, -1, 0);
  //staptest// fchownat (-1, "foobar", 5002, -1, 0x0) = -NNNN (EBADF)

  fchownat(AT_FDCWD, (char *)-1, 5002, -1, 0);
#ifdef __s390__
  //staptest// fchownat (AT_FDCWD, 0x[7]?[f]+, 5002, -1, 0x0) = -NNNN (EFAULT)
#else
  //staptest// fchownat (AT_FDCWD, 0x[f]+, 5002, -1, 0x0) = -NNNN (EFAULT)
#endif

  fchownat(AT_FDCWD, "foobar", -1, 5000, 0);
  //staptest// fchownat (AT_FDCWD, "foobar", -1, 5000, 0x0) = NNNN

  fchownat(AT_FDCWD, "foobar", 5002, -1, 0);
  //staptest// fchownat (AT_FDCWD, "foobar", 5002, -1, 0x0) = NNNN
#endif

  fchown(fd, -1, 5003);
  //staptest// fchown (NNNN, -1, 5003) = NNNN

  lchown("foobar", 5004, -1);
  //staptest// [[[[lchown (!!!!fchownat (AT_FDCWD, ]]]]"foobar", 5004, -1[[[[, AT_SYMLINK_NOFOLLOW]]]]?) = NNNN

  lchown("foobar", -1, 5005);
  //staptest// [[[[lchown (!!!!fchownat (AT_FDCWD, ]]]]"foobar", -1, 5005[[[[, AT_SYMLINK_NOFOLLOW]]]]?) = NNNN

  lchown((char *)-1, 5005, 5006);
#ifdef __s390__
  //staptest// lchown (0x[7]?[f]+, 5005, 5006) = -NNNN (EFAULT)
#else
  //staptest// [[[[lchown (!!!!fchownat (AT_FDCWD, ]]]]0x[f]+, 5005, 5006[[[[, AT_SYMLINK_NOFOLLOW]]]]?) = -NNNN (EFAULT)
#endif

#if __WORDSIZE != 64
#ifdef SYS_chown
  syscall(SYS_chown, "foobar", 5000, -1);
  //staptest// chown[[[[16]]]]? ("foobar", 5000, -1) = NNNN
  syscall(SYS_chown, "foobar", -1, 5001);
  //staptest// chown[[[[16]]]]? ("foobar", -1, 5001) = NNNN
#endif

#ifdef SYS_fchown
  syscall(SYS_fchown, fd, 5002, -1);
  //staptest// fchown[[[[16]]]]? (NNNN, 5002, -1) = NNNN
  syscall(SYS_fchown, fd, -1, 5003);
  //staptest// fchown[[[[16]]]]? (NNNN, -1, 5003) = NNNN
#endif

#ifdef SYS_lchown
  syscall(SYS_lchown, "foobar", 5004, -1);
  //staptest// lchown[[[[16]]]]? ("foobar", 5004, -1) = NNNN
  syscall(SYS_lchown, "foobar", -1, 5005);
  //staptest// lchown[[[[16]]]]? ("foobar", -1, 5005) = NNNN
#endif
#endif

  close(fd);
  return 0;
}
