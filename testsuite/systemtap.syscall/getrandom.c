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

#ifdef __NR_getrandom
static inline int __getrandom(void *buf, size_t buflen, unsigned int flags)
{
    return syscall(__NR_getrandom, buf, buflen, flags);
}
#endif

int main()
{
// e.g. kernel-headers-3.13.9-200.fc20.x86_64 do not have __NR_getrandom
#ifdef __NR_getrandom
  char i[5] = {0};
  char k[5] = {0};

  __getrandom(i, sizeof(i), GRND_RANDOM);
  //staptest// [[[[getrandom ("", 5, GRND_RANDOM) = 5!!!!ni_syscall () = NNNN]]]]

  __getrandom((void *)-1, 5, GRND_RANDOM);
#ifdef __s390__ 
  //staptest// [[[[getrandom (0x[7]?[f]+, 5, GRND_RANDOM)!!!!ni_syscall ()]]]] = -NNNN
#else
  //staptest// [[[[getrandom (0x[f]+, 5, GRND_RANDOM)!!!!ni_syscall ()]]]] = -NNNN
#endif

  /* Here we want to make sure to pass NULL as the buffer, as we don't
   * really want to write that many bytes into a buffer that wouldn't
   * be that big. */
  __getrandom(NULL, (size_t)-1, GRND_RANDOM);
#if __WORDSIZE == 64
  //staptest// [[[[getrandom (0x0, 18446744073709551615, GRND_RANDOM)!!!!ni_syscall ()]]]] = NNNN
#else
  //staptest// [[[[getrandom (0x0, 4294967295, GRND_RANDOM)!!!!ni_syscall ()]]]] = NNNN
#endif

  __getrandom(k, sizeof(k), -1);
  //staptest// [[[[getrandom ("", 5, GRND_[^ ]+|XXXX)!!!!ni_syscall ()]]]] = -NNNN
#endif
  return 0;
}
