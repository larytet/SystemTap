/* COVERAGE: lsetxattr lgetxattr llistxattr lremovexattr */

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
    lsetxattr(PATH, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE), XATTR_CREATE);
    //staptest// lsetxattr ("foobar", "user.systemtap.test", "testing", 8, XATTR_CREATE) = NNNN

    // Set a binary value.
    lsetxattr(PATH, XATTR_NAME "2", array, sizeof(array), XATTR_CREATE);
#if __BYTE_ORDER == __LITTLE_ENDIAN
    //staptest// lsetxattr ("foobar", "user.systemtap.test2", "\\xef\\xbe\\xad\\xde\\x02\\x00\\x00\\x00\\x03\\x00\\x00\\x00\\x04\\x00\\x00\\x00\\x05\\x00\\x00\\x00", 20, XATTR_CREATE) = NNNN
#elif __BYTE_ORDER == __BIG_ENDIAN
    //staptest// lsetxattr ("foobar", "user.systemtap.test2", "\\xde\\xad\\xbe\\xef\\x00\\x00\\x00\\x02\\x00\\x00\\x00\\x03\\x00\\x00\\x00\\x04\\x00\\x00\\x00\\x05", 20, XATTR_CREATE) = NNNN
#else
#error "byte order can't be determined"
#endif

    lgetxattr(PATH, XATTR_NAME, buffer, sizeof(buffer));
    //staptest// lgetxattr ("foobar", "user.systemtap.test", XXXX, 1024) = NNNN

    llistxattr(PATH, buffer, sizeof(buffer));
    //staptest// llistxattr ("foobar", XXXX, 1024) = NNNN

    lremovexattr(PATH, XATTR_NAME);
    //staptest// lremovexattr ("foobar", "user.systemtap.test") = NNNN

    // Limits testing.

    lsetxattr((char *)-1, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE),
	      XATTR_CREATE);
#ifdef __s390__
    //staptest// lsetxattr (0x[7]?[f]+, "user.systemtap.test", "testing", 8, XATTR_CREATE) = NNNN
#else
    //staptest// lsetxattr (0x[f]+, "user.systemtap.test", "testing", 8, XATTR_CREATE) = NNNN
#endif

    lsetxattr(PATH, (char *)-1, XATTR_VALUE, sizeof(XATTR_VALUE),
	      XATTR_CREATE);
#ifdef __s390__
    //staptest// lsetxattr ("foobar", 0x[7]?[f]+, "testing", 8, XATTR_CREATE) = NNNN
#else
    //staptest// lsetxattr ("foobar", 0x[f]+, "testing", 8, XATTR_CREATE) = NNNN
#endif

    lsetxattr(PATH, XATTR_NAME, (void *)-1, sizeof(XATTR_VALUE), XATTR_CREATE);
#ifdef __s390__
    //staptest// lsetxattr ("foobar", "user.systemtap.test", 0x[7]?[f]+, 8, XATTR_CREATE) = NNNN
#else
    //staptest// lsetxattr ("foobar", "user.systemtap.test", 0x[f]+, 8, XATTR_CREATE) = NNNN
#endif

    lsetxattr(PATH, XATTR_NAME, NULL, (size_t)-1, XATTR_CREATE);
#if __WORDSIZE == 64
    //staptest// lsetxattr ("foobar", "user.systemtap.test", 0x0, 18446744073709551615, XATTR_CREATE) = NNNN
#else
    //staptest// lsetxattr ("foobar", "user.systemtap.test", 0x0, 4294967295, XATTR_CREATE) = NNNN
#endif

    lsetxattr(PATH, XATTR_NAME, XATTR_VALUE, sizeof(XATTR_VALUE), -1);
    //staptest// lsetxattr ("foobar", "user.systemtap.test", "testing", 8, XATTR_[^ ]+|XXXX) = NNNN

    lgetxattr((char *)-1, XATTR_NAME, buffer, sizeof(buffer));
#ifdef __s390__
    //staptest// lgetxattr (0x[7]?[f]+, "user.systemtap.test", XXXX, 1024) = NNNN
#else
    //staptest// lgetxattr (0x[f]+, "user.systemtap.test", XXXX, 1024) = NNNN
#endif

    lgetxattr(PATH, (char *)-1, buffer, sizeof(buffer));
#ifdef __s390__
    //staptest// lgetxattr ("foobar", 0x[7]?[f]+, XXXX, 1024) = NNNN
#else
    //staptest// lgetxattr ("foobar", 0x[f]+, XXXX, 1024) = NNNN
#endif

    lgetxattr(PATH, XATTR_NAME, (void *)-1, sizeof(buffer));
#ifdef __s390__
    //staptest// lgetxattr ("foobar", "user.systemtap.test", 0x[7]?[f]+, 1024) = NNNN
#else
    //staptest// lgetxattr ("foobar", "user.systemtap.test", 0x[f]+, 1024) = NNNN
#endif

    lgetxattr(PATH, XATTR_NAME, buffer, (size_t)-1);
#if __WORDSIZE == 64
    //staptest// lgetxattr ("foobar", "user.systemtap.test", XXXX, 18446744073709551615) = NNNN
#else
    //staptest// lgetxattr ("foobar", "user.systemtap.test", XXXX, 4294967295) = NNNN
#endif

    llistxattr((char *)-1, buffer, sizeof(buffer));
#ifdef __s390__
    //staptest// llistxattr (0x[7]?[f]+, XXXX, 1024) = NNNN
#else
    //staptest// llistxattr (0x[f]+, XXXX, 1024) = NNNN
#endif

    llistxattr(PATH, (char *)-1, sizeof(buffer));
#ifdef __s390__
    //staptest// llistxattr ("foobar", 0x[7]?[f]+, 1024) = NNNN
#else
    //staptest// llistxattr ("foobar", 0x[f]+, 1024) = NNNN
#endif

    llistxattr(PATH, NULL, (size_t)-1);
#if __WORDSIZE == 64
    //staptest// llistxattr ("foobar", 0x0+, 18446744073709551615) = NNNN
#else
    //staptest// llistxattr ("foobar", 0x0+, 4294967295) = NNNN
#endif

    lremovexattr((char *)-1, XATTR_NAME);
#ifdef __s390__
    //staptest// lremovexattr (0x[7]?[f]+, "user.systemtap.test") = NNNN
#else
    //staptest// lremovexattr (0x[f]+, "user.systemtap.test") = NNNN
#endif

    lremovexattr(PATH, (char *)-1);
#ifdef __s390__
    //staptest// lremovexattr ("foobar", 0x[7]?[f]+) = NNNN
#else
    //staptest// lremovexattr ("foobar", 0x[f]+) = NNNN
#endif

    return 0;
}
