/* COVERAGE: sendfile sendfile64 */
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

int main ()
{
	int fd, read_fd;
	int write_fd;
	off_t offset = 0;
	char buff[22]; // Note below 22 == EINVAL
	int ret;

	memset(buff, 5, sizeof(buff));
	
	/* create a file with something in it */
        fd = creat("foobar",S_IREAD|S_IWRITE);
	write(fd, buff, sizeof(buff));
	fsync(fd);
	close(fd);
	read_fd = open ("foobar", O_RDONLY);
	if (read_fd < 0) 
		return 1;

	/* Open the output file for writing */
	write_fd = creat("foobar2",S_IREAD|S_IWRITE|S_IRWXO);

	/* 
	 * For 2.6 the write_fd had to be a socket otherwise
	 * sendfile would fail. So we also test for failure here.
	 */
	ret = sendfile (write_fd, read_fd, &offset, sizeof(buff));
	//staptest// sendfile (NNNN, NNNN, XXXX, 22) = -?22

	sendfile (-1, read_fd, &offset, sizeof(buff));
	//staptest// sendfile (-1, NNNN, XXXX, 22) = -NNNN (EBADF)

	sendfile (write_fd, -1, &offset, sizeof(buff));
	//staptest// sendfile (NNNN, -1, XXXX, 22) = -NNNN (EBADF)

	sendfile (write_fd, read_fd, (off_t *)-1, sizeof(buff));
#ifdef __s390__
	//staptest// sendfile (NNNN, NNNN, 0x[7]?[f]+, 22) = -NNNN (EFAULT)
#else
	//staptest// sendfile (NNNN, NNNN, 0x[f]+, 22) = -NNNN (EFAULT)
#endif

	sendfile (write_fd, read_fd, &offset, -1);
	// The 'count' argument is a size_t, which is an unsigned
	// long, whose size varies by platform. (The return value
	// varies here as well, so we'll just ignore it.)
#if __WORDSIZE == 64
	//staptest// sendfile (NNNN, NNNN, XXXX, 18446744073709551615)
#else
	//staptest// sendfile (NNNN, NNNN, XXXX, 4294967295)
#endif
	close (read_fd);
	close (write_fd);
	unlink("foobar");
	unlink("foobar2");
	
	return 0;
}
