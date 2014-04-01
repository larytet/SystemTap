/* COVERAGE: connect */

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

    connect(-1, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (-1, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN (EBADF)

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)-1, sizeof(sin1));
    //staptest// connect (NNNN, {\.\.\.\}, 16) = -NNNN (EFAULT)

    connect(s, (struct sockaddr *)&sin1, -1);
    //staptest// connect (NNNN, {unknown .+}, 4294967295) = -NNNN (EINVAL)

    connect(s, (struct sockaddr *)&sin1, 3);
    //staptest// connect (NNNN, {unknown .+}, 3) = -NNNN (EINVAL)

    connect(fd_null, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN (ENOTSOCK)

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN (EISCONN)

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin2, sizeof(sin2));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN (ECONNREFUSED)

    connect(s, (struct sockaddr *)&sin4, sizeof(sin4));
    //staptest// connect (NNNN, {unknown .+}, 16) = -NNNN (EAFNOSUPPORT)

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0
    
    if (pid > 0)
	(void)kill(pid, SIGKILL);	/* kill server */
    //staptest// kill (NNNN, SIGKILL) = 0

    return 0;
}
