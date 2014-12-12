/* COVERAGE: epoll_create epoll_ctl epoll_wait epoll_pwait poll ppoll */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/epoll.h>
#include <poll.h>
#include <signal.h>
#include <sys/syscall.h>

int main()
{
  struct epoll_event ev, events[17];
  struct pollfd pfd = {7, 0x23, 0};
  int fd;
  struct timespec tim = {.tv_sec=0, .tv_nsec=200000000};
  sigset_t sigs;

  sigemptyset(&sigs);
  sigaddset(&sigs,SIGUSR2);

#ifdef EPOLL_CLOEXEC
  fd = epoll_create1(EPOLL_CLOEXEC);
  //staptest// epoll_create1 (EPOLL_CLOEXEC) = NNNN

  epoll_create1(-1);
  //staptest// epoll_create1 (EPOLL_CLOEXEC|0xfff7ffff) = -NNNN (EINVAL)
#else
  fd = epoll_create(32);
  //staptest// epoll_create (32) = NNNN

  epoll_create(-1);
  //staptest// epoll_create (-1) = -NNNN (EINVAL)
#endif

  epoll_ctl(fd, EPOLL_CTL_ADD, 13, &ev);
  //staptest// epoll_ctl (NNNN, EPOLL_CTL_ADD, 13, XXXX) = -NNNN (EBADF)

  epoll_ctl(-1, EPOLL_CTL_ADD, 13, &ev);
  //staptest// epoll_ctl (-1, EPOLL_CTL_ADD, 13, XXXX) = -NNNN (EBADF)

  epoll_ctl(fd, -1, 13, &ev);
  //staptest// epoll_ctl (NNNN, 0xffffffff, 13, XXXX) = -NNNN (EBADF)

  epoll_ctl(fd, EPOLL_CTL_ADD, -1, &ev);
  //staptest// epoll_ctl (NNNN, EPOLL_CTL_ADD, -1, XXXX) = -NNNN (EBADF)

  epoll_ctl(fd, EPOLL_CTL_ADD, 13, (struct epoll_event *)-1);
#ifdef __s390__
  //staptest// epoll_ctl (NNNN, EPOLL_CTL_ADD, 13, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// epoll_ctl (NNNN, EPOLL_CTL_ADD, 13, 0x[f]+) = -NNNN (EFAULT)
#endif

  epoll_wait(fd, events, 17, 0);
#if defined(__arm__) || defined(__aarch64__)
  //staptest// epoll_pwait (NNNN, XXXX, 17, 0, XXXX, NNNN) = 0
#else
  //staptest// epoll_wait (NNNN, XXXX, 17, 0) = 0
#endif

  epoll_wait(-1, events, 17, 0);
#if defined(__arm__) || defined(__aarch64__)
  //staptest// epoll_pwait (-1, XXXX, 17, 0, XXXX, NNNN) = -NNNN (EBADF)
#else
  //staptest// epoll_wait (-1, XXXX, 17, 0) = -NNNN (EBADF)
#endif

  epoll_wait(fd, (struct epoll_event *)-1, 17, 0);
#ifdef __s390__
  //staptest// epoll_wait (NNNN, 0x[7]?[f]+, 17, 0) =
#elif defined(__arm__) || defined(__aarch64__)
  //staptest// epoll_pwait (NNNN, 0x[f]+, 17, 0, XXXX, NNNN) = NNNN
#else
  //staptest// epoll_wait (NNNN, 0x[f]+, 17, 0) =
#endif

  epoll_wait(fd, events, -1, 0);
#if defined(__arm__) || defined(__aarch64__)
  //staptest// epoll_pwait (NNNN, XXXX, -1, 0, XXXX, NNNN) = NNNN (EINVAL)
#else
  //staptest// epoll_wait (NNNN, XXXX, -1, 0) = -NNNN (EINVAL)
#endif

  epoll_wait(-1, events, 17, -1);
#if defined(__arm__) || defined(__aarch64__)
  //staptest// epoll_pwait (-1, XXXX, 17, -1, XXXX, NNNN) = NNNN (EBADF)
#else
  //staptest// epoll_wait (-1, XXXX, 17, -1) = -NNNN (EBADF)
#endif

#ifdef SYS_epoll_pwait
  epoll_pwait(fd, events, 17, 0, NULL);
  //staptest// epoll_pwait (NNNN, XXXX, 17, 0, 0x0, NNNN) = 0

  epoll_pwait(fd, events, 17, 0, &sigs);
  //staptest// epoll_pwait (NNNN, XXXX, 17, 0, XXXX, NNNN) = 0

  epoll_pwait(-1, events, 17, 0, &sigs);
  //staptest// epoll_pwait (-1, XXXX, 17, 0, XXXX, NNNN) = -NNNN (EBADF)

  epoll_pwait(fd, (struct epoll_event *)-1, 17, 0, &sigs);
#ifdef __s390__
  //staptest// epoll_pwait (NNNN, 0x[7]?[f]+, 17, 0, XXXX, NNNN) =
#else
  //staptest// epoll_pwait (NNNN, 0x[f]+, 17, 0, XXXX, NNNN) =
#endif

  epoll_pwait(fd, events, -1, 0, &sigs);
  //staptest// epoll_pwait (NNNN, XXXX, -1, 0, XXXX, NNNN) = -NNNN (EINVAL)

  epoll_pwait(-1, events, 17, -1, &sigs);
  //staptest// epoll_pwait (-1, XXXX, 17, -1, XXXX, NNNN) = -NNNN (EBADF)

  epoll_pwait(fd, events, 17, 0, (sigset_t *)-1);
#ifdef __s390__
  //staptest// epoll_pwait (NNNN, XXXX, 17, 0, 0x[7]?[f]+, NNNN) = -NNNN (EFAULT)
#else
  //staptest// epoll_pwait (NNNN, XXXX, 17, 0, 0x[f]+, NNNN) = -NNNN (EFAULT)
#endif
#endif

  close(fd);
  //staptest// close (NNNN) = 0

  poll(&pfd, 1, 0);
#if defined(__arm__) || defined(__aarch64__)
  //staptest// ppoll (XXXX, 1, \[0.000000000\], XXXX, NNNN) = NNNN
#else
  //staptest// poll (XXXX, 1, 0) = NNNN
#endif

  poll((struct pollfd *)-1, 1, 0);
#ifdef __s390__
  //staptest// poll (0x[7]?[f]+, 1, 0) = -NNNN (EFAULT)
#elif defined(__arm__) || defined(__aarch64__)
  //staptest// ppoll (0x[f]+, 1, \[0.000000000\], XXXX, NNNN) = -NNNN (EFAULT)
#else
  //staptest// poll (0x[f]+, 1, 0) = -NNNN (EFAULT)
#endif

  poll(&pfd, -1, 0);
#if defined(__arm__) || defined(__aarch64__)
  //staptest// ppoll (XXXX, 4294967295, \[0.000000000\], XXXX, NNNN) = -NNNN (EINVAL)
#else
  //staptest// poll (XXXX, 4294967295, 0) = -NNNN (EINVAL)
#endif

  // A timetout value of -1 means an infinite timeout. So, we'll also
  // send a NULL pollfd structure pointer.
  poll(NULL, 1, -1);
#if defined(__arm__) || defined(__aarch64__)
  //staptest// ppoll (0x0, 1, NULL, XXXX, NNNN) = -NNNN (EFAULT)
#else
  //staptest// poll (0x0, 1, -1) = -NNNN (EFAULT)
#endif

#ifdef SYS_ppoll
  ppoll(&pfd, 1, &tim, &sigs);
  //staptest//  ppoll (XXXX, 1, \[0.200000000\], XXXX, 8) = NNNN

  ppoll((struct pollfd *)-1, 1, &tim, &sigs);
#ifdef __s390__
  //staptest//  ppoll (0x[7]?[f]+, 1, \[0.200000000\], XXXX, 8) = -NNNN (EFAULT)
#else
  //staptest//  ppoll (0x[f]+, 1, \[0.200000000\], XXXX, 8) = -NNNN (EFAULT)
#endif

  ppoll(&pfd, -1, &tim, &sigs);
  //staptest//  ppoll (XXXX, 4294967295, \[0.200000000\], XXXX, 8) = -NNNN (EINVAL)

  // Specifying a timespec pointer of -1 will crash the test
  // executable, so we'll have to skip it.
  //ppoll((struct pollfd *)-1, 1, (struct timespec *)-1, &sigs);

  ppoll(&pfd, 1, &tim, (sigset_t *)-1);
#ifdef __s390__
  //staptest//  ppoll (XXXX, 1, \[0.200000000\], 0x[7]?[f]+, 8) = -NNNN (EFAULT)
#else
  //staptest//  ppoll (XXXX, 1, \[0.200000000\], 0x[f]+, 8) = -NNNN (EFAULT)
#endif
#endif

  return 0;
}
