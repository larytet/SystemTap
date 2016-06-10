/* COVERAGE: recvmmsg */

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
#include <limits.h>

static int sfd, ufd;	/* shared between start_server and do_child */

#define TM	"123456789+"

void sender(int fd)
{
	struct msghdr mh;
	struct iovec iov[1];

	/* Since sendmmsg() isn't available on RHEL6, even though
	 * recvmmsg() is available, just send 2 messages with sendmsg(). */
	memset(&mh, 0, sizeof(mh));

	/* set up msghdr */
	iov[0].iov_base = TM;
	iov[0].iov_len = sizeof(TM);
	mh.msg_iov = &iov[0];
	mh.msg_iovlen = 1;

	/* do it */
	(void)sendmsg(fd, &mh, 0);
	(void)sendmsg(fd, &mh, 0);
}

void do_child()
{
    struct sockaddr_in fsin;
    struct sockaddr_un fsun;
    fd_set afds, rfds;
    int nfds, cc, fd;

    FD_ZERO(&afds);
    FD_SET(sfd, &afds);
    FD_SET(ufd, &afds);

    nfds = FD_SETSIZE;

    /* accept connections until killed */
    while (1) {
	socklen_t fromlen;

	memcpy(&rfds, &afds, sizeof(rfds));

	if (select(nfds, &rfds, (fd_set *) 0, (fd_set *) 0,
		   (struct timeval *)0) < 0) {
	    if (errno != EINTR)
		exit(1);
	    continue;
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
	if (FD_ISSET(ufd, &rfds)) {
	    int newfd;

	    fromlen = sizeof(fsun);
	    newfd = accept(ufd, (struct sockaddr *)&fsun, &fromlen);
	    if (newfd >= 0)
		FD_SET(newfd, &afds);
	}
	for (fd = 0; fd < nfds; ++fd) {
	    if (fd != sfd && fd != ufd && FD_ISSET(fd, &rfds)) {
		char rbuf[1024];

		cc = read(fd, rbuf, sizeof(rbuf));
		if (cc && rbuf[0] == 'R')
		    sender(fd);
		if (cc == 0 || (cc < 0 && errno != EINTR)) {
		    (void)close(fd);
		    FD_CLR(fd, &afds);
		}
	    }
	}
    }
}

pid_t
start_server(struct sockaddr_in *ssin, struct sockaddr_un *ssun)
{
    pid_t pid;

    sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
	return -1;
    if (bind(sfd, (struct sockaddr *)ssin, sizeof(*ssin)) < 0)
	return -1;
    if (listen(sfd, 10) < 0)
	return -1;

    /* set up UNIX-domain socket */
    ufd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (ufd < 0)
	return -1;
    if (bind(ufd, (struct sockaddr *)ssun, sizeof(*ssun)))
	return -1;
    if (listen(ufd, 10) < 0)
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
    struct sockaddr_in sin1, sin2, sin4, from;
    pid_t pid = 0;
    char buf[2][1024];
    fd_set rdfds;
    struct timeval timeout;
    socklen_t fromlen;
    struct mmsghdr msgs[2];
    struct iovec iov[2];
    struct sockaddr_un sun1;
    char tmpsunpath[1024];
    struct timespec tim = {0, 20000};

    /* initialize sockaddr's */
    sin1.sin_family = AF_INET;
    sin1.sin_port = htons((getpid() % 32768) + 11000);
    sin1.sin_addr.s_addr = INADDR_ANY;

    (void)strcpy(tmpsunpath, "udsockXXXXXX");
    s = mkstemp(tmpsunpath);
    close(s);
    unlink(tmpsunpath);
    sun1.sun_family = AF_UNIX;
    strcpy(sun1.sun_path, tmpsunpath);

    pid = start_server(&sin1, &sun1);

    sin2.sin_family = AF_INET;
    /* this port must be unused! */
    sin2.sin_port = htons((getpid() % 32768) + 10000);
    sin2.sin_addr.s_addr = INADDR_ANY;

    sin4.sin_family = 47;	/* bogus address family */
    sin4.sin_port = 0;
    sin4.sin_addr.s_addr = htonl(0x0AFFFEFD);
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
    //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_WRONLY) = NNNN

    recvmmsg(-1, msgs, 2, 0, NULL);
    //staptest// recvmmsg (-1, XXXX, 2, 0x0, NULL) = -NNNN (EBADF)

    recvmmsg(fd_null, msgs, 2, MSG_DONTWAIT, NULL);
    //staptest// recvmmsg (NNNN, XXXX, 2, MSG_DONTWAIT, NULL) = -NNNN (ENOTSOCK)

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
    //staptest// [[[[select (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+]!!!!pselect6 (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+], 0x0]]]]) = 1

    // Starting with glibc-2.23.90-19.fc25, this causes a SEGFAULT, so
    // we'll skip it.
#if !__GLIBC_PREREQ(2, 23)
    recvmmsg(s, (struct mmsghdr *)-1, 2, 0, NULL);
#ifdef __s390__
    //staptest// recvmmsg (NNNN, 0x[7]?[f]+, 2, 0x0, NULL) = -NNNN (EFAULT)
#else
    //staptest// recvmmsg (NNNN, 0x[f]+, 2, 0x0, NULL) = -NNNN (EFAULT)
#endif
#endif

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
    //staptest// [[[[select (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+]!!!!pselect6 (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+], 0x0]]]]) = 1

    // Note that the exact failure return value can differ here, so
    // we'll just ignore it. Also note that on a 32-bit kernel (i686
    // for instance), MAXSTRINGLEN is only 256. Passing a -1 as the
    // flags value can produce a string that will cause argstr to get
    // too big. So, we'll make the end of the argument optional.
    recvmmsg(s, msgs, 2, -1, NULL);
    //staptest// recvmmsg (NNNN, XXXX, 2, MSG_[^ ]+[[[[|XXXX, NULL]]]]?) = -NNNN

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_UNIX, SOCK_STREAM, 0);
    //staptest// socket (PF_LOCAL, SOCK_STREAM, 0) = NNNN

    connect(s, (struct sockaddr *)&sun1, sizeof(sun1));
    //staptest// connect (NNNN, {AF_UNIX, "[^"]+"}, 110) = 0

    // We don't want to wait for -1 vectors, since we'd be waiting for
    // a long time...
    recvmmsg(s, msgs, -1, MSG_DONTWAIT, NULL);
    //staptest// recvmmsg (NNNN, XXXX, 4294967295, MSG_DONTWAIT, NULL) = -NNNN (EAGAIN)

    recvmmsg(s, msgs, 2, MSG_DONTWAIT, (struct timespec *)-1);
#ifdef __s390__
    //staptest// recvmmsg (NNNN, XXXX, 2, MSG_DONTWAIT, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// recvmmsg (NNNN, XXXX, 2, MSG_DONTWAIT, 0x[f]+) = -NNNN (EFAULT)
#endif

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_UNIX, SOCK_STREAM, 0);
    //staptest// socket (PF_LOCAL, SOCK_STREAM, 0) = NNNN

    connect(s, (struct sockaddr *)&sun1, sizeof(sun1));
    //staptest// connect (NNNN, {AF_UNIX, "[^"]+"}, 110) = 0

    write(s, "R", 1);
    //staptest// write (NNNN, "R", 1) = 1

    /* Wait for something to be readable */
    FD_ZERO(&rdfds);
    FD_SET(s, &rdfds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    select(s + 1, &rdfds, 0, 0, &timeout);
    //staptest// [[[[select (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+]!!!!pselect6 (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+], 0x0]]]]) = 1

    recvmmsg(s, msgs, 2, MSG_DONTWAIT, NULL);
    //staptest// recvmmsg (NNNN, XXXX, 2, MSG_DONTWAIT, NULL) = NNNN

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_UNIX, SOCK_STREAM, 0);
    //staptest// socket (PF_LOCAL, SOCK_STREAM, 0) = NNNN

    connect(s, (struct sockaddr *)&sun1, sizeof(sun1));
    //staptest// connect (NNNN, {AF_UNIX, "[^"]+"}, 110) = 0

    write(s, "R", 1);
    //staptest// write (NNNN, "R", 1) = 1

    /* Wait for something to be readable */
    FD_ZERO(&rdfds);
    FD_SET(s, &rdfds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    select(s + 1, &rdfds, 0, 0, &timeout);
    //staptest// [[[[select (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+]!!!!pselect6 (NNNN, XXXX, 0x[0]+, 0x[0]+, [2\.[0]+], 0x0]]]]) = 1

    recvmmsg(s, msgs, 2, MSG_DONTWAIT, &tim);
    //staptest// recvmmsg (NNNN, XXXX, 2, MSG_DONTWAIT, \[0.000020000\]) = NNNN

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    if (pid > 0)
	(void)kill(pid, SIGKILL);	/* kill server */
    //staptest// kill (NNNN, SIGKILL) = 0

    (void)unlink(tmpsunpath);

    return 0;
}
