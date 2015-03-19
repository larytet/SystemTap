/* COVERAGE: socketpair close */

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

int main()
{
    int fds[2], fl, i;

    socketpair(0, SOCK_STREAM, 0, fds);
    //staptest// socketpair (PF_UNSPEC, SOCK_STREAM, 0, XXXX) = -NNNN (EAFNOSUPPORT)

    socketpair(PF_INET, 75, IPPROTO_IP, fds);
    //staptest// socketpair (PF_INET, 0x4b, IPPROTO_IP, XXXX) = -NNNN (EINVAL)

    socketpair(PF_UNIX, SOCK_STREAM, 0, 0);
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM, 0, 0x0) = -NNNN (EFAULT)

    socketpair(PF_INET, SOCK_DGRAM, IPPROTO_UDP, fds);
    //staptest// socketpair (PF_INET, SOCK_DGRAM, IPPROTO_UDP, XXXX) = -NNNN (EOPNOTSUPP)

    socketpair(PF_INET, SOCK_DGRAM, IPPROTO_TCP, fds);
    //staptest// socketpair (PF_INET, SOCK_DGRAM, IPPROTO_TCP, XXXX) = -NNNN (EPROTONOSUPPORT)

    socketpair(PF_INET, SOCK_STREAM, IPPROTO_TCP, fds);
    //staptest// socketpair (PF_INET, SOCK_STREAM, IPPROTO_TCP, XXXX) = -NNNN (EOPNOTSUPP)

    socketpair(PF_INET, SOCK_STREAM, IPPROTO_ICMP, fds);
    //staptest// socketpair (PF_INET, SOCK_STREAM, IPPROTO_ICMP, XXXX) = -NNNN (EPROTONOSUPPORT)

    socketpair(PF_UNIX, SOCK_DGRAM, 0, fds);
    //staptest// socketpair (PF_LOCAL, SOCK_DGRAM, 0, XXXX) = 0

    close(fds[0]);
    //staptest// close (NNNN) = 0
    close(fds[1]);
    //staptest// close (NNNN) = 0

    socketpair(PF_UNIX, SOCK_STREAM, 0, fds);
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM, 0, XXXX) = 0

    close(fds[0]);
    //staptest// close (NNNN) = 0
    close(fds[1]);
    //staptest// close (NNNN) = 0

#ifdef SOCK_CLOEXEC
    socketpair(PF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0, fds);
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM|SOCK_CLOEXEC, 0, XXXX) = 0

    close(fds[0]);
    //staptest// close (NNNN) = 0
    close(fds[1]);
    //staptest// close (NNNN) = 0
#endif

#ifdef SOCK_NONBLOCK
    socketpair(PF_UNIX, SOCK_STREAM|SOCK_NONBLOCK, 0, fds);
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM|SOCK_NONBLOCK, 0, XXXX) = 0

    close(fds[0]);
    //staptest// close (NNNN) = 0
    close(fds[1]);
    //staptest// close (NNNN) = 0
#endif

    socketpair(-1, SOCK_STREAM, 0, fds);
    //staptest// socketpair (0xffffffff, SOCK_STREAM, 0, XXXX) = -NNNN (EAFNOSUPPORT)

    socketpair(PF_UNIX, -1, 0, fds);
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
    //staptest// socketpair (PF_LOCAL, SOCK_CLOEXEC|SOCK_NONBLOCK|0xfff7f7ff, 0, XXXX) = -NNNN (EINVAL)
#else
    //staptest// socketpair (PF_LOCAL, 0xffffffff, 0, XXXX) = -NNNN (EINVAL)
#endif

    socketpair(PF_UNIX, SOCK_STREAM, -1, fds);
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM, -1, XXXX) = -NNNN (EPROTONOSUPPORT)

    socketpair(PF_UNIX, SOCK_STREAM, 0, (int *)-1);
#ifdef __s390__
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM, 0, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM, 0, 0x[f]+) = -NNNN (EFAULT)
#endif
}
