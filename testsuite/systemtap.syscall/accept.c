/* COVERAGE: accept */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

int main()
{
    int s, fd_null;
    struct sockaddr_in sin1;
    socklen_t sinlen;

    /* On several platforms, glibc substitutes accept4() for
     * accept(). So, we'll just accept accept() or accept4(). */

    /* initialize sockaddr's */
    sin1.sin_family = AF_INET;
    sin1.sin_port = htons((getpid() % 32768) + 11000);
    sin1.sin_addr.s_addr = INADDR_ANY;
    sinlen = sizeof(sin1);

    fd_null = open("/dev/null", O_WRONLY);

    accept(-1, (struct sockaddr *)&sin1, &sinlen);
    //staptest// accept[4]? (-1, XXXX, XXXX[[[[, 0x0]]]]?) = -NNNN (EBADF)

    accept(fd_null, (struct sockaddr *)&sin1, &sinlen);
    //staptest// accept[4]? (NNNN, XXXX, XXXX[[[[, 0x0]]]]?) = -NNNN (ENOTSOCK)

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    bind(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// bind (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = NNNN

    accept(s, (struct sockaddr *)-1, &sinlen);
    //staptest// accept[4]? (NNNN, 0x[f]+, XXXX[[[[, 0x0]]]]?) = -NNNN (EINVAL)

    accept(s, (struct sockaddr *)&sin1, (socklen_t *)-1);
    //staptest// accept[4]? (NNNN, XXXX, 0x[f]+[[[[, 0x0]]]]?) = -NNNN (EINVAL)

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    return 0;
}
