/* COVERAGE: copy_file_range */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>

#ifdef SYS_copy_file_range
static loff_t
copy_file_range(int fd_in, loff_t *off_in, int fd_out,
		loff_t *off_out, size_t len, unsigned int flags)
{
    return syscall(SYS_copy_file_range, fd_in, off_in, fd_out,
		   off_out, len, flags);
}
#endif

int main ()
{
#ifdef SYS_copy_file_range
    int fd_in, fd_out;
    loff_t off_in, off_out;
    char buf[] = "Hello world abcdefghijklmnopqrstuvwxyz 01234567890123456789";
    loff_t buf_size = sizeof(buf) - 1;

    /* Create a file with something in it. */
    fd_in = open("foobar", O_WRONLY|O_CREAT, 0666);
    write(fd_in, buf, buf_size);
    fsync(fd_in);
    close(fd_in);

    fd_in = open("foobar", O_RDONLY);
    fd_out = open("foobar2", O_WRONLY|O_CREAT, 0666);

    /* Copy 'foobar' to 'foobar2'. */
    off_in = off_out = 0;
    copy_file_range(fd_in, &off_in, fd_out, &off_out, buf_size, 0);
    //staptest// copy_file_range (NNNN, XXXX, NNNN, XXXX, 59, 0x0) = 59

    /* Test variations of the copy_file range to make sure that the
     * syscall tapset prints out the correct values. */
    copy_file_range(-1, &off_in, fd_out, &off_out, buf_size, 0);
    //staptest// copy_file_range (-1, XXXX, NNNN, XXXX, 59, 0x0) = -NNNN

    copy_file_range(fd_in, (loff_t*)-1, fd_out, &off_out, buf_size, 0);
#ifdef __s390__
    //staptest// copy_file_range (NNNN, 0x[7]?[f]+, NNNN, XXXX, 59, 0x0) = -NNNN
#else
    //staptest// copy_file_range (NNNN, 0x[f]+, NNNN, XXXX, 59, 0x0) = -NNNN
#endif

    copy_file_range(fd_in, &off_in, -1, &off_out, buf_size, 0);
    //staptest// copy_file_range (NNNN, XXXX, -1, XXXX, 59, 0x0) = -NNNN

    copy_file_range(fd_in, &off_in, fd_out, (loff_t*)-1, buf_size, 0);
#ifdef __s390__
    //staptest// copy_file_range (NNNN, XXXX, NNNN, 0x[7]?[f]+, 59, 0x0) = -NNNN
#else
    //staptest// copy_file_range (NNNN, XXXX, NNNN, 0x[f]+, 59, 0x0) = -NNNN
#endif

    copy_file_range(fd_in, &off_in, fd_out, &off_out, -1L, 0);
#if __WORDSIZE == 64
    //staptest// copy_file_range (NNNN, XXXX, NNNN, XXXX, 18446744073709551615, 0x0) = NNNN
#else
    //staptest// copy_file_range (NNNN, XXXX, NNNN, XXXX, 4294967295, 0x0) = NNNN
#endif

    /* Note: flags is unused and should be set to 0, otherwise an
     * error occurs. This may change if the syscall is developed. */
    copy_file_range(fd_in, &off_in, fd_out, &off_out, buf_size, -1);
    //staptest// copy_file_range (NNNN, XXXX, NNNN, XXXX, 59, 0xffffffff) = NNNN

    close(fd_out);
    close(fd_in);
    unlink("foobar");
    unlink("foobar2");
#endif

    return 0;
}
