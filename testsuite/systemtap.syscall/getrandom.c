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

#ifdef __NR_getrandom
#define my_getrandom __NR_getrandom
#elif defined(SYS_getrandom)
#define my_getrandom SYS_getrandom
#else
#error "SYS_getrandom not defined"
#endif

int main()
{
  char i[5] = {0};
  char j[5] = {0};
  char k[5] = {0};
  syscall(my_getrandom, i, sizeof(i), GRND_RANDOM);
  //staptest// getrandom ("", 5, GRND_RANDOM) = 5
  syscall(my_getrandom, (size_t)-1, 5, GRND_RANDOM);
#if __WORDSIZE == 64
  //staptest// getrandom ([16]?[f]+, 5, GRND_RANDOM) = -NNNN (EFAULT)
#else
  //staptest// getrandom ([8]?[f]+, 5, GRND_RANDOM) = -NNNN (EFAULT)
#endif
  syscall(my_getrandom, j, -1, GRND_RANDOM);
  //staptest// getrandom ("", 4294967295, GRND_RANDOM) = NNNN
  syscall(my_getrandom, k, sizeof(k), -1);
  //staptest// getrandom ("", 5, GRND_[^ ]+|XXXX) = -NNNN (EINVAL)
  return 0;
}
