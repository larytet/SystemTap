/* COVERAGE: mknod mknodat */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/fcntl.h>

inline int __mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev) {
    return syscall(__NR_mknodat, dirfd, pathname, mode, dev);
}

int main() {

    // ------- test normal operation

    mknod("testfile1", S_IFREG | 0644, 0);
    //staptest// mknod ("testfile1", S_IFREG|0644, 0) = 0

    __mknodat(AT_FDCWD, "testfile2", S_IFREG | 0644, 0);
    //staptest// mknodat (AT_FDCWD, "testfile2", S_IFREG|0644, 0) = 0

    // ------- test nasty things

    mknod((const char *)-1, S_IFREG | 0644, 0);
#ifdef __s390__
    //staptest// mknod ([7]?[f]+, S_IFREG|0644, 0) = NNNN
#else
    //staptest// mknod ([f]+, S_IFREG|0644, 0) = NNNN
#endif

    mknod("testfile1", -1, 0);
    //staptest// mknod ("testfile1", 037777777777, 0) = NNNN

    syscall(__NR_mknod, "testfile1", 1, -1);
    //staptest// mknod ("testfile1", 01, 4294967295) = NNNN

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
    //staptest// mknodat (AT_FDCWD, [7]?[f]+, S_IFREG|0644, 0) = NNNN
#else
    //staptest// mknodat (AT_FDCWD, [f]+, S_IFREG|0644, 0) = NNNN
#endif

    __mknodat(AT_FDCWD, "testfile2", -1, 0);
    //staptest// mknodat (AT_FDCWD, "testfile2", 037777777777, 0) = NNNN

    __mknodat(AT_FDCWD, "testfile2", 1, -1);
    //staptest// mknodat (AT_FDCWD, "testfile2", 01, 4294967295) = NNNN

    return 0;

}
