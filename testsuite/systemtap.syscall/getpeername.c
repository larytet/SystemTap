/* COVERAGE: getpeername */

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>

int main()
{
    int s;
    int fd_null;
    int sv[2];
    struct sockaddr_in sin0, sin1;
    socklen_t sinlen;

    /* initialize local sockaddr */
    sin0.sin_family = AF_INET;
    sin0.sin_port = 0;
    sin0.sin_addr.s_addr = INADDR_ANY;

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    bind(s, (struct sockaddr *)&sin0, sizeof(sin0));
    //staptest// bind (NNNN, {AF_INET, 0.0.0.0, 0}, 16) = 0

    fd_null = open("/dev/null", O_WRONLY);
    //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_WRONLY) = NNNN

    socketpair(PF_UNIX, SOCK_STREAM, 0, sv);
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM, 0, XXXX) = 0

    sinlen = sizeof(sin1);
    getpeername(-1, (struct sockaddr *)&sin1, &sinlen);
    //staptest// getpeername (-1, XXXX, XXXX) = -NNNN (EBADF)

    getpeername(fd_null, (struct sockaddr *)&sin1, &sinlen);
    //staptest// getpeername (NNNN, XXXX, XXXX) = -NNNN (ENOTSOCK)

    getpeername(s, (struct sockaddr *)&sin1, &sinlen);
    //staptest// getpeername (NNNN, XXXX, XXXX) = -NNNN (ENOTCONN)

    getpeername(sv[0], (struct sockaddr *)-1, &sinlen);
#ifdef __s390__
    //staptest// getpeername (NNNN, 0x[7]?[f]+, XXXX) = -NNNN (EFAULT)
#else
    //staptest// getpeername (NNNN, 0x[f]+, XXXX) = -NNNN (EFAULT)
#endif

    getpeername(sv[0], (struct sockaddr *)&sin1, (socklen_t *)0);
    //staptest// getpeername (NNNN, XXXX, 0x0) = -NNNN (EFAULT)

    getpeername(sv[0], (struct sockaddr *)&sin1, (socklen_t *)-1);
#ifdef __s390__
    //staptest// getpeername (NNNN, XXXX, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// getpeername (NNNN, XXXX, 0x[f]+) = -NNNN (EFAULT)
#endif

    getpeername(sv[0], (struct sockaddr *)&sin1, &sinlen);
    //staptest// getpeername (NNNN, XXXX, XXXX) = 0

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    close(sv[0]);
    //staptest// close (NNNN) = 0

    close(sv[1]);
    //staptest// close (NNNN) = 0

    return 0;
}
