/* COVERAGE: mkdir chdir open fchdir close rmdir mkdirat */
#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

int main()
{
  int fd;

  mkdir("foobar", 0765);
  //staptest// [[[[mkdir (!!!!mkdirat (AT_FDCWD, ]]]]"foobar", 0765) =

  mkdir((char *)-1, 0765);
#ifdef __s390__
  //staptest// mkdir ([7]?[f]+, 0765) = -NNNN
#else
  //staptest// [[[[mkdir (!!!!mkdirat (AT_FDCWD, ]]]][f]+, 0765) = -NNNN
#endif

  mkdir("foobar2", (mode_t)-1);
  //staptest// [[[[mkdir (!!!!mkdirat (AT_FDCWD, ]]]]"foobar2", 037777777777) = NNNN

  chdir("foobar");
  //staptest// chdir ("foobar") = 0

  chdir("..");
  //staptest// chdir ("..") = 0

  chdir((char *)-1);
#ifdef __s390__
  //staptest// chdir ([7]?[f]+) = -NNNN
#else
  //staptest// chdir ([f]+) = -NNNN
#endif

  fd = open("foobar", O_RDONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"foobar", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN

  fchdir(fd);
  //staptest// fchdir (NNNN) = 0

  fchdir(-1);
  //staptest// fchdir (-1) = -NNNN (EBADF)

  chdir("..");
  //staptest// chdir ("..") = 0

  close(fd);
  //staptest// close (NNNN) = 0

  rmdir("foobar");
  //staptest// [[[[rmdir ("foobar"!!!!unlinkat (AT_FDCWD, "foobar", AT_REMOVEDIR]]]]) = 0

  rmdir((char *)-1);
#ifdef __s390__
  //staptest// rmdir ([7]?[f]+) = -NNNN
#else
  //staptest// [[[[rmdir ([f]+!!!!unlinkat (AT_FDCWD, [f]+, AT_REMOVEDIR]]]]) = -NNNN
#endif

  fd = open(".", O_RDONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]".", O_RDONLY[[[[.O_LARGEFILE]]]]?) = NNNN

#ifdef SYS_mkdirat
  mkdirat(fd, "xyzzy", 0765);
  //staptest// mkdirat (NNNN, "xyzzy", 0765) = 0

  mkdirat(-1, "xyzzy2", 0765);
  //staptest// mkdirat (-1, "xyzzy2", 0765) = -NNNN (EBADF)

  mkdirat(fd, (char *)-1, 0765);
#ifdef __s390__
  //staptest// mkdirat (NNNN, [7]?[f]+, 0765) = -NNNN
#else
  //staptest// mkdirat (NNNN, [f]+, 0765) = -NNNN
#endif

  mkdirat(fd, "xyzzy2", (mode_t)-1);
  //staptest// mkdirat (NNNN, "xyzzy2", 037777777777) = NNNN
#endif

  close(fd);
  //staptest// close (NNNN) = 0

  rmdir("xyzzy");
  //staptest// [[[[rmdir ("xyzzy"!!!!unlinkat (AT_FDCWD, "xyzzy", AT_REMOVEDIR]]]]) =

  return 0;
}
