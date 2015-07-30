/* COVERAGE: getsockopt */

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
    int optval;
    socklen_t optlen;

    /* initialize local sockaddr */
    sin0.sin_family = AF_INET;
    sin0.sin_port = 0;
    sin0.sin_addr.s_addr = INADDR_ANY;
    sinlen = sizeof(sin1);
    optlen = sizeof(optval);

    sock_stream = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    bind(sock_stream, (struct sockaddr *)&sin0, sizeof(sin0));
    //staptest// bind (NNNN, {AF_INET, 0.0.0.0, 0}, 16) = 0

    fd_null = open("/dev/null", O_WRONLY);
    //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_WRONLY) = NNNN

    getsockopt(-1, SOL_SOCKET, SO_OOBINLINE, &optval, &optlen);
    //staptest// getsockopt (-1, SOL_SOCKET, SO_OOBINLINE, XXXX, XXXX) = -NNNN (EBADF)

    getsockopt(fd_null, SOL_SOCKET, SO_OOBINLINE, &optval, &optlen);
    //staptest// getsockopt (NNNN, SOL_SOCKET, SO_OOBINLINE, XXXX, XXXX) = -NNNN (ENOTSOCK)

    getsockopt(sock_stream, SOL_SOCKET, SO_OOBINLINE, (void *)-1, &optlen);
#ifdef __s390__
    //staptest// getsockopt (NNNN, SOL_SOCKET, SO_OOBINLINE, 0x[7]?[f]+, XXXX) = -NNNN (EFAULT)
#else
    //staptest// getsockopt (NNNN, SOL_SOCKET, SO_OOBINLINE, 0x[f]+, XXXX) = -NNNN (EFAULT)
#endif

    getsockopt(sock_stream, SOL_SOCKET, SO_OOBINLINE, &optval, (socklen_t *)-1);
#ifdef __s390__
    //staptest// getsockopt (NNNN, SOL_SOCKET, SO_OOBINLINE, XXXX, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// getsockopt (NNNN, SOL_SOCKET, SO_OOBINLINE, XXXX, 0x[f]+) = -NNNN (EFAULT)
#endif

    getsockopt(sock_stream, 500, SO_OOBINLINE, &optval, &optlen);
    //staptest// getsockopt (NNNN, 0x1f4, SO_OOBINLINE, XXXX, XXXX) = -NNNN (EOPNOTSUPP)

    getsockopt(sock_stream, -1, SO_OOBINLINE, &optval, &optlen);
    //staptest// getsockopt (NNNN, 0x[f]+, SO_OOBINLINE, XXXX, XXXX) = -NNNN (EOPNOTSUPP)

    getsockopt(sock_stream, IPPROTO_UDP, SO_OOBINLINE, &optval, &optlen);
    //staptest// getsockopt (NNNN, SOL_UDP, SO_OOBINLINE, XXXX, XXXX) = -NNNN (EOPNOTSUPP)

    getsockopt(sock_stream, IPPROTO_IP, -1, &optval, &optlen);
    //staptest// getsockopt (NNNN, SOL_IP, 0x[f]+, XXXX, XXXX) = -NNNN (ENOPROTOOPT)

    getsockopt(sock_stream, SOL_SOCKET, SO_OOBINLINE, &optval, &optlen);
    //staptest// getsockopt (NNNN, SOL_SOCKET, SO_OOBINLINE, XXXX, XXXX) = 0

    close(sock_stream);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    return 0;
}
