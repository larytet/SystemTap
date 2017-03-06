/* COVERAGE: kexec_load kexec_file_load */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/capability.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/version.h>

#define capget(x,y) syscall(__NR_capget,x,y)
#define capset(x,y) syscall(__NR_capset,x,y)

static struct __user_cap_header_struct header;
static struct __user_cap_data_struct data;

// The linux/kexec.h header file was exported to userspace starting
// with kernel 3.7.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
#include <linux/kexec.h>
#endif

// kexec flags for different usage scenarios
#ifndef KEXEC_ON_CRASH
#define KEXEC_ON_CRASH          0x00000001
#endif

// These values match the ELF architecture values.
#ifndef KEXEC_ARCH_DEFAULT
#define KEXEC_ARCH_DEFAULT ( 0 << 16)
#endif
#ifndef KEXEC_ARCH_386
#define KEXEC_ARCH_386     ( 3 << 16)
#endif

// These constants were added in kernel 3.17 for kexec_file_load().
#ifndef KEXEC_FILE_UNLOAD
#define KEXEC_FILE_UNLOAD       0x00000001
#endif
#ifndef KEXEC_FILE_ON_CRASH
#define KEXEC_FILE_ON_CRASH     0x00000002
#endif
#ifndef KEXEC_FILE_NO_INITRAMFS
#define KEXEC_FILE_NO_INITRAMFS 0x00000004
#endif

static inline int __kexec_load(unsigned long entry, unsigned long nr_segments,
			       void *segments, unsigned long flags)
{
    return syscall(__NR_kexec_load, entry, nr_segments, segments, flags);
}

#ifdef __NR_kexec_file_load
static inline int __kexec_file_load(int kernel_fd, int initrd_fd,
				    unsigned long cmdline_len,
				    const char * cmdline, unsigned long flags)
{
    return syscall(__NR_kexec_file_load, kernel_fd, initrd_fd, cmdline_len,
		   cmdline, flags);
}
#endif

int main()
{
#ifdef __NR_kexec_file_load
    int fd;
    char *cmdline = "KEYTABLE=us LANG=en_US.UTF-8";
#endif
   
    // Ensure the syscall won't be able to succeed
    header.version = _LINUX_CAPABILITY_VERSION;
    header.pid = getpid();
    capget(&header, &data);
    data.effective &= ~(1 << CAP_SYS_BOOT);
    capset(&header, &data);

    // Test normal operation
    __kexec_load(0, 0, NULL, KEXEC_ON_CRASH|KEXEC_ARCH_386);
    //staptest// [[[[kexec_load (0x0, 0, 0x0, KEXEC_ON_CRASH|KEXEC_ARCH_386)!!!!ni_syscall ()]]]] = NNNN

    // Limit testing
    __kexec_load((unsigned long)-1, 0, NULL, KEXEC_ARCH_DEFAULT);
#if __WORDSIZE == 64
    //staptest// [[[[kexec_load (0xffffffffffffffff, 0, 0x0, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[kexec_load (0xffffffff, 0, 0x0, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#endif

    __kexec_load(0, (unsigned long)-1, NULL, KEXEC_ARCH_DEFAULT);
#if __WORDSIZE == 64
    //staptest// [[[[kexec_load (0x0, 18446744073709551615, 0x0, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[kexec_load (0x0, 4294967295, 0x0, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#endif

    __kexec_load(0, 0, (struct kexec_segment *)-1, KEXEC_ARCH_DEFAULT);
#ifdef __s390__
    //staptest// kexec_load (0x0, 0, 0x[7]?[f]+, KEXEC_ARCH_DEFAULT) = -NNNN
#else
    //staptest// [[[[kexec_load (0x0, 0, 0x[f]+, KEXEC_ARCH_DEFAULT)!!!!ni_syscall ()]]]] = -NNNN
#endif

    __kexec_load(0, 0, NULL, (unsigned long)-1);
    //staptest// [[[[kexec_load (0x0, 0, 0x0, KEXEC_ON_CRASH|[^)]+)!!!!ni_syscall ()]]]] = -NNNN

#ifdef __NR_kexec_file_load
    /* Note that these calls won't succeed, since we dropped the
     * CAP_SYS_BOOT capability. */

    fd = open("/dev/null", O_RDONLY);
    //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_RDONLY) = NNNN

    __kexec_file_load(fd, fd, strlen(cmdline), cmdline, KEXEC_FILE_ON_CRASH);
    //staptest// [[[[kexec_file_load (NNNN, NNNN, NNNN, "KEYTABLE=us LANG=en_US.UTF-8", KEXEC_FILE_ON_CRASH)!!!!ni_syscall ()]]]] = -NNNN

    /* Limit testing. */
    __kexec_file_load(-1, fd, strlen(cmdline), cmdline, KEXEC_FILE_UNLOAD);
    //staptest// [[[[kexec_file_load (-1, NNNN, NNNN, "KEYTABLE=us LANG=en_US.UTF-8", KEXEC_FILE_UNLOAD)!!!!ni_syscall ()]]]] = -NNNN

    __kexec_file_load(fd, -1, strlen(cmdline), cmdline, KEXEC_FILE_NO_INITRAMFS);
    //staptest// [[[[kexec_file_load (NNNN, -1, NNNN, "KEYTABLE=us LANG=en_US.UTF-8", KEXEC_FILE_NO_INITRAMFS)!!!!ni_syscall ()]]]] = -NNNN

    __kexec_file_load(fd, fd, -1, cmdline, KEXEC_FILE_ON_CRASH);
#if __WORDSIZE == 64
    //staptest// [[[[kexec_file_load (NNNN, NNNN, 18446744073709551615, "KEYTABLE=us LANG=en_US.UTF-8", KEXEC_FILE_ON_CRASH)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[kexec_file_load (NNNN, NNNN, 4294967295, "KEYTABLE=us LANG=en_US.UTF-8", KEXEC_FILE_ON_CRASH)!!!!ni_syscall ()]]]] = -NNNN
#endif

    __kexec_file_load(fd, fd, strlen(cmdline), (char *)-1, KEXEC_FILE_ON_CRASH);
    //staptest// [[[[kexec_file_load (NNNN, NNNN, NNNN, 0x[f]+, KEXEC_FILE_ON_CRASH)!!!!ni_syscall ()]]]] = -NNNN

    __kexec_file_load(fd, fd, strlen(cmdline), cmdline, -1);
    //staptest// [[[[kexec_file_load (NNNN, NNNN, NNNN, "KEYTABLE=us LANG=en_US.UTF-8", KEXEC_FILE_[^ ]+|XXXX)!!!!ni_syscall ()]]]] = -NNNN

    close(fd);
#endif
    return 0;
}
