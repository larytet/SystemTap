/* COVERAGE: recvmsg */

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

	memset(&mh, 0x00, sizeof(mh));

	/* set up msghdr */
	iov[0].iov_base = TM;
	iov[0].iov_len = sizeof(TM);
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;
	mh.msg_flags = 0;
	mh.msg_control = NULL;
	mh.msg_controllen = 0;

	/* do it */
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
		if (cc > 0 && rbuf[0] == 'R')
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
    char buf[1024];
    fd_set rdfds;
    struct timeval timeout;
    socklen_t fromlen;
    struct msghdr msgdat;
    struct iovec iov;
    struct sockaddr_un sun1;
    char tmpsunpath[1024];

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
    //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_WRONLY) = NNNN

    recvmsg(-1, &msgdat, 0);
    //staptest// recvmsg (-1, XXXX, 0x0) = -NNNN (EBADF)

    recvmsg(fd_null, &msgdat, MSG_DONTWAIT);
    //staptest// recvmsg (NNNN, XXXX, MSG_DONTWAIT) = -NNNN (ENOTSOCK)

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
    recvmsg(s, (struct msghdr *)-1, 0);
    //staptest// recvmsg (NNNN, 0x[f]+, 0x0) = -NNNN (EFAULT)
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
    recvmsg(s, &msgdat, -1);
    //staptest// recvmsg (NNNN, XXXX, MSG_[^ ]+[[[[|XXXX]]]]?) = -NNNN

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

    recvmsg(s, &msgdat, MSG_DONTWAIT);
    //staptest// recvmsg (NNNN, XXXX, MSG_DONTWAIT) = 11

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
