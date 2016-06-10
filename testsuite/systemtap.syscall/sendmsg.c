/* COVERAGE: sendmsg */

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
    char buf[1024];

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
		cc = read(fd, buf, sizeof(buf));
		if (cc == 0 || (cc < 0 && errno != EINTR)) {
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
    struct sockaddr_in sin1, from;
    pid_t pid = 0;
    char buf[1024];
    fd_set rdfds;
    struct msghdr msgdat;
    struct iovec iov;
    socklen_t fromlen;

    /* initialize sockaddr's */
    sin1.sin_family = AF_INET;
    sin1.sin_port = htons((getpid() % 32768) + 11000);
    sin1.sin_addr.s_addr = INADDR_ANY;

    pid = start_server(&sin1);

    fromlen = sizeof(from);
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    msgdat.msg_name = &from;
    msgdat.msg_namelen = fromlen;
    msgdat.msg_iov = &iov;
    msgdat.msg_iovlen = 1;
    msgdat.msg_control = NULL;
    msgdat.msg_controllen = 0;
    msgdat.msg_flags = 0;

    fd_null = open("/dev/null", O_WRONLY);

    sendmsg(-1, &msgdat, 0);
    //staptest// sendmsg (-1, XXXX, 0x0) = -NNNN (EBADF)

    sendmsg(fd_null, &msgdat, MSG_DONTWAIT);
    //staptest// sendmsg (NNNN, XXXX, MSG_DONTWAIT) = -NNNN (ENOTSOCK)

    s = socket(PF_INET, SOCK_DGRAM, 0);
    //staptest// socket (PF_INET, SOCK_DGRAM, IPPROTO_IP) = NNNN

    sendmsg(s, &msgdat, MSG_OOB);
    //staptest// sendmsg (NNNN, XXXX, MSG_OOB) = -NNNN (EOPNOTSUPP)

    // Starting with glibc-2.23.90-19.fc25, this causes a SEGFAULT, so
    // we'll skip it.
#if !__GLIBC_PREREQ(2, 23)
    sendmsg(s, (struct msghdr *)-1, 0);
    //staptest// sendmsg (NNNN, 0x[f]+, 0x0) = -NNNN (EFAULT)
#endif

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    // Note that the exact return value can differ here, so we'll just
    // ignore it.
    sendmsg(s, &msgdat, -1);
    //staptest// sendmsg (NNNN, XXXX, MSG_[^ ]+[[[[|XXXX]]]]?)

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    sendmsg(s, &msgdat, MSG_DONTWAIT);
    //staptest// sendmsg (NNNN, XXXX, MSG_DONTWAIT) = 1024

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    if (pid > 0)
	(void)kill(pid, SIGKILL);	/* kill server */
    //staptest// kill (NNNN, SIGKILL) = 0

    return 0;
}
