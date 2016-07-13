/* COVERAGE: sendmmsg */

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
#include <linux/net.h>

/* glibc sometimes gets too smart and substitutes sendmsg for
 * sendmmsg. So, we'll have to define our own sendmmsg(). */
#if defined(SYS_sendmmsg)
static inline int
sys_sendmmsg(int fd, struct mmsghdr *vmessages, unsigned int vlen, int flags)
{
    return syscall(SYS_sendmmsg, fd, vmessages, vlen, flags);
}
#endif

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
#if defined(SYS_sendmmsg)
    int s, fd_null;
    struct sockaddr_in sin1, from;
    pid_t pid = 0;
    char buf[2][1024];
    fd_set rdfds;
    struct mmsghdr msgs[2];
    struct iovec iov[2];
    socklen_t fromlen;

    /* initialize sockaddr's */
    sin1.sin_family = AF_INET;
    sin1.sin_port = htons((getpid() % 32768) + 11000);
    sin1.sin_addr.s_addr = INADDR_ANY;

    pid = start_server(&sin1);

    fromlen = sizeof(from);

    memset(msgs, 0, sizeof(msgs));
    iov[0].iov_base = buf[0];
    iov[0].iov_len = sizeof(buf[0]);
    msgs[0].msg_hdr.msg_iov = &iov[0];
    msgs[0].msg_hdr.msg_iovlen = 1;
    iov[1].iov_base = buf[1];
    iov[1].iov_len = sizeof(buf[1]);
    msgs[1].msg_hdr.msg_iov = &iov[1];
    msgs[1].msg_hdr.msg_iovlen = 1;

    fd_null = open("/dev/null", O_WRONLY);

    sys_sendmmsg(-1, msgs, 2, 0);
    //staptest// sendmmsg (-1, XXXX, 2, 0x0) = -NNNN (EBADF)

    sys_sendmmsg(fd_null, msgs, 2, MSG_DONTWAIT);
    //staptest// sendmmsg (NNNN, XXXX, 2, MSG_DONTWAIT) = -NNNN (ENOTSOCK)

    s = socket(PF_INET, SOCK_DGRAM, 0);
    //staptest// socket (PF_INET, SOCK_DGRAM, IPPROTO_IP) = NNNN

    sys_sendmmsg(s, msgs, 2, 0);
    //staptest// sendmmsg (NNNN, XXXX, 2, 0x0) = -NNNN (EDESTADDRREQ)

    sys_sendmmsg(s, (struct mmsghdr *)-1, 2, 0);
    //staptest// sendmmsg (NNNN, 0x[f]+, 2, 0x0) = -NNNN (EFAULT)

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    // Note that the exact failure return value can differ here, so
    // we'll just ignore it. Also note that on a 32-bit kernel (i686
    // for instance), MAXSTRINGLEN is only 256. Passing a -1 as the
    // flags value can produce a string that will cause argstr to get
    // too big. So, we'll make the end of the argument optional.
    sys_sendmmsg(s, msgs, 2, -1);
    //staptest// sendmmsg (NNNN, XXXX, 2, MSG_[^ ]+[[[[|XXXX]]]]?) = NNNN

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    // We don't want to wait for -1 vectors, since we'd be waiting for
    // a long time...
    sys_sendmmsg(s, msgs, -1, MSG_DONTWAIT);
    //staptest// sendmmsg (NNNN, XXXX, 4294967295, MSG_DONTWAIT)

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    sys_sendmmsg(s, msgs, 2, MSG_DONTWAIT);
    //staptest// sendmmsg (NNNN, XXXX, 2, MSG_DONTWAIT) = NNNN

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    if (pid > 0)
	(void)kill(pid, SIGKILL);	/* kill server */
    //staptest// kill (NNNN, SIGKILL) = 0
#endif

    return 0;
}
