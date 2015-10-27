/* COVERAGE: access faccessat */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// To test for glibc support for faccessat():
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
  int fd1;

  fd1 = creat("foobar1",S_IREAD|S_IWRITE);

  access("foobar1", F_OK);
  //staptest// [[[[access (!!!!faccessat (AT_FDCWD, ]]]]"foobar1", F_OK) = 0

#if GLIBC_SUPPORT
  faccessat(AT_FDCWD, "foobar1", F_OK, 0);
  //staptest// faccessat (AT_FDCWD, "foobar1", F_OK) = 0
#endif

  access("foobar1", R_OK);
  //staptest// [[[[access (!!!!faccessat (AT_FDCWD, ]]]]"foobar1", R_OK) = 0

#if GLIBC_SUPPORT
  faccessat(AT_FDCWD, "foobar1", R_OK, 0);
  //staptest// faccessat (AT_FDCWD, "foobar1", R_OK) = 0
#endif

  access("foobar1", W_OK);
  //staptest// [[[[access (!!!!faccessat (AT_FDCWD, ]]]]"foobar1", W_OK) = 0

#if GLIBC_SUPPORT
  faccessat(AT_FDCWD, "foobar1", W_OK, 0);
  //staptest// faccessat (AT_FDCWD, "foobar1", W_OK) = 0
#endif

  access("foobar1", X_OK);
  //staptest// [[[[access (!!!!faccessat (AT_FDCWD, ]]]]"foobar1", X_OK) = -NNNN (EACCES)

#if GLIBC_SUPPORT
  faccessat(AT_FDCWD, "foobar1", X_OK, 0);
  //staptest// faccessat (AT_FDCWD, "foobar1", X_OK) = -NNNN (EACCES)
#endif

  access("foobar1", R_OK|W_OK);
  //staptest// [[[[access (!!!!faccessat (AT_FDCWD, ]]]]"foobar1", R_OK|W_OK) = 0

#if GLIBC_SUPPORT
  faccessat(AT_FDCWD, "foobar1", R_OK|W_OK, 0);
  //staptest// faccessat (AT_FDCWD, "foobar1", R_OK|W_OK) = 0
#endif

  access("foobar1", R_OK|W_OK|X_OK);
  //staptest// [[[[access (!!!!faccessat (AT_FDCWD, ]]]]"foobar1", R_OK|W_OK|X_OK) = -NNNN (EACCES)

#if GLIBC_SUPPORT
  faccessat(AT_FDCWD, "foobar1", R_OK|W_OK|X_OK, 0);
  //staptest// faccessat (AT_FDCWD, "foobar1", R_OK|W_OK|X_OK) = -NNNN (EACCES)
#endif

  access((char *)-1, F_OK);
#ifdef __s390__
  //staptest// access (0x[7]?[f]+, F_OK) = -NNNN (EFAULT)
#else
  //staptest// [[[[access (!!!!faccessat (AT_FDCWD, ]]]]0x[f]+, F_OK) = -NNNN (EFAULT)
#endif

  access("foobar1", -1);
  //staptest// [[[[access (!!!!faccessat (AT_FDCWD, ]]]]"foobar1", R_OK|W_OK|X_OK|0x[f]+8) = -NNNN (EINVAL)

#if GLIBC_SUPPORT
  faccessat(-1, "foobar1", F_OK, 0);
  //staptest// faccessat (-1, "foobar1", F_OK) = -NNNN (EBADF)

  faccessat(AT_FDCWD, (char *)-1, F_OK, 0);
#ifdef __s390__
  //staptest// faccessat (AT_FDCWD, 0x[7]?[f]+, F_OK) = -NNNN (EFAULT)
#else
  //staptest// faccessat (AT_FDCWD, 0x[f]+, F_OK) = -NNNN (EFAULT)
#endif

  faccessat(AT_FDCWD, "foobar1", -1, 0);
  //staptest// faccessat (AT_FDCWD, "foobar1", R_OK|W_OK|X_OK|0x[f]+8) = -NNNN (EINVAL)

  // We can't test the last argument to faccessat() as a -1, since
  // glibc will realize that's wrong and not issue a syscall.
#endif

  return 0;
}
