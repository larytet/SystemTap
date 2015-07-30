/* COVERAGE: accept4 */

#define _GNU_SOURCE
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
#include <sys/syscall.h>

// Note we can't test for __NR_accept4 here and avoid compilation if
// it isn't set, since on some platforms accept4() gets implemented
// via socketcall(). But, accept4 support was added to glibc in
// version 2.10.

int main()
{
#if __GLIBC_PREREQ(2, 10)
    int s, fd_null;
    struct sockaddr_in sin1;
    socklen_t sinlen;

    /* initialize sockaddr's */
    sin1.sin_family = AF_INET;
    sin1.sin_port = htons((getpid() % 32768) + 11000);
    sin1.sin_addr.s_addr = INADDR_ANY;

    fd_null = open("/dev/null", O_WRONLY);

    accept4(-1, NULL, NULL, 0);
    //staptest// accept4 (-1, 0x0, 0x0, 0x0) = -NNNN (EBADF)

    accept4(fd_null, (struct sockaddr *)&sin1, &sinlen, 0);
    //staptest// accept4 (NNNN, XXXX, XXXX, 0x0) = -NNNN (ENOTSOCK)

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    bind(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// bind (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    accept4(s, (struct sockaddr *)-1, &sinlen, 0);
    //staptest// accept4 (NNNN, 0x[f]+, XXXX, 0x0) = -NNNN (EINVAL)

    accept4(s, (struct sockaddr *)&sin1, (socklen_t *)-1, 0);
    //staptest// accept4 (NNNN, XXXX, 0x[f]+, 0x0) = -NNNN (EINVAL)

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 02000000
#endif
    accept4(s, (struct sockaddr *)&sin1, (socklen_t *)&sinlen, SOCK_CLOEXEC);
    //staptest// accept4 (NNNN, XXXX, XXXX, SOCK_CLOEXEC) = -NNNN (EINVAL)

    accept4(s, (struct sockaddr *)&sin1, (socklen_t *)&sinlen, -1);
    //staptest// accept4 (NNNN, XXXX, XXXX, SOCK_CLOEXEC|SOCK_NONBLOCK|0xfff7f7ff) = -NNNN (EINVAL)

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0
#endif

    return 0;
}
