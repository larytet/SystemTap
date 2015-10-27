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

int main()
{
// e.g. kernel-headers-3.13.9-200.fc20.x86_64 do not have __NR_getrandom
#ifdef __NR_getrandom
  char i[5] = {0};
  char j[5] = {0};
  char k[5] = {0};

  syscall(__NR_getrandom, i, sizeof(i), GRND_RANDOM);
  //staptest// [[[[getrandom ("", 5, GRND_RANDOM) = 5!!!!ni_syscall () = NNNN]]]]

  syscall(__NR_getrandom, (size_t)-1, 5, GRND_RANDOM);
#if __WORDSIZE == 64
  //staptest// [[[[getrandom (0x[16]?[f]+, 5, GRND_RANDOM)!!!!ni_syscall ()]]]] = -NNNN
#else
  //staptest// [[[[getrandom (0x[8]?[f]+, 5, GRND_RANDOM)!!!!ni_syscall ()]]]] = -NNNN
#endif

  syscall(__NR_getrandom, j, -1, GRND_RANDOM);
  //staptest// [[[[getrandom ("", 4294967295, GRND_RANDOM)!!!!ni_syscall ()]]]] = NNNN

  syscall(__NR_getrandom, k, sizeof(k), -1);
  //staptest// [[[[getrandom ("", 5, GRND_[^ ]+|XXXX)!!!!ni_syscall ()]]]] = -NNNN
#endif
  return 0;
}
