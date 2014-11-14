/* COVERAGE: unlink unlinkat */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// To test for glibc support for unlinkat(), symlinkat(), readlinkat():
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
  close (fd1);
  
#if GLIBC_SUPPORT
  fd1 = creat("foobar2", S_IREAD|S_IWRITE);
  close (fd1);
#endif

  unlink("foobar1");
  //staptest// [[[[unlink ("foobar1"!!!!unlinkat (AT_FDCWD, "foobar1", 0x0]]]]) = 0

#if GLIBC_SUPPORT
  unlinkat(AT_FDCWD, "foobar2", 0);
  //staptest// unlinkat (AT_FDCWD, "foobar2", 0x0) = 0
#endif

  unlink("foobar1");
  //staptest// [[[[unlink ("foobar1"!!!!unlinkat (AT_FDCWD, "foobar1", 0x0]]]]) = -NNNN (ENOENT)

#if GLIBC_SUPPORT
  unlinkat(AT_FDCWD, "foobar1", 0);
  //staptest// unlinkat (AT_FDCWD, "foobar1", 0x0) = -NNNN (ENOENT)
#endif

  unlink("foobar2");
  //staptest// [[[[unlink ("foobar2"!!!!unlinkat (AT_FDCWD, "foobar2", 0x0]]]]) = -NNNN (ENOENT)

#if GLIBC_SUPPORT
  unlinkat(AT_FDCWD, "foobar2", 0);
  //staptest// unlinkat (AT_FDCWD, "foobar2", 0x0) = -NNNN (ENOENT)
#endif

  unlink(0);
  //staptest// [[[[unlink ( *(null)!!!!unlinkat (AT_FDCWD,  *(null), 0x0]]]]) = -NNNN (EFAULT)

#if GLIBC_SUPPORT
  unlinkat(AT_FDCWD, 0, 0);
  //staptest// unlinkat (AT_FDCWD, *(null), 0x0) = -NNNN (EFAULT)
#endif

  unlink("..");
  //staptest// [[[[unlink (".."!!!!unlinkat (AT_FDCWD, "..", 0x0]]]]) = -NNNN (EISDIR)

#if GLIBC_SUPPORT
  unlinkat(AT_FDCWD, "..", 0);
  //staptest// unlinkat (AT_FDCWD, "..", 0x0) = -NNNN (EISDIR)
#endif

  unlink("");
  //staptest// [[[[unlink (""!!!!unlinkat (AT_FDCWD, "", 0x0]]]]) = -NNNN (ENOENT)

#if GLIBC_SUPPORT
  unlinkat(AT_FDCWD, "", 0);
  //staptest// unlinkat (AT_FDCWD, "", 0x0) = -NNNN (ENOENT)
#endif

  return 0;
}
