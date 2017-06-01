/* COVERAGE: memfd_create */

/*
 * Glibc doesn't support memfd_create yet, so we have to use syscall(2)
 */
#include <sys/syscall.h>
#include <unistd.h>
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001u
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002u
#endif

int main()
{
#ifdef __NR_memfd_create
   syscall(__NR_memfd_create,"memfd_create", MFD_CLOEXEC|MFD_ALLOW_SEALING);
   //staptest// [[[[memfd_create ("memfd_create", MFD_CLOEXEC|MFD_ALLOW_SEALING)!!!!ni_syscall ()]]]] = NNNN
   syscall(__NR_memfd_create, (size_t)-1, MFD_CLOEXEC|MFD_ALLOW_SEALING);
#if __WORDSIZE == 64
   //staptest// [[[[memfd_create (0x[16]?[f]+, MFD_CLOEXEC|MFD_ALLOW_SEALING)!!!!ni_syscall ()]]]] = -NNNN
#else
#ifdef __s390__
   //staptest// [[[[memfd_create (0x[7]?[f]+, MFD_CLOEXEC|MFD_ALLOW_SEALING)!!!!ni_syscall ()]]]] = -NNNN
#else
   //staptest// [[[[memfd_create (0x[8]?[f]+, MFD_CLOEXEC|MFD_ALLOW_SEALING)!!!!ni_syscall ()]]]] = -NNNN
#endif
#endif
   syscall(__NR_memfd_create,"memfd_create1", -1);
   //staptest// [[[[memfd_create ("memfd_create1", MFD_[^ ]+|XXXX)!!!!ni_syscall ()]]]] = -NNNN
#endif

  return 0;
}
