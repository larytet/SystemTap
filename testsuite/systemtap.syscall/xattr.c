/* COVERAGE: setxattr getxattr listxattr removexattr */

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
    close(fd);

    // Set a ascii value.
    setxattr(PATH, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE), XATTR_CREATE);
    //staptest// setxattr ("foobar", "user.systemtap.test", "testing", 8, XATTR_CREATE) = NNNN

    // Set a binary value.
    setxattr(PATH, XATTR_NAME "2", array, sizeof(array), XATTR_CREATE);
#if __BYTE_ORDER == __LITTLE_ENDIAN
    //staptest// setxattr ("foobar", "user.systemtap.test2", "\\xef\\xbe\\xad\\xde\\x02\\x00\\x00\\x00\\x03\\x00\\x00\\x00\\x04\\x00\\x00\\x00\\x05\\x00\\x00\\x00", 20, XATTR_CREATE) = NNNN
#elif __BYTE_ORDER == __BIG_ENDIAN
    //staptest// setxattr ("foobar", "user.systemtap.test2", "\\xde\\xad\\xbe\\xef\\x00\\x00\\x00\\x02\\x00\\x00\\x00\\x03\\x00\\x00\\x00\\x04\\x00\\x00\\x00\\x05", 20, XATTR_CREATE) = NNNN
#else
#error "byte order can't be determined"
#endif

    getxattr(PATH, XATTR_NAME, buffer, sizeof(buffer));
    //staptest// getxattr ("foobar", "user.systemtap.test", XXXX, 1024) = NNNN

    listxattr(PATH, buffer, sizeof(buffer));
    //staptest// listxattr ("foobar", XXXX, 1024) = NNNN

    removexattr(PATH, XATTR_NAME);
    //staptest// removexattr ("foobar", "user.systemtap.test") = NNNN

    // Limits testing.

    setxattr((char *)-1, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE),
	     XATTR_CREATE);
#ifdef __s390__
    //staptest// setxattr (0x[7]?[f]+, "user.systemtap.test", "testing", 8, XATTR_CREATE) = NNNN
#else
    //staptest// setxattr (0x[f]+, "user.systemtap.test", "testing", 8, XATTR_CREATE) = NNNN
#endif

    setxattr(PATH, (char *)-1, XATTR_VALUE, sizeof(XATTR_VALUE), XATTR_CREATE);
#ifdef __s390__
    //staptest// setxattr ("foobar", 0x[7]?[f]+, "testing", 8, XATTR_CREATE) = NNNN
#else
    //staptest// setxattr ("foobar", 0x[f]+, "testing", 8, XATTR_CREATE) = NNNN
#endif

    setxattr(PATH, XATTR_NAME, (void *)-1, sizeof(XATTR_VALUE), XATTR_CREATE);
#ifdef __s390__
    //staptest// setxattr ("foobar", "user.systemtap.test", 0x[7]?[f]+, 8, XATTR_CREATE) = NNNN
#else
    //staptest// setxattr ("foobar", "user.systemtap.test", 0x[f]+, 8, XATTR_CREATE) = NNNN
#endif

    setxattr(PATH, XATTR_NAME, NULL, (size_t)-1, XATTR_CREATE);
#if __WORDSIZE == 64
    //staptest// setxattr ("foobar", "user.systemtap.test", 0x0, 18446744073709551615, XATTR_CREATE) = NNNN
#else
    //staptest// setxattr ("foobar", "user.systemtap.test", 0x0, 4294967295, XATTR_CREATE) = NNNN
#endif

    setxattr(PATH, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE), -1);
    //staptest// setxattr ("foobar", "user.systemtap.test", "testing", 8, XATTR_[^ ]+|XXXX) = NNNN

    getxattr((char *)-1, XATTR_NAME, buffer, sizeof(buffer));
#ifdef __s390__
    //staptest// getxattr (0x[7]?[f]+, "user.systemtap.test", XXXX, 1024) = NNNN
#else
    //staptest// getxattr (0x[f]+, "user.systemtap.test", XXXX, 1024) = NNNN
#endif

    getxattr(PATH, (char *)-1, buffer, sizeof(buffer));
#ifdef __s390__
    //staptest// getxattr ("foobar", 0x[7]?[f]+, XXXX, 1024) = NNNN
#else
    //staptest// getxattr ("foobar", 0x[f]+, XXXX, 1024) = NNNN
#endif

    getxattr(PATH, XATTR_NAME, (void *)-1, sizeof(buffer));
#ifdef __s390__
    //staptest// getxattr ("foobar", "user.systemtap.test", 0x[7]?[f]+, 1024) = NNNN
#else
    //staptest// getxattr ("foobar", "user.systemtap.test", 0x[f]+, 1024) = NNNN
#endif

    getxattr(PATH, XATTR_NAME, buffer, (size_t)-1);
#if __WORDSIZE == 64
    //staptest// getxattr ("foobar", "user.systemtap.test", XXXX, 18446744073709551615) = NNNN
#else
    //staptest// getxattr ("foobar", "user.systemtap.test", XXXX, 4294967295) = NNNN
#endif

    listxattr((char *)-1, buffer, sizeof(buffer));
#ifdef __s390__
    //staptest// listxattr (0x[7]?[f]+, XXXX, 1024) = NNNN
#else
    //staptest// listxattr (0x[f]+, XXXX, 1024) = NNNN
#endif

    listxattr(PATH, (char *)-1, sizeof(buffer));
#ifdef __s390__
    //staptest// listxattr ("foobar", 0x[7]?[f]+, 1024) = NNNN
#else
    //staptest// listxattr ("foobar", 0x[f]+, 1024) = NNNN
#endif

    listxattr(PATH, NULL, (size_t)-1);
#if __WORDSIZE == 64
    //staptest// listxattr ("foobar", 0x0+, 18446744073709551615) = NNNN
#else
    //staptest// listxattr ("foobar", 0x0+, 4294967295) = NNNN
#endif

    removexattr((char *)-1, XATTR_NAME);
#ifdef __s390__
    //staptest// removexattr (0x[7]?[f]+, "user.systemtap.test") = NNNN
#else
    //staptest// removexattr (0x[f]+, "user.systemtap.test") = NNNN
#endif

    removexattr(PATH, (char *)-1);
#ifdef __s390__
    //staptest// removexattr ("foobar", 0x[7]?[f]+) = NNNN
#else
    //staptest// removexattr ("foobar", 0x[f]+) = NNNN
#endif

    return 0;
}
