/* COVERAGE: kcmp */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef __NR_kcmp
// The kernel-headers-3.13.9-200.fc20 doesn't have linux/kcmp.h,
// thus defining needed enum here:
enum kcmp_type {
        KCMP_FILE,
        KCMP_VM,
        KCMP_FILES,
        KCMP_FS,
        KCMP_SIGHAND,
        KCMP_IO,
        KCMP_SYSVSEM,
        KCMP_TYPES,
};

static inline int __kcmp(pid_t pid1, pid_t pid2, int type,
                     unsigned long idx1, unsigned long idx2)
{
    return syscall(__NR_kcmp, pid1, pid2, type, idx1, idx2);
}

int main()
{
    int mypid = getpid();
    unsigned long fd = open("kcmp_testfile", O_CREAT);

    __kcmp(mypid, mypid, KCMP_FILE, fd, fd);
    //staptest// [[[[kcmp (NNNN, NNNN, KCMP_FILE, NNNN, NNNN) = 0!!!!ni_syscall () = -38 (ENOSYS)]]]]

    // Limit testing

    __kcmp(-1, 0, 0, 0, 0);
    //staptest// [[[[kcmp (-1, 0, KCMP_FILE, 0, 0)!!!!ni_syscall ()]]]] = -NNNN

    __kcmp(0, -1, 0, 0, 0);
    //staptest// [[[[kcmp (0, -1, KCMP_FILE, 0, 0)!!!!ni_syscall ()]]]] = -NNNN

    __kcmp(0, 0, -1, 0, 0);
    //staptest// [[[[kcmp (0, 0, 0x[f]+, 0, 0)!!!!ni_syscall ()]]]] = -NNNN

    __kcmp(0, 0, 0, -1, 0);
#if __WORDSIZE == 64
    //staptest// [[[[kcmp (0, 0, KCMP_FILE, 18446744073709551615, 0)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[kcmp (0, 0, KCMP_FILE, 4294967295, 0)!!!!ni_syscall ()]]]] = -NNNN
#endif

    __kcmp(0, 0, 0, 0, -1);
#if __WORDSIZE == 64
    //staptest// [[[[kcmp (0, 0, KCMP_FILE, 0, 18446744073709551615)!!!!ni_syscall ()]]]] = -NNNN
#else
    //staptest// [[[[kcmp (0, 0, KCMP_FILE, 0, 4294967295)!!!!ni_syscall ()]]]] = -NNNN
#endif


    close(fd);
    unlink("kcmp_testfile");
    return 0;
}
#else
int main()
{
    return 0;
}
#endif /* ifdef __NR_kcmp */
