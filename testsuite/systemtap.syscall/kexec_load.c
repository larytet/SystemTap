/* COVERAGE: kexec_load */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/capability.h>

static struct __user_cap_header_struct header;
static struct __user_cap_data_struct data;

// KEXEC_LOAD(2): The required constants are in the Linux kernel source file
// linux/kexec.h, which is not currently exported to glibc. Therefore, these
// constants must be defined manually.

// kexec flags for different usage scenarios
#define KEXEC_ON_CRASH          0x00000001
// These values match the ELF architecture values.
#define KEXEC_ARCH_DEFAULT ( 0 << 16)
#define KEXEC_ARCH_386     ( 3 << 16)

int main()
{
    // Ensure the syscall won't be able to succeed
    header.version = _LINUX_CAPABILITY_VERSION;
    header.pid = getpid();
    capget(&header, &data);
    data.effective &= ~(1 << CAP_SYS_BOOT);
    capset(&header, &data);

    // Test normal operation
    syscall(__NR_kexec_load, NULL, 0, NULL, KEXEC_ON_CRASH|KEXEC_ARCH_386);
    //staptest// [[[[kexec_load (0x0, 0, 0x0, KEXEC_ON_CRASH|KEXEC_ARCH_386)!!!!ni_syscall ()]]]] = NNNN

    // Limit testing
    syscall(__NR_kexec_load, (unsigned long)-1, 0, NULL, KEXEC_ARCH_DEFAULT);
#if __WORDSIZE == 64
    //staptest// [[[[kexec_load (0xffffffffffffffff, 0, 0x0, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[kexec_load (0xffffffff, 0, 0x0, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#endif

    syscall(__NR_kexec_load, NULL, (unsigned long)-1, NULL, KEXEC_ARCH_DEFAULT);
#if __WORDSIZE == 64
    //staptest// [[[[kexec_load (0x0, 18446744073709551615, 0x0, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[kexec_load (0x0, 4294967295, 0x0, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#endif

    syscall(__NR_kexec_load, NULL, 0, (struct kexec_segment *)-1, KEXEC_ARCH_DEFAULT);
#ifdef __s390__
    //staptest// kexec_load (0x0, 0, 0x[7]?[f]+, KEXEC_ARCH_DEFAULT) = -NNNN
#else
    //staptest// [[[[kexec_load (0x0, 0, 0x[f]+, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#endif

    syscall(__NR_kexec_load, NULL, 0, NULL, (unsigned long)-1);
    //staptest// [[[[kexec_load (0x0, 0, 0x0, KEXEC_ON_CRASH|[^)]+)!!!!ni_syscall ()]]]] = -NNNN

    return 0;
}
