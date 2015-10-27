/* COVERAGE: utimes futimes futimesat utimensat */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/utime.h>
#include <linux/version.h>

#ifndef UTIME_NOW
#define UTIME_NOW       ((1l << 30) - 1l)
#define UTIME_OMIT      ((1l << 30) - 2l)
#endif

int main()
{
  int fd;
  struct timeval tv[2];
  struct timespec ts[2];
  struct utimbuf times;

  fd = creat("foobar", 0666);

  /* access time */
  tv[0].tv_sec = 1000000000;
  tv[0].tv_usec = 1234;
  tv[1].tv_sec = 2000000000;
  tv[1].tv_usec = 5678;


#ifdef __NR_utime
 times.actime = 1000000000;
 times.modtime = 2000000000;
 syscall(__NR_utime, "foobar", &times );
 //staptest// utime ("foobar", \[Sun Sep  9 01:46:40 2001, Wed May 18 03:33:20 2033])

 syscall(__NR_utime, (char *)-1, &times );
#ifdef __s390__
 //staptest// utime (0x[7]?[f]+, \[Sun Sep  9 01:46:40 2001, Wed May 18 03:33:20 2033]) = -NNNN
#else
 //staptest// utime (0x[f]+, \[Sun Sep  9 01:46:40 2001, Wed May 18 03:33:20 2033]) = -NNNN
#endif

 syscall(__NR_utime, "foobar", (struct utimbuf *)-1 );
 //staptest// utime ("foobar", \[Thu Jan  1 00:00:00 1970, Thu Jan  1 00:00:00 1970\]) = -NNNN (EFAULT)
#endif /* __NR_utime */
  
#ifdef __NR_utimes
  syscall(__NR_utimes, "foobar", tv);
  //staptest// utimes ("foobar", \[1000000000.001234\]\[2000000000.005678\])

  syscall(__NR_utimes, (char *)-1, tv);
#ifdef __s390__
  //staptest// utimes (0x[7]?[f]+, \[1000000000.001234\]\[2000000000.005678\])
#else
  //staptest// utimes (0x[f]+, \[1000000000.001234\]\[2000000000.005678\])
#endif

  syscall(__NR_utimes, "foobar", (struct timeval *)-1);
#ifdef __s390__
  //staptest// utimes ("foobar", 0x[7]?[f]+) = -NNNN
#else
  //staptest// utimes ("foobar", 0x[f]+) = -NNNN
#endif
#endif /* __NR_utimes */

#ifdef __NR_futimesat
  syscall(__NR_futimesat, 7, "foobar", tv);
  //staptest// futimesat (7, "foobar", \[1000000000.001234\]\[2000000000.005678\])

  syscall(__NR_futimesat, AT_FDCWD, "foobar", tv);
  //staptest// futimesat (AT_FDCWD, "foobar", \[1000000000.001234\]\[2000000000.005678\])

  syscall(__NR_futimesat, -1, "foobar", tv);
  //staptest// futimesat (-1, "foobar", \[1000000000.001234\]\[2000000000.005678\]) = -NNNN

  syscall(__NR_futimesat, AT_FDCWD, (char *)-1, tv);
#ifdef __s390__
  //staptest// futimesat (AT_FDCWD, 0x[7]?[f]+, \[1000000000.001234\]\[2000000000.005678\]) = -NNNN
#else
  //staptest// futimesat (AT_FDCWD, 0x[f]+, \[1000000000.001234\]\[2000000000.005678\]) = -NNNN
#endif

  syscall(__NR_futimesat, AT_FDCWD, "foobar", (struct timeval *)-1);
#ifdef __s390__
  //staptest// futimesat (AT_FDCWD, "foobar", 0x[7]?[f]+) = -NNNN
#else
  //staptest// futimesat (AT_FDCWD, "foobar", 0x[f]+) = -NNNN
#endif
#endif /* __NR_futimesat */

#if defined(__NR_utimensat) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
  ts[0].tv_sec = 1000000000;
  ts[0].tv_nsec = 123456789;
  ts[1].tv_sec = 2000000000;
  ts[1].tv_nsec = 56780000;  
  syscall(__NR_utimensat, AT_FDCWD, "foobar", ts, 0);
  //staptest// utimensat (AT_FDCWD, "foobar", \[1000000000.123456789\]\[2000000000.056780000\], 0x0)

  ts[0].tv_sec = 0;
  ts[0].tv_nsec = UTIME_NOW;
  ts[1].tv_sec = 0;
  ts[1].tv_nsec = UTIME_OMIT;
  syscall(__NR_utimensat, AT_FDCWD, "foobar", ts, AT_SYMLINK_NOFOLLOW);
  //staptest// utimensat (AT_FDCWD, "foobar", \[UTIME_NOW\]\[UTIME_OMIT\], AT_SYMLINK_NOFOLLOW)

  syscall(__NR_utimensat, 22, "foobar", ts, 0x42);
  //staptest// utimensat (22, "foobar", \[UTIME_NOW\]\[UTIME_OMIT\], 0x42)

  syscall(__NR_utimensat, -1, "foobar", ts, AT_SYMLINK_NOFOLLOW);
  //staptest// utimensat (-1, "foobar", \[UTIME_NOW\]\[UTIME_OMIT\], AT_SYMLINK_NOFOLLOW) = -NNNN

  syscall(__NR_utimensat, AT_FDCWD, (char *)-1, ts, AT_SYMLINK_NOFOLLOW);
#ifdef __s390__
  //staptest// utimensat (AT_FDCWD, 0x[7]?[f]+, \[UTIME_NOW\]\[UTIME_OMIT\], AT_SYMLINK_NOFOLLOW)
#else
  //staptest// utimensat (AT_FDCWD, 0x[f]+, \[UTIME_NOW\]\[UTIME_OMIT\], AT_SYMLINK_NOFOLLOW)
#endif

  syscall(__NR_utimensat, AT_FDCWD, "foobar", (struct timespec *)-1, AT_SYMLINK_NOFOLLOW);
#ifdef __s390__
  //staptest// utimensat (AT_FDCWD, "foobar", 0x[7]?[f]+, AT_SYMLINK_NOFOLLOW)
#else
  //staptest// utimensat (AT_FDCWD, "foobar", 0x[f]+, AT_SYMLINK_NOFOLLOW)
#endif

  syscall(__NR_utimensat, AT_FDCWD, "foobar", ts, -1);
  //staptest// utimensat (AT_FDCWD, "foobar", \[UTIME_NOW\]\[UTIME_OMIT\], AT_[^ ]+|XXXX)
#endif 

  return 0;
}
