/* COVERAGE: getsockname */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main()
{
    int sock_stream;
    int fd_null;
    struct sockaddr_in sin0, sin1;
    socklen_t sinlen;

    /* initialize local sockaddr */
    sin0.sin_family = AF_INET;
    sin0.sin_port = 0;
    sin0.sin_addr.s_addr = INADDR_ANY;
    sinlen = sizeof(sin1);

    sock_stream = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    bind(sock_stream, (struct sockaddr *)&sin0, sizeof(sin0));
    //staptest// bind (NNNN, {AF_INET, 0.0.0.0, 0}, 16) = 0

    fd_null = open("/dev/null", O_WRONLY);
    //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_WRONLY) = NNNN

    getsockname(-1, (struct sockaddr *)&sin1, &sinlen);
    //staptest// getsockname (-1, XXXX, XXXX) = -NNNN (EBADF)

    getsockname(fd_null, (struct sockaddr *)&sin1, &sinlen);
    //staptest// getsockname (NNNN, XXXX, XXXX) = -NNNN (ENOTSOCK)

    getsockname(sock_stream, (struct sockaddr *)-1, &sinlen);
#ifdef __s390__
    //staptest// getsockname (NNNN, 0x[7]?[f]+, XXXX) = -NNNN (EFAULT)
#else
    //staptest// getsockname (NNNN, 0x[f]+, XXXX) = -NNNN (EFAULT)
#endif

    getsockname(sock_stream, (struct sockaddr *)&sin1, (socklen_t *)-1);
#ifdef __s390__
    //staptest// getsockname (NNNN, XXXX, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// getsockname (NNNN, XXXX, 0x[f]+) = -NNNN (EFAULT)
#endif

    getsockname(sock_stream, (struct sockaddr *)&sin1, (socklen_t *)&sinlen);
    //staptest// getsockname (NNNN, XXXX, XXXX) = 0

    close(sock_stream);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    return 0;
}
