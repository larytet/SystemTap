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

#ifdef __NR_memfd_create
#define my_memfd_create __NR_memfd_create
#elif defined(SYS_memfd_create)
#define my_memfd_create SYS_memfd_create
#else
#error "SYS_memfd_create not defined"
#endif

int main()
{
   syscall(my_memfd_create,"memfd_create", MFD_CLOEXEC|MFD_ALLOW_SEALING);
   //staptest// memfd_create ("memfd_create", MFD_CLOEXEC|MFD_ALLOW_SEALING) = NNNN
   syscall(my_memfd_create, (size_t)-1, MFD_CLOEXEC|MFD_ALLOW_SEALING);
#if __WORDSIZE == 64
   //staptest// memfd_create ([16]?[f]+, MFD_CLOEXEC|MFD_ALLOW_SEALING) = -NNNN (EFAULT)
#else
   //staptest// memfd_create ([8]?[f]+, MFD_CLOEXEC|MFD_ALLOW_SEALING) = -NNNN (EFAULT)
#endif
   syscall(my_memfd_create,"memfd_create1", -1);
   //staptest// memfd_create ("memfd_create1", MFD_[^ ]+|XXXX) = -NNNN (EINVAL)
  return 0;
}
