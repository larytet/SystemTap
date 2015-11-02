/* COVERAGE: fstatfs statfs ustat fstatfs64 statfs64 */

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ustat.h>
#include <sys/vfs.h>
#include <sys/syscall.h>
#include <sys/statfs.h>

// glibc mangles some ustat calls, so define our own using syscall().
#if defined(__NR_ustat)
static inline int __ustat(dev_t dev, struct ustat *ubuf)
{
    return syscall(__NR_ustat, dev, ubuf);
}
#endif

// Here's what the man page says about statfs64()/fstatfs64():
//
//   The original Linux statfs() and fstatfs() system calls were not
//   designed with extremely large file sizes in mind. Subsequently,
//   Linux 2.6 added new statfs64() and fstatfs64() system calls that
//   employ a new structure, statfs64. The new structure contains the
//   same fields as the original statfs structure, but the sizes of
//   various fields are increased, to accommodate large file
//   sizes. The glibc statfs() and fstatfs() wrapper functions
//   transparently deal with the kernel differences.
//
// Note that when _LARGEFILE64_SOURCE is defined, there actually *are*
// statfs64()/fstatfs64() glibc wrappers, but sometimes glibc still
// calls statfs()/fstatfs() anyway. Since we want to explicitly test
// statfs64()/fstatfs64(), we'll make our own wrappers. Also note that
// our wrappers include the 'sz' parameter, which the glibc wrappers
// don't.

#ifdef __NR_statfs64
static inline int __statfs64(const char *path, size_t sz, struct statfs64 *buf)
{
    return syscall(__NR_statfs64, path, sz, buf);
}
#endif

#ifdef __NR_fstatfs64
static inline int __fstatfs64(int fd, size_t sz, struct statfs64 *buf)
{
    return syscall(__NR_fstatfs64, fd, sz, buf);
}
#endif

int main()
{
    int fd;
    struct stat sbuf;
    struct statfs buf;
    struct statfs64 buf64;
#ifdef __NR_ustat
    struct ustat ubuf;
#endif

    fd = open("abc", O_WRONLY|O_CREAT, S_IRWXU);
    fstat(fd, &sbuf);

    // Since ustat() can fail on NFS filesystems, don't check for
    // success here.
#ifdef __NR_ustat
    ustat(sbuf.st_dev, &ubuf);
    //staptest// ustat (NNNN, XXXX) = NNNN
#endif

    statfs("abc", &buf);
    //staptest// statfs ("abc", XXXX) = 0

    fstatfs(fd, &buf);
    //staptest// fstatfs (NNNN, XXXX) = 0

#ifdef __NR_statfs64
    __statfs64("abc", sizeof(buf64), &buf64);
    //staptest// statfs64 ("abc", NNNN, XXXX) = 0
#endif

#ifdef __NR_fstatfs64
    __fstatfs64(fd, sizeof(buf64), &buf64);
    //staptest// fstatfs64 (NNNN, NNNN, XXXX) = 0
#endif

    /* Limit testing. */

#ifdef __NR_ustat
    __ustat(-1, &ubuf);
    //staptest// ustat (4294967295, XXXX) = NNNN

    ustat(sbuf.st_dev, (struct ustat *)-1);
#ifdef __s390__
    //staptest// ustat (NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// ustat (NNNN, 0x[f]+) = NNNN
#endif
#endif

    statfs((char *)-1, &buf);
#ifdef __s390__
    //staptest// statfs (0x[7]?[f]+, XXXX) = NNNN
#else
    //staptest// statfs (0x[f]+, XXXX) = NNNN
#endif

    statfs("abc", (struct statfs *)-1);
#ifdef __s390__
    //staptest// statfs ("abc", 0x[7]?[f]+) = NNNN
#else
    //staptest// statfs ("abc", 0x[f]+) = NNNN
#endif

    fstatfs(-1, &buf);
    //staptest// fstatfs (-1, XXXX) = NNNN

    fstatfs(fd, (struct statfs *)-1);
#ifdef __s390__
    //staptest// fstatfs (NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// fstatfs (NNNN, 0x[f]+) = NNNN
#endif

#ifdef __NR_statfs64
    __statfs64((char *)-1, sizeof(buf64), &buf64);
#ifdef __s390__
    //staptest// statfs64 (0x[7]?[f]+, NNNN, XXXX) = NNNN
#else
    //staptest// statfs64 (0x[f]+, NNNN, XXXX) = NNNN
#endif

    __statfs64("abc", (size_t)-1, NULL);
#if __WORDSIZE == 64
    //staptest// statfs64 ("abc", 18446744073709551615, 0x0) = NNNN
#else
    //staptest// statfs64 ("abc", 4294967295, 0x0) = NNNN
#endif

    __statfs64("abc", sizeof(buf64), (struct statfs64 *)-1);
#ifdef __s390__
    //staptest// statfs64 ("abc", NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// statfs64 ("abc", NNNN, 0x[f]+) = NNNN
#endif
#endif

#ifdef __NR_fstatfs64
    __fstatfs64(-1, sizeof(buf64), &buf64);
    //staptest// fstatfs64 (-1, NNNN, XXXX) = NNNN

    __fstatfs64(fd, (size_t)-1, NULL);
#if __WORDSIZE == 64
    //staptest// fstatfs64 (NNNN, 18446744073709551615, 0x0) = NNNN
#else
    //staptest// fstatfs64 (NNNN, 4294967295, 0x0) = NNNN
#endif

    __fstatfs64(fd, sizeof(buf64), (struct statfs64 *)-1);
#ifdef __s390__
    //staptest// fstatfs64 (NNNN, NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// fstatfs64 (NNNN, NNNN, 0x[f]+) = NNNN
#endif
#endif

    close(fd);
    return 0;
}
