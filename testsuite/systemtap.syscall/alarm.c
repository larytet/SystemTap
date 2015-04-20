/* COVERAGE: alarm nanosleep pause */
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

static void
sigrt_act_handler(int signo, siginfo_t *info, void *context)
{
}

int main()
{
  struct timespec rem, t = {0,789};
  struct sigaction sigrt_act;
  memset(&sigrt_act, 0, sizeof(sigrt_act));
  sigrt_act.sa_handler = (void *)sigrt_act_handler;
  sigaction(SIGALRM, &sigrt_act, NULL);

  alarm(1);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,1.000000\], XXXX) = 0
#else
  //staptest// alarm (1) = 0
#endif

  pause();
#if defined(__ia64__) || defined(__aarch64__)
  //staptest// rt_sigsuspend (\[EMPTY\], 8) = NNNN
#else
  //staptest// pause () = NNNN
#endif

  alarm(0);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,0.000000\], XXXX) = 0
#else
  //staptest// alarm (0) = 0
#endif

  alarm(-1);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,-1.000000\], XXXX) = NNNN
#else
#if __WORDSIZE == 64
  // Sigh. On s390x and ppc64, the kernel gets a 32-bit value.
  //staptest// alarm ([[[[18446744073709551615!!!!4294967295]]]]) = 0
#else
  //staptest// alarm (4294967295) = 0
#endif
#endif

  alarm(0);
#if defined(__ia64__) || defined(__arm__) || defined(__aarch64__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,0.000000\], XXXX) = NNNN
#else
  //staptest// alarm (0) = NNNN
#endif

  sleep(1);
  //staptest// nanosleep (\[1.000000000\], XXXX) = 0

  usleep(1234);
  //staptest// nanosleep (\[0.001234000\], 0x[0]+) = 0

  nanosleep(&t, &rem); 
  //staptest// nanosleep (\[0.000000789\], XXXX) = 0

  nanosleep(&t, NULL); 
  //staptest// nanosleep (\[0.000000789\], 0x[0]+) = 0

  nanosleep((struct timespec *)-1, NULL);
#ifdef __s390__
  //staptest// nanosleep (0x[7]?[f]+, 0x[0]+) = -NNNN
#else
  //staptest// nanosleep (0x[f]+, 0x[0]+) = -NNNN
#endif

  nanosleep(&t, (struct timespec *)-1);
#ifdef __s390__
  //staptest// nanosleep (\[0.000000789\], 0x[7]?[f]+) = NNNN
#else
  //staptest// nanosleep (\[0.000000789\], 0x[f]+) = NNNN
#endif

  return 0;
}

