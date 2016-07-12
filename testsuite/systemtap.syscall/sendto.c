/* COVERAGE: sendto */

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
    struct sockaddr_in sin1;
    pid_t pid = 0;
    char buf[1024];
    fd_set rdfds;
    struct timeval timeout;

    /* initialize sockaddr's */
    sin1.sin_family = AF_INET;
    sin1.sin_port = htons((getpid() % 32768) + 11000);
    sin1.sin_addr.s_addr = INADDR_ANY;

    signal(SIGPIPE, SIG_IGN);

    pid = start_server(&sin1);

    fd_null = open("/dev/null", O_WRONLY);

    memset(buf, 'a', sizeof(buf));

    sendto(-1, buf, sizeof(buf), 0, NULL, 0);
    //staptest// sendto (-1, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"..., 1024, 0x0, NULL, 0) = -NNNN (EBADF)

    memset(buf, 'b', sizeof(buf));

    sendto(fd_null, buf, sizeof(buf), MSG_DONTWAIT, NULL, 0);
    //staptest// sendto (NNNN, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"..., 1024, MSG_DONTWAIT, NULL, 0) = -NNNN (ENOTSOCK)

    s = socket(PF_INET, SOCK_DGRAM, 0);
    //staptest// socket (PF_INET, SOCK_DGRAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    memset(buf, 'c', sizeof(buf));

    sendto(s, (void *)buf, sizeof(buf), MSG_OOB, (struct sockaddr *)&sin1,
	   sizeof(sin1));
    //staptest// sendto (NNNN, "ccccccccccccccccccccccccccccccccccccccccccccc"..., 1024, MSG_OOB, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN (EOPNOTSUPP)

    sendto(s, (void *)-1, sizeof(buf), 0, (struct sockaddr *)&sin1,
	   sizeof(sin1));
    //staptest// sendto (NNNN, 0x[f]+, 1024, 0x0, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN (EFAULT)

    memset(buf, 'd', sizeof(buf));

    sendto(s, (void *)buf, -1, 0, (struct sockaddr *)&sin1, sizeof(sin1));
#if __WORDSIZE == 64
    //staptest// sendto (NNNN, "ddddddddddddddddddddddddddddddddddddddddddddd"..., 18446744073709551615, 0x0, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN
#else
    //staptest// sendto (NNNN, "ddddddddddddddddddddddddddddddddddddddddddddd"..., 4294967295, 0x0, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN
#endif

    close(s);
    //staptest// close (NNNN) = 0

    s = socket(PF_INET, SOCK_STREAM, 0);
    //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

    connect(s, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// connect (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = 0

    memset(buf, 'e', sizeof(buf));

    sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// sendto (NNNN, "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"..., 1024, 0x0, {AF_INET, 0.0.0.0, NNNN}, 16) = 1024

    memset(buf, 'f', sizeof(buf));

    sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&sin1, -1);
    //staptest// sendto (NNNN, "fffffffffffffffffffffffffffffffffffffffffffff"..., 1024, 0x0, {unknown .+}, 4294967295) = -NNNN (EINVAL)

    memset(buf, 'g', sizeof(buf));

    sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)-1, sizeof(sin1));
    //staptest// sendto (NNNN, "ggggggggggggggggggggggggggggggggggggggggggggg"..., 1024, 0x0, {\.\.\.}, 16) = -NNNN (EFAULT)

    memset(buf, 'h', sizeof(buf));

    // Ignore the return value on this sendto() call.  Note that on a
    // 32-bit kernel (i686 for instance), MAXSTRINGLEN is only
    // 256. Passing a -1 as the flags value can produce a string that
    // will cause argstr to get too big. So, we'll make the end of the
    // arguments optional.
    sendto(s, buf, sizeof(buf), -1, (struct sockaddr *)&sin1, sizeof(sin1));
    //staptest// sendto (NNNN, "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"..., 1024, MSG_[^ ]+[[[[|XXXX, {AF_INET, 0.0.0.0, NNNN}, 16]]]]?) = NNNN

    close(s);
    //staptest// close (NNNN) = 0

    close(fd_null);
    //staptest// close (NNNN) = 0

    if (pid > 0)
	(void)kill(pid, SIGKILL);	/* kill server */
    //staptest// kill (NNNN, SIGKILL) = 0

    return 0;
}
