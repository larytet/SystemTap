/* COVERAGE: link linkat symlink symlinkat readlink readlinkat */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// To test for glibc support for linkat(), symlinkat(), readlinkat():
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
  char buf[128];

  fd = open("foobar",O_WRONLY|O_CREAT, S_IRWXU);
  close(fd);

  link("foobar", "foobar2");
  //staptest// [[[[link ("foobar", "foobar2"!!!!linkat (AT_FDCWD, "foobar", AT_FDCWD, "foobar2". 0x0]]]]) = 0

#if GLIBC_SUPPORT
  linkat(AT_FDCWD, "foobar", AT_FDCWD, "foobar3", 0);
  //staptest// linkat (AT_FDCWD, "foobar", AT_FDCWD, "foobar3", 0x0) = 0
#endif

  link("foobar", "foobar");
  //staptest// [[[[link ("foobar", "foobar"!!!!linkat (AT_FDCWD, "foobar", AT_FDCWD, "foobar", 0x0]]]]) = -NNNN (EEXIST)

#if GLIBC_SUPPORT
  linkat(AT_FDCWD, "foobar", AT_FDCWD, "foobar", 0);
  //staptest// linkat (AT_FDCWD, "foobar", AT_FDCWD, "foobar", 0x0) = -NNNN (EEXIST)
#endif

  link("nonexist", "foo");
  //staptest// [[[[link ("nonexist", "foo"!!!!linkat (AT_FDCWD, "nonexist", AT_FDCWD, "foo", 0x0]]]]) = -NNNN (ENOENT)

  link((char *)-1, "foo");
#ifdef __s390__
  //staptest// link (0x[7]?[f]+, "foo") = -NNNN (EFAULT)
#else
  //staptest// [[[[link (0x[f]+, "foo"!!!!linkat (AT_FDCWD, 0x[f]+, AT_FDCWD, "foo", 0x0]]]]) = -NNNN (EFAULT)
#endif

  link("nonexist", (char *)-1);
#ifdef __s390__
  //staptest// link ("nonexist", 0x[7]?[f]+) = -NNNN
#else
  //staptest// [[[[link ("nonexist", 0x[f]+!!!!linkat (AT_FDCWD, "nonexist", AT_FDCWD, 0x[f]+, 0x0]]]]) = -NNNN
#endif

#if GLIBC_SUPPORT
  linkat(AT_FDCWD, "nonexist", AT_FDCWD, "foo", 0);
  //staptest// linkat (AT_FDCWD, "nonexist", AT_FDCWD, "foo", 0x0) = -NNNN (ENOENT)

  linkat(-1, "nonexist", AT_FDCWD, "foo", 0);
  //staptest// linkat (-1, "nonexist", AT_FDCWD, "foo", 0x0) = -NNNN (EBADF)

  linkat(AT_FDCWD, (char *)-1, AT_FDCWD, "foo", 0);
#ifdef __s390__
  //staptest// linkat (AT_FDCWD, 0x[7]?[f]+, AT_FDCWD, "foo", 0x0) = -NNNN (EFAULT)
#else
  //staptest// linkat (AT_FDCWD, 0x[f]+, AT_FDCWD, "foo", 0x0) = -NNNN (EFAULT)
#endif

  linkat(AT_FDCWD, "nonexist", -1, "foo", 0);
  //staptest// linkat (AT_FDCWD, "nonexist", -1, "foo", 0x0) = -NNNN (ENOENT)

  linkat(AT_FDCWD, "nonexist", AT_FDCWD, (char *)-1, 0);
#ifdef __s390__
  //staptest// linkat (AT_FDCWD, "nonexist", AT_FDCWD, 0x[7]?[f]+, 0x0) = -NNNN
#else
  //staptest// linkat (AT_FDCWD, "nonexist", AT_FDCWD, 0x[f]+, 0x0) = -NNNN
#endif

  linkat(AT_FDCWD, "nonexist", AT_FDCWD, "foo", -1);
  //staptest// linkat (AT_FDCWD, "nonexist", AT_FDCWD, "foo", AT_[^ ]+|XXXX) = -NNNN (EINVAL)
#endif

  symlink("foobar", "Sfoobar");
  //staptest// [[[[symlink ("foobar", !!!!symlinkat ("foobar", AT_FDCWD, ]]]]"Sfoobar") = 0

  symlink((char *)-1, "Sfoobar");
#ifdef __s390__
  //staptest// symlink (0x[7]?[f]+, "Sfoobar") = -NNNN (EFAULT)
#else
  //staptest// [[[[symlink (0x[f]+, !!!!symlinkat (0x[f]+, AT_FDCWD, ]]]]"Sfoobar") = -NNNN (EFAULT)
#endif

  symlink("foobar", (char *)-1);
#ifdef __s390__
  //staptest// symlink ("foobar", 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// [[[[symlink ("foobar", !!!!symlinkat ("foobar", AT_FDCWD, ]]]]0x[f]+) = -NNNN (EFAULT)
#endif

#if GLIBC_SUPPORT
  symlinkat("foobar", AT_FDCWD, "Sfoobar2");
  //staptest// symlinkat ("foobar", AT_FDCWD, "Sfoobar2") = 0

  symlinkat((char *)-1, AT_FDCWD, "Sfoobar2");
#ifdef __s390__
  //staptest// symlinkat (0x[7]?[f]+, AT_FDCWD, "Sfoobar2") = -NNNN (EFAULT)
#else
  //staptest// symlinkat (0x[f]+, AT_FDCWD, "Sfoobar2") = -NNNN (EFAULT)
#endif

  symlinkat("foobar", -1, "Sfoobar2");
  //staptest// symlinkat ("foobar", -1, "Sfoobar2") = -NNNN (EBADF)

  symlinkat("foobar", AT_FDCWD, (char *)-1);
#ifdef __s390__
  //staptest// symlinkat ("foobar", AT_FDCWD, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// symlinkat ("foobar", AT_FDCWD, 0x[f]+) = -NNNN (EFAULT)
#endif
#endif

  readlink("Sfoobar", buf, sizeof(buf));
  //staptest// [[[[readlink (!!!!readlinkat (AT_FDCWD, ]]]]"Sfoobar", XXXX, 128) = 6

  readlink((char *)-1, buf, sizeof(buf));
#ifdef __s390__
  //staptest// [[[[readlink (!!!!readlinkat (AT_FDCWD, ]]]]0x[7]?[f]+, XXXX, 128) = -NNNN (EFAULT)
#else
  //staptest// [[[[readlink (!!!!readlinkat (AT_FDCWD, ]]]]0x[f]+, XXXX, 128) = -NNNN (EFAULT)
#endif

  readlink("Sfoobar", (char *)-1, sizeof(buf));
#ifdef __s390__
  //staptest// readlink ("Sfoobar", 0x[7]?[f]+, 128) = -NNNN (EFAULT)
#else
  //staptest// [[[[readlink (!!!!readlinkat (AT_FDCWD, ]]]]"Sfoobar", 0x[f]+, 128) = -NNNN (EFAULT)
#endif

  readlink("Sfoobar", buf, -1);
  //staptest// [[[[readlink (!!!!readlinkat (AT_FDCWD, ]]]]"Sfoobar", XXXX, -1) = -NNNN (EINVAL)

#if GLIBC_SUPPORT
  readlinkat(AT_FDCWD, "Sfoobar", buf, sizeof(buf));
  //staptest// readlinkat (AT_FDCWD, "Sfoobar", XXXX, 128) = 6

  readlinkat(-1, "Sfoobar", buf, sizeof(buf));
  //staptest// readlinkat (-1, "Sfoobar", XXXX, 128) = -NNNN (EBADF)

  readlinkat(AT_FDCWD, (char *)-1, buf, sizeof(buf));
#ifdef __s390__
  //staptest// readlinkat (AT_FDCWD, 0x[7]?[f]+, XXXX, 128) = -NNNN (EFAULT)
#else
  //staptest// readlinkat (AT_FDCWD, 0x[f]+, XXXX, 128) = -NNNN (EFAULT)
#endif

  readlinkat(AT_FDCWD, "Sfoobar", (char *)-1, sizeof(buf));
#ifdef __s390__
  //staptest// readlinkat (AT_FDCWD, "Sfoobar", 0x[7]?[f]+, 128) = -NNNN (EFAULT)
#else
  //staptest// readlinkat (AT_FDCWD, "Sfoobar", 0x[f]+, 128) = -NNNN (EFAULT)
#endif

  readlinkat(AT_FDCWD, "Sfoobar", buf, -1);
  //staptest// readlinkat (AT_FDCWD, "Sfoobar", XXXX, -1) = -NNNN (EINVAL)
#endif

  return 0;
}
