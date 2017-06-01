/* COVERAGE: getcwd fstat stat lstat fstatat fstatat64 utime */
/* COVERAGE: fstat64 stat64 lstat64 */
/* COVERAGE: newfstat newfstatat newlstat newstat */
/* COVERAGE: statx */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <utime.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef __NR_statx
/* For struct statx */
#include <linux/stat.h>
/* The AT_STATX_FORCE_SYNC is defined within linux/fcntl.h,
 * but including that would cause conflict with already included fcntl.h
 */
#ifndef AT_STATX_FORCE_SYNC
#define AT_STATX_FORCE_SYNC 0x2000
#endif
#endif

// To test for glibc support for fstatat():
//
// Since glibc 2.10:
//	_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L
// Before glibc 2.10:
//	_ATFILE_SOURCE

#define GLIBC_SUPPORT \
  (_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L \
   || defined(_ATFILE_SOURCE))

#ifdef __NR_statx
static inline
ssize_t statx(int dfd, const char *filename, unsigned flags,
              unsigned int mask, struct statx *buffer)
{
        return syscall(__NR_statx, dfd, filename, flags, mask, buffer);
}
#endif

int main()
{
  int fd;
  char cwd[128];
  struct stat sbuf;
  struct utimbuf ubuf;
#ifdef __NR_statx
  struct statx stx;
#endif

  getcwd(cwd, 128);
  //staptest// getcwd (XXXX, 128) = NNNN

  fd = creat("foobar", S_IREAD|S_IWRITE);
  //staptest// [[[[open ("foobar", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?|O_TRUNC!!!!creat ("foobar"!!!!openat (AT_FDCWD, "foobar", O_WRONLY|O_CREAT|O_TRUNC]]]], 0600) = NNNN

  fstat(fd, &sbuf);
  //staptest// fstat (NNNN, XXXX) = 0

  fstat(-1, &sbuf);
  //staptest// fstat (-1, XXXX) = -NNNN (EBADF)

  // Here we specify -1 to both arguments, to avoid a SIGSEGV.
  fstat(-1, (struct stat *)-1);
#if __WORDSIZE != 64
  // Notice we're not checking for 0x[f]+ here for the 2nd
  // argument. On RHEL[67] {x86_64,s390x}, for a 32-bit exe, glibc
  // substituted a real structure address (verified with strace).
  // staptest// fstat (-1, XXXX) = -NNNN
#else
  //staptest// fstat (-1, 0x[f]+) = -NNNN
#endif

#ifdef __NR_statx
  memset(&stx, 0xbf, sizeof(stx));
  statx(AT_FDCWD, "foobar", AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, &stx);
  //staptest// statx (AT_FDCWD, "foobar", AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, XXXX) = 0

  statx((int)-1, "foobar", AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, &stx);
  //staptest// statx (-1, "foobar", AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, XXXX) = -NNNN (EBADF)

  statx(AT_FDCWD, (const char *)-1, AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, &stx);
#ifdef __s390__
  //staptest// statx (AT_FDCWD, 0x[7]?[f]+, AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, XXXX) = -NNNN (EFAULT)
#else
  //staptest// statx (AT_FDCWD, 0x[f]+, AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, XXXX) = -NNNN (EFAULT)
#endif

  statx(AT_FDCWD, "foobar", (unsigned)-1, AT_STATX_FORCE_SYNC, &stx);
  //staptest// statx (AT_FDCWD, "foobar", AT_SYMLINK_NOFOLLOW|AT_REMOVEDIR|AT_SYMLINK_FOLLOW|AT_NO_AUTOMOUNT|AT_EMPTY_PATH|XXXX, AT_STATX_FORCE_SYNC, XXXX) = -NNNN (EINVAL)

  statx(AT_FDCWD, "foobar", AT_SYMLINK_NOFOLLOW, (unsigned)-1, &stx);
  //staptest// statx (AT_FDCWD, "foobar", AT_SYMLINK_NOFOLLOW, 0x[f]+, XXXX) = NNNN

  statx(AT_FDCWD, "foobar", AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, (struct statx *)-1);
#ifdef __s390__
  //staptest// statx (AT_FDCWD, "foobar", AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// statx (AT_FDCWD, "foobar", AT_SYMLINK_NOFOLLOW, AT_STATX_FORCE_SYNC, 0x[f]+) = -NNNN (EFAULT)
#endif
#endif

  close(fd);

  stat("foobar", &sbuf);
  //staptest// [[[[stat ("foobar", XXXX!!!!fstatat (AT_FDCWD, "foobar", XXXX, 0x0]]]]) = 0

  stat((char *)-1, &sbuf);
#if defined(__s390__)
  //staptest// stat (0x[7]?[f]+, XXXX) = -NNNN
#else
  //staptest// [[[[stat (0x[f]+, XXXX!!!!fstatat (AT_FDCWD, 0x[f]+, XXXX, 0x0]]]]) = -NNNN
#endif

  // Here we specify -1 to both arguments, to avoid a SIGSEGV.
  stat((char *)-1, (struct stat *)-1);
#if __WORDSIZE != 64
  // Notice we're not checking for 0x[f]+ here for the 2nd
  // argument. On RHEL[67] {x86_64,s390x}, for a 32-bit exe, glibc
  // substituted a real structure address (verified with strace).
  //staptest// stat (0x[7]?[f]+, XXXX) = -NNNN
#else
  //staptest// [[[[stat (0x[f]+, 0x[f]+!!!!fstatat (AT_FDCWD, 0x[f]+, 0x[f]+, 0x0]]]]) = -NNNN
#endif

  lstat("foobar", &sbuf);
  //staptest// [[[[lstat ("foobar", XXXX!!!!fstatat (AT_FDCWD, "foobar", XXXX, AT_SYMLINK_NOFOLLOW]]]]) = 0

  lstat((char *)-1, &sbuf);
#if defined(__s390__)
  //staptest// lstat (0x[7]?[f]+, XXXX) = -NNNN (EFAULT)
#else
  //staptest// [[[[lstat (0x[f]+, XXXX!!!!fstatat (AT_FDCWD, 0x[f]+, XXXX, AT_SYMLINK_NOFOLLOW]]]]) = -NNNN (EFAULT)
#endif

  // Here we specify -1 to both arguments, to avoid a SIGSEGV.
  lstat((char *)-1, (struct stat *)-1);
#if __WORDSIZE != 64
  // Notice we're not checking for 0x[f]+ here for the 2nd
  // argument. On RHEL[67] {x86_64,s390x}, for a 32-bit exe, glibc
  // substituted a real structure address (verified with strace).
  //staptest// lstat (0x[7]?[f]+, XXXX) = -NNNN
#else
  //staptest// [[[[lstat (0x[f]+, 0x[f]+!!!!fstatat (AT_FDCWD, 0x[f]+, 0x[f]+, AT_SYMLINK_NOFOLLOW]]]]) = -NNNN
#endif

#if GLIBC_SUPPORT
  fstatat(AT_FDCWD, "foobar", &sbuf, AT_SYMLINK_NOFOLLOW);
  //staptest// fstatat (AT_FDCWD, "foobar", XXXX, AT_SYMLINK_NOFOLLOW) = 0

  fstatat(-1, "foobar", &sbuf, AT_SYMLINK_NOFOLLOW);
  //staptest// fstatat (-1, "foobar", XXXX, AT_SYMLINK_NOFOLLOW) = -NNNN (EBADF)

  fstatat(AT_FDCWD, (char *)-1, &sbuf, AT_SYMLINK_NOFOLLOW);
#if defined(__s390__)
  //staptest// fstatat (AT_FDCWD, 0x[7]?[f]+, XXXX, AT_SYMLINK_NOFOLLOW) = -NNNN (EFAULT)
#else
  //staptest// fstatat (AT_FDCWD, 0x[f]+, XXXX, AT_SYMLINK_NOFOLLOW) = -NNNN (EFAULT)
#endif

  // Try to avoid a SIGSEGV.
  fstatat(-1, "foobar", (struct stat *)-1, AT_SYMLINK_NOFOLLOW);
#if __WORDSIZE != 64
  // Notice we're not checking for 0x[f]+ here for the 3rd
  // argument. On RHEL[67] {x86_64,s390x}, for a 32-bit exe, glibc
  // substituted a real structure address (verified with strace).
  //staptest// fstatat (-1, "foobar", XXXX, AT_SYMLINK_NOFOLLOW) = -NNNN
#else
  //staptest// fstatat (-1, "foobar", 0x[f]+, AT_SYMLINK_NOFOLLOW) = -NNNN
#endif

  fstatat(AT_FDCWD, "foobar", &sbuf, -1);
  //staptest// fstatat (AT_FDCWD, "foobar", XXXX, AT_[^ ]+|XXXX) = -NNNN (EINVAL)
#endif

  ubuf.actime = 1;
  ubuf.modtime = 1135641600;
  utime("foobar", &ubuf);
#if defined(__aarch64__)
  //staptest// utimensat (AT_FDCWD, "foobar", \[1.[0]+\]\[1135641600.[0]+\], 0x0) =
#elif defined(__ia64__) || defined(__arm__)
  //staptest// utimes ("foobar", \[1.000000\]\[1135641600.000000\]) =
#else
  //staptest// utime ("foobar", \[Thu Jan  1 00:00:01 1970, Tue Dec 27 00:00:00 2005\]) = 0
#endif

  ubuf.actime =  1135690000;
  ubuf.modtime = 1135700000;
  utime("foobar", &ubuf);
#if defined(__aarch64__)
  //staptest// utimensat (AT_FDCWD, "foobar", \[1135690000.[0]+\]\[1135700000.[0]+\], 0x0) =
#elif defined(__ia64__) || defined(__arm__)
  //staptest// utimes ("foobar", \[1135690000.000000\]\[1135700000.000000\]) =
#else
  //staptest// utime ("foobar", \[Tue Dec 27 13:26:40 2005, Tue Dec 27 16:13:20 2005\]) = 0
#endif

  ubuf.actime = 1;
  ubuf.modtime = 1135641600;
  utime((char *)-1, &ubuf);
#if defined(__aarch64__)
  //staptest// utimensat (AT_FDCWD, 0x[f]+, \[1.[0]+\]\[1135641600.[0]+\], 0x0) = -NNNN
#elif defined(__ia64__) || defined(__arm__)
  //staptest// utimes (0x[f]+, \[1.000000\]\[1135641600.000000\]) = -NNNN
#elif defined(__s390__)
  //staptest// utime (0x[7]?[f]+, \[Thu Jan  1 00:00:01 1970, Tue Dec 27 00:00:00 2005\]) = -NNNN
#else
  //staptest// utime (0x[f]+, \[Thu Jan  1 00:00:01 1970, Tue Dec 27 00:00:00 2005\]) = -NNNN
#endif

#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  // Avoid a SIGSEGV by specifying NULL, not -1.
  utime("foobar", (struct utimbuf *)NULL);
  //staptest// [[[[utimes ("foobar", NULL!!!!utimensat (AT_FDCWD, "foobar", NULL, 0x0]]]]) = NNNN
#else
  utime("foobar", (struct utimbuf *)-1);
  //staptest// utime ("foobar", \[Thu Jan  1 00:00:00 1970, Thu Jan  1 00:00:00 1970\]) = -NNNN
#endif


  return 0;
}
