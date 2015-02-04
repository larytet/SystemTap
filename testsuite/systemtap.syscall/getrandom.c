/* COVERAGE: getrandom */

/*
 * Glibc doesn't support getrandom yet, so we have to use syscall(2)
 */
#include <sys/syscall.h>
#include <unistd.h>

#ifndef GRND_NONBLOCK
#define GRND_NONBLOCK 0x0001
#endif
#ifndef GRND_RANDOM
#define GRND_RANDOM 0x0002
#endif

#if defined(__NR_getrandom) && !defined(SYS_getrandom)
#define SYS_getrandom __NR_getrandom
#endif

int main()
{
#ifdef SYS_getrandom
  char i[5] = {0};
  char j[5] = {0};
  char k[5] = {0};

  syscall(SYS_getrandom, i, sizeof(i), GRND_RANDOM);
  //staptest// getrandom ("", 5, GRND_RANDOM) = 5

  syscall(SYS_getrandom, (size_t)-1, 5, GRND_RANDOM);
#if __WORDSIZE == 64
  //staptest// getrandom ([16]?[f]+, 5, GRND_RANDOM) = -NNNN (EFAULT)
#else
  //staptest// getrandom ([8]?[f]+, 5, GRND_RANDOM) = -NNNN (EFAULT)
#endif

  syscall(SYS_getrandom, j, -1, GRND_RANDOM);
  //staptest// getrandom ("", 4294967295, GRND_RANDOM) = NNNN

  syscall(SYS_getrandom, k, sizeof(k), -1);
  //staptest// getrandom ("", 5, GRND_[^ ]+|XXXX) = -NNNN (EINVAL)
#endif
  return 0;
}
