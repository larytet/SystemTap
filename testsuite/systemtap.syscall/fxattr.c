/* COVERAGE: fsetxattr fgetxattr flistxattr fremovexattr */

#include <stdio.h>
#include <unistd.h>
#include <sys/xattr.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PATH "foobar"
#define XATTR_NAME "user.systemtap.test"
#define XATTR_VALUE "testing"

int
main()
{
    char buffer[1024];
    int fd;
    unsigned int array[5] = { 0xdeadbeef, 2, 3, 4, 5 };

    fd = creat(PATH, 0666);

    // Set a ascii value.
    fsetxattr(fd, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE), XATTR_CREATE);
    //staptest// fsetxattr (NNNN, "user.systemtap.test", "testing", 8, XATTR_CREATE) = NNNN

    // Set a binary value.
    fsetxattr(fd, XATTR_NAME "2", array, sizeof(array), XATTR_CREATE);
#if __BYTE_ORDER == __LITTLE_ENDIAN
    //staptest// fsetxattr (NNNN, "user.systemtap.test2", "\\xef\\xbe\\xad\\xde\\x02\\x00\\x00\\x00\\x03\\x00\\x00\\x00\\x04\\x00\\x00\\x00\\x05\\x00\\x00\\x00", 20, XATTR_CREATE) = NNNN
#elif __BYTE_ORDER == __BIG_ENDIAN
    //staptest// fsetxattr (NNNN, "user.systemtap.test2", "\\xde\\xad\\xbe\\xef\\x00\\x00\\x00\\x02\\x00\\x00\\x00\\x03\\x00\\x00\\x00\\x04\\x00\\x00\\x00\\x05", 20, XATTR_CREATE) = NNNN
#else
#error "byte order can't be determined"
#endif

    fgetxattr(fd, XATTR_NAME, buffer, sizeof(buffer));
    //staptest// fgetxattr (NNNN, "user.systemtap.test", XXXX, 1024) = NNNN

    flistxattr(fd, buffer, sizeof(buffer));
    //staptest// flistxattr (NNNN, XXXX, 1024) = NNNN

    fremovexattr(fd, XATTR_NAME);
    //staptest// fremovexattr (NNNN, "user.systemtap.test") = NNNN

    // Limits testing.

    fsetxattr(-1, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE), XATTR_CREATE);
    //staptest// fsetxattr (-1, "user.systemtap.test", "testing", 8, XATTR_CREATE) = NNNN

    fsetxattr(fd, (char *)-1, XATTR_VALUE, sizeof(XATTR_VALUE), XATTR_CREATE);
#ifdef __s390__
    //staptest// fsetxattr (NNNN, 0x[7]?[f]+, "testing", 8, XATTR_CREATE) = NNNN
#else
    //staptest// fsetxattr (NNNN, 0x[f]+, "testing", 8, XATTR_CREATE) = NNNN
#endif

    fsetxattr(fd, XATTR_NAME, (void *)-1, sizeof(XATTR_VALUE), XATTR_CREATE);
#ifdef __s390__
    //staptest// fsetxattr (NNNN, "user.systemtap.test", 0x[7]?[f]+, 8, XATTR_CREATE) = NNNN
#else
    //staptest// fsetxattr (NNNN, "user.systemtap.test", 0x[f]+, 8, XATTR_CREATE) = NNNN
#endif

    fsetxattr(fd, XATTR_NAME, NULL, (size_t)-1, XATTR_CREATE);
#if __WORDSIZE == 64
    //staptest// fsetxattr (NNNN, "user.systemtap.test", 0x0, 18446744073709551615, XATTR_CREATE) = NNNN
#else
    //staptest// fsetxattr (NNNN, "user.systemtap.test", 0x0, 4294967295, XATTR_CREATE) = NNNN
#endif

    fsetxattr(fd, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE), -1);
    //staptest// fsetxattr (NNNN, "user.systemtap.test", "testing", 8, XATTR_[^ ]+|XXXX) = NNNN

    fgetxattr(-1, XATTR_NAME, buffer, sizeof(buffer));
    //staptest// fgetxattr (-1, "user.systemtap.test", XXXX, 1024) = NNNN

    fgetxattr(fd, (char *)-1, buffer, sizeof(buffer));
#ifdef __s390__
    //staptest// fgetxattr (NNNN, 0x[7]?[f]+, XXXX, 1024) = NNNN
#else
    //staptest// fgetxattr (NNNN, 0x[f]+, XXXX, 1024) = NNNN
#endif

    fgetxattr(fd, XATTR_NAME, (void *)-1, sizeof(buffer));
#ifdef __s390__
    //staptest// fgetxattr (NNNN, "user.systemtap.test", 0x[7]?[f]+, 1024) = NNNN
#else
    //staptest// fgetxattr (NNNN, "user.systemtap.test", 0x[f]+, 1024) = NNNN
#endif

    fgetxattr(fd, XATTR_NAME, buffer, (size_t)-1);
#if __WORDSIZE == 64
    //staptest// fgetxattr (NNNN, "user.systemtap.test", XXXX, 18446744073709551615) = NNNN
#else
    //staptest// fgetxattr (NNNN, "user.systemtap.test", XXXX, 4294967295) = NNNN
#endif

    flistxattr(-1, buffer, sizeof(buffer));
    //staptest// flistxattr (-1, XXXX, 1024) = NNNN

    flistxattr(fd, (char *)-1, sizeof(buffer));
#ifdef __s390__
    //staptest// flistxattr (NNNN, 0x[7]?[f]+, 1024) = NNNN
#else
    //staptest// flistxattr (NNNN, 0x[f]+, 1024) = NNNN
#endif

    flistxattr(fd, NULL, (size_t)-1);
#if __WORDSIZE == 64
    //staptest// flistxattr (NNNN, 0x0, 18446744073709551615) = NNNN
#else
    //staptest// flistxattr (NNNN, 0x0, 4294967295) = NNNN
#endif

    fremovexattr(-1, XATTR_NAME);
    //staptest// fremovexattr (-1, "user.systemtap.test") = NNNN

    fremovexattr(fd, (char *)-1);
#ifdef __s390__
    //staptest// fremovexattr (NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// fremovexattr (NNNN, 0x[f]+) = NNNN
#endif

    close(fd);
    return 0;
}
