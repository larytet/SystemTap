/* COVERAGE: recvfrom */

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
    struct sockaddr_in sin1, sin2, sin4, from;
    pid_t pid = 0;
    char buf[1024];
    fd_set rdfds;
    struct timeval timeout;
    socklen_t fromlen;

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
    fromlen = sizeof(from);

    fd_null = open("/dev/null", O_WRONLY);
    //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_WRONLY) = NNNN

    recvfrom(-1, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
    //staptest// recvfrom (-1, XXXX, 1024, 0x0, XXXX, XXXX) = -NNNN (EBADF)

    recvfrom(fd_null, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&from,
	     &fromlen);
    //staptest// recvfrom (NNNN, XXXX, 1024, MSG_DONTWAIT, XXXX, XXXX) = -NNNN (ENOTSOCK)

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

    recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)-1, &fromlen);
#ifdef __s390__
    //staptest// recvfrom (NNNN, XXXX, 1024, 0x0, 0x[7]?[f]+, XXXX) = 6
#else
    //staptest// recvfrom (NNNN, XXXX, 1024, 0x0, 0x[f]+, XXXX) = 6
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

    recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from,
	     (socklen_t *)-1);
#ifdef __s390__
    //staptest// recvfrom (NNNN, XXXX, 1024, 0x0, XXXX, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// recvfrom (NNNN, XXXX, 1024, 0x0, XXXX, 0x[f]+) = -NNNN (EFAULT)
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

    recvfrom(s, (void *)-1, sizeof(buf), 0, (struct sockaddr *)&from,
	     &fromlen);
#ifdef __s390__
    //staptest// recvfrom (NNNN, 0x[7]?[f]+, 1024, 0x0, XXXX, XXXX) = -NNNN (EFAULT)
#else
    //staptest// recvfrom (NNNN, 0x[f]+, 1024, 0x0, XXXX, XXXX) = -NNNN (EFAULT)
#endif

    recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
    //staptest// recvfrom (NNNN, XXXX, 1024, 0x0, XXXX, XXXX) = 6

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
    // too big. So, we'll make the end of the arguments optional.
    recvfrom(s, buf, sizeof(buf), -1, (struct sockaddr *)&from, &fromlen);
    //staptest// recvfrom (NNNN, XXXX, 1024, MSG_[^ ]+[[[[|XXXX, XXXX, XXXX]]]]?) = -NNNN

    recvfrom(s, buf, (size_t)-1, MSG_DONTWAIT, (struct sockaddr *)&from,
	     &fromlen);
#if __WORDSIZE == 64
    //staptest// recvfrom (NNNN, XXXX, 18446744073709551615, MSG_DONTWAIT, XXXX, XXXX) = NNNN
#else
    //staptest// recvfrom (NNNN, XXXX, 4294967295, MSG_DONTWAIT, XXXX, XXXX) = NNNN
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
