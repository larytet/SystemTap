/* COVERAGE: mknod mknodat */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/fcntl.h>

#ifdef __NR_mknodat
static inline int
__mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev)
{
    return syscall(__NR_mknodat, dirfd, pathname, mode, dev);
}
#endif

int main()
{
    // ------- test normal operation

    mknod("testfile1", S_IFREG | 0644, 0);
    //staptest// [[[[mknod (!!!!mknodat (AT_FDCWD, ]]]]"testfile1", S_IFREG|0644, 0) = 0

#ifdef __NR_mknodat
    __mknodat(AT_FDCWD, "testfile2", S_IFREG | 0644, 0);
    //staptest// mknodat (AT_FDCWD, "testfile2", S_IFREG|0644, 0) = 0
#endif

    // ------- test nasty things

    mknod((const char *)-1, S_IFREG | 0644, 0);
#ifdef __s390__
    //staptest// [[[[mknod (!!!!mknodat (AT_FDCWD, ]]]]0x[7]?[f]+, S_IFREG|0644, 0) = NNNN
#else
    //staptest// [[[[mknod (!!!!mknodat (AT_FDCWD, ]]]]0x[f]+, S_IFREG|0644, 0) = NNNN
#endif

    mknod("testfile1", -1, 0);
    //staptest// [[[[mknod (!!!!mknodat (AT_FDCWD, ]]]]"testfile1", 037777777777, 0) = NNNN

#ifdef __NR_mknod
    syscall(__NR_mknod, "testfile1", 1, -1);
    //staptest// mknod ("testfile1", 01, 4294967295) = NNNN
#endif

#ifdef __NR_mknodat
    __mknodat(-1, "testfile2", S_IFREG | 0644, 0);
    //staptest// mknodat (-1, "testfile2", S_IFREG|0644, 0) = NNNN

    __mknodat(-1, "testfile2", S_IFCHR | 0644, 0);
    //staptest// mknodat (-1, "testfile2", S_IFCHR|0644, 0) = NNNN

    __mknodat(-1, "testfile2", S_IFBLK | 0644, 0);
    //staptest// mknodat (-1, "testfile2", S_IFBLK|0644, 0) = NNNN

    __mknodat(-1, "testfile2", S_IFIFO | 0644, 0);
    //staptest// mknodat (-1, "testfile2", S_IFIFO|0644, 0) = NNNN

    __mknodat(-1, "testfile2", S_IFSOCK | 0644, 0);
    //staptest// mknodat (-1, "testfile2", S_IFSOCK|0644, 0) = NNNN

    __mknodat(AT_FDCWD, (const char *)-1, S_IFREG | 0644, 0);
#ifdef __s390__
    //staptest// mknodat (AT_FDCWD, 0x[7]?[f]+, S_IFREG|0644, 0) = NNNN
#else
    //staptest// mknodat (AT_FDCWD, 0x[f]+, S_IFREG|0644, 0) = NNNN
#endif

    __mknodat(AT_FDCWD, "testfile2", -1, 0);
    //staptest// mknodat (AT_FDCWD, "testfile2", 037777777777, 0) = NNNN

    __mknodat(AT_FDCWD, "testfile2", 1, -1);
    //staptest// mknodat (AT_FDCWD, "testfile2", 01, 4294967295) = NNNN
#endif

    return 0;

}
