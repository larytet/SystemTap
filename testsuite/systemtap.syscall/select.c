/* COVERAGE: select pselect6 pselect7 */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/syscall.h>

int main()
{
  int fd;
  struct timespec tim = {0, 200000000};
  sigset_t sigs;
  fd_set rfds;
  struct timeval tv = {0, 117};

  sigemptyset(&sigs);
  sigaddset(&sigs,SIGUSR2);

  select(1, &rfds, NULL, NULL, &tv);
  //staptest// [[[[select (1, XXXX, 0x[0]+, 0x[0]+, \[0.000117\])!!!!pselect6 (1, XXXX, 0x[0]+, 0x[0]+, \[0.000117[0]+\], 0x0)]]]]

  tv.tv_sec = 0;
  tv.tv_usec = 113;

  select(1, NULL, NULL, NULL, &tv);
  //staptest// [[[[select (1, 0x[0]+, 0x[0]+, 0x[0]+, \[0.000113\])!!!!pselect6 (1, 0x[0]+, 0x[0]+, 0x[0]+, \[0.000113[0]+\], 0x0)]]]]

  tv.tv_sec = 0;
  tv.tv_usec = 120;
  select(-1, &rfds, NULL, NULL, &tv);
  //staptest// [[[[select (-1, XXXX, 0x[0]+, 0x[0]+, \[0.000120\]!!!!pselect6 (-1, XXXX, 0x[0]+, 0x[0]+, \[0.000120[0]+\], 0x0]]]]) = -NNNN (EINVAL)

  tv.tv_sec = 0;
  tv.tv_usec = 121;
  select(1, (fd_set *)-1, NULL, NULL, &tv);
#ifdef __s390__
  //staptest// select (1, 0x[7]?[f]+, 0x[0]+, 0x[0]+, \[0.000121\]) = [[[[0!!!!-NNNN (EFAULT)]]]]
#else
  //staptest// [[[[select (1, 0x[f]+, 0x[0]+, 0x[0]+, \[0.000121\]!!!!pselect6 (1, 0x[f]+, 0x[0]+, 0x[0]+, \[0.000121[0]+\], 0x0]]]]) = [[[[0!!!!-NNNN (EFAULT)]]]]
#endif

  tv.tv_sec = 0;
  tv.tv_usec = 122;
  select(1, &rfds, (fd_set *)-1, NULL, &tv);
#ifdef __s390__
  //staptest// select (1, XXXX, 0x[7]?[f]+, 0x[0]+, \[0.000122\]) = [[[[0!!!!-NNNN (EFAULT)]]]]
#else
  //staptest// [[[[select (1, XXXX, 0x[f]+, 0x[0]+, \[0.000122\]!!!!pselect6 (1, XXXX, 0x[f]+, 0x[0]+, \[0.000122[0]+\], 0x0]]]]) = [[[[0!!!!-NNNN (EFAULT)]]]]
#endif

  tv.tv_sec = 0;
  tv.tv_usec = 123;
  select(1, &rfds, NULL, (fd_set *)-1, &tv);
#ifdef __s390__
  //staptest// select (1, XXXX, 0x[0]+, 0x[7]?[f]+, \[0.000123\]) = [[[[0!!!!-NNNN (EFAULT)]]]]
#else
  //staptest// [[[[select (1, XXXX, 0x[0]+, 0x[f]+, \[0.000123\]!!!!pselect6 (1, XXXX, 0x[0]+, 0x[f]+, \[0.000123[0]+\], 0x0]]]]) = [[[[0!!!!-NNNN (EFAULT)]]]]
#endif

#if defined(__arm__) || defined(__aarch64__)
  syscall(SYS_pselect6, 1, &rfds, NULL, NULL, (struct timeval *)-1);
#else
  select(1, &rfds, NULL, NULL, (struct timeval *)-1);
#endif
#ifdef __s390__
  //staptest// select (1, XXXX, 0x[0]+, 0x[0]+, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
  //staptest// [[[[select (1, XXXX, 0x[0]+, 0x[0]+, 0x[f]+!!!!pselect6 (1, XXXX, 0x[0]+, 0x[0]+, 0x[f]+, 0x0]]]]) = -NNNN (EFAULT)
#endif

#if defined(SYS_pselect6) || defined(SYS_pselect7)
  pselect( 1, &rfds, NULL, NULL, &tim, &sigs);
  //staptest//pselect[67] (1, XXXX, 0x[0]+, 0x[0]+, \[0.200000000\], XXXX) = NNNN

  pselect( 0, NULL, NULL, NULL, &tim, &sigs);
  //staptest// pselect[67] (0, 0x[0]+, 0x[0]+, 0x[0]+, \[0.200000000\], XXXX) = NNNN

  pselect(-1, NULL, NULL, NULL, &tim, &sigs);
  //staptest//pselect[67] (-1, 0x[0]+, 0x[0]+, 0x[0]+, \[0.200000000\], XXXX) = -NNNN

  pselect(1, (fd_set *)-1, NULL, NULL, &tim, &sigs);
#ifdef __s390__
  //staptest//pselect[67] (1, 0x[7]?[f]+, 0x[0]+, 0x[0]+, \[0.200000000\], XXXX) = NNNN
#else
  //staptest//pselect[67] (1, 0x[f]+, 0x[0]+, 0x[0]+, \[0.200000000\], XXXX) = NNNN
#endif

  pselect(1, &rfds, (fd_set *)-1, NULL, &tim, &sigs);
#ifdef __s390__
  //staptest//pselect[67] (1, XXXX, 0x[7]?[f]+, 0x[0]+, \[0.200000000\], XXXX) = NNNN
#else
  //staptest//pselect[67] (1, XXXX, 0x[f]+, 0x[0]+, \[0.200000000\], XXXX) = NNNN
#endif

  pselect(1, &rfds, NULL, (fd_set *)-1, &tim, &sigs);
#ifdef __s390__
  //staptest//pselect[67] (1, XXXX, 0x[0]+, 0x[7]?[f]+, \[0.200000000\], XXXX) = NNNN
#else
  //staptest//pselect[67] (1, XXXX, 0x[0]+, 0x[f]+, \[0.200000000\], XXXX) = NNNN
#endif

  // This causes the exe to get a SIGSEGV...
  //pselect(1, &rfds, NULL, NULL, (struct timespec *)-1, &sigs);

  // glibc is "fixing" this one...
  //pselect(1, &rfds, NULL, NULL, &tim, (sigset_t *)-1);
#endif

  return 0;
}
