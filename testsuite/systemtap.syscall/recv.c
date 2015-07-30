/* COVERAGE: recv */

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

static int sfd;			/* shared between start_server and do_child */

void do_child()
{
    struct sockaddr_in fsin;
    fd_set afds, rfds;
    int nfds, cc, fd;
    char c;

    FD_ZERO(&afds);
    FD_SET(sfd, &afds);

    nfds = FD_SETSIZE;

    /* accept connections until killed */
    while (1) {
	socklen_t fromlen;

	memcpy(&rfds, &afds, sizeof(rfds));

	if (select(nfds, &rfds, (fd_set *) 0, (fd_set *) 0,
		   (struct timeval *)0) < 0) {
	    if (errno != EINTR)
		exit(1);
	}
	if (FD_ISSET(sfd, &rfds)) {
	    int newfd;

	    fromlen = sizeof(fsin);
	    newfd = accept(sfd, (struct sockaddr *)&fsin, &fromlen);
	    if (newfd >= 0)
		FD_SET(newfd, &afds);
	    /* send something back */
	    (void)write(newfd, "XXXXX\n", 6);
	}
	for (fd = 0; fd < nfds; ++fd) {
	    if (fd != sfd && FD_ISSET(fd, &rfds)) {
		if ((cc = read(fd, &c, 1)) == 0) {
		    (void)close(fd);
		    FD_CLR(fd, &afds);
		}
	    }
	}
    }
}

pid_t
start_server(struct sockaddr_in *sin0)
{
    struct sockaddr_in sin1 = *sin0;
    pid_t pid;

    sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
	return -1;
    if (bind(sfd, (struct sockaddr *)&sin1, sizeof(sin1)) < 0)
	return -1;
    if (listen(sfd, 10) < 0)
	return -1;

    switch (pid = fork()) {
    case 0:		/* child */
	do_child();
	break;
    case -1:			/* fall through */
    default:			/* parent */
	(void)close(sfd);
	return pid;
    }

    return -1;
}

int main()
{
    int s, fd_null;
    struct sockaddr_in sin1, sin2, sin4;
    pid_t pid = 0;
    char buf[1024];
    fd_set rdfds;
    struct timeval timeout;

    /* On several platforms, glibc substitutes recvfrom() for
     * recv(). We could assume that if SYS_recv is defined, we'll
     * actually use recv(), but that isn't true on all platforms. So,
     * we'll just accept recv() or recvfrom(). */

    /* initialize sockaddr's */
    sin1.sin_family = AF_INET;
    sin1.sin_port = htons((getpid() % 32768) + 11000);
    sin1.sin_addr.s_addr = INADDR_ANY;
    pid = start_server(&sin1);

    sin2.sin_family = AF_INET;
    /* this port must be unused! */
    sin2.sin_port = htons((getpid() % 32768) + 10000);
    sin2.sin_addr.s_addr = INADDR_ANY;

    sin4.sin_family = 47;	/* bogus address family */
    sin4.sin_port = 0;
    sin4.sin_addr.s_addr = htonl(0x0AFFFEFD);

    fd_null = open("/dev/null", O_WRONLY);

    recv(-1, buf, sizeof(buf), 0);
    //staptest// recv[[[[from]]]]? (-1, XXXX, 1024, 0x0[[[[, 0x0, 0x0]]]]?) = -NNNN (EBADF)

    recv(fd_null, buf, sizeof(buf), MSG_DONTWAIT);
    //staptest// recv[[[[from]]]]? (NNNN, XXXX, 1024, MSG_DONTWAIT[[[[, 0x0, 0x0]]]]?) = -NNNN (ENOTSOCK)

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    /* Wait for something to be readable, else we won't detect EFAULT */
    FD_ZERO(&rdfds);
    FD_SET(s, &rdfds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    select(s + 1, &rdfds, 0, 0, &timeout);
    //staptest// [[[[select (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+]!!!!pselect6 (NNNN, XXXX, 0x0, 0x0, \[2.[0]+\], 0x0]]]]) = 1

    recv(s, (void *)-1, sizeof(buf), 0);
    //staptest// recv[[[[from]]]]? (NNNN, 0x[f]+, 1024, 0x0[[[[, 0x0, 0x0]]]]?) = -NNNN (EFAULT)

    recv(s, buf, sizeof(buf), 0);
    //staptest// recv[[[[from]]]]? (NNNN, XXXX, 1024, 0x0[[[[, 0x0, 0x0]]]]?) = 6

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    /* Wait for something to be readable */
    FD_ZERO(&rdfds);
    FD_SET(s, &rdfds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    select(s + 1, &rdfds, 0, 0, &timeout);
    //staptest// [[[[select (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+]!!!!pselect6 (NNNN, XXXX, 0x[0]+, 0x[0]+, [2.\[0]+], 0x0]]]]) = 1

    recv(s, buf, sizeof(buf), -1);
    //staptest// recv[[[[from]]]]? (NNNN, XXXX, 1024, MSG_[^ ]+[[[[|XXXX, 0x0, 0x0]]]]?) = -NNNN

    recv(s, buf, (size_t)-1, MSG_DONTWAIT);
#if __WORDSIZE == 64
    //staptest// recv[[[[from]]]]? (NNNN, XXXX, 18446744073709551615, MSG_DONTWAIT[[[[, 0x0, 0x0]]]]?) = NNNN
#else
    //staptest// recv[[[[from]]]]? (NNNN, XXXX, 4294967295, MSG_DONTWAIT[[[[, 0x0, 0x0]]]]?) = NNNN
#endif

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    if (pid > 0)
	(void)kill(pid, SIGKILL);	/* kill server */
    //staptest// kill (NNNN, SIGKILL) = 0

    return 0;
}
