/* COVERAGE: socketpair close */
#include <sys/types.h>
#include <sys/socket.h>

int main()
{
    int fds[2], fl, i;

    socketpair(0, SOCK_STREAM, 0, fds);
    //staptest// socketpair (PF_UNSPEC, SOCK_STREAM, 0, XXXX) = -NNNN (EAFNOSUPPORT)

    socketpair(PF_INET, 75, 0, fds);
    //staptest// socketpair (PF_INET, UNKNOWN VALUE: NNNN, 0, XXXX) = -NNNN (EINVAL)

    socketpair(PF_UNIX, SOCK_STREAM, 0, 0);
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM, 0, 0x0) = -NNNN (EFAULT)

    socketpair(PF_INET, SOCK_DGRAM, 17, fds);
    //staptest// socketpair (PF_INET, SOCK_DGRAM, 17, XXXX) = -NNNN (EOPNOTSUPP)

    socketpair(PF_INET, SOCK_DGRAM, 6, fds);
    //staptest// socketpair (PF_INET, SOCK_DGRAM, 6, XXXX) = -NNNN (EPROTONOSUPPORT)

    socketpair(PF_INET, SOCK_STREAM, 6, fds);
    //staptest// socketpair (PF_INET, SOCK_STREAM, 6, XXXX) = -NNNN (EOPNOTSUPP)

    socketpair(PF_INET, SOCK_STREAM, 1, fds);
    //staptest// socketpair (PF_INET, SOCK_STREAM, 1, XXXX) = -NNNN (EPROTONOSUPPORT)

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

#ifdef SOCK_NONBLOCK
    socketpair(PF_UNIX, SOCK_STREAM|SOCK_NONBLOCK, 0, fds);
    //staptest// socketpair (PF_LOCAL, SOCK_STREAM|SOCK_NONBLOCK, 0, XXXX) = 0

    close(fds[0]);
    //staptest// close (NNNN) = 0
    close(fds[1]);
    //staptest// close (NNNN) = 0
#endif
}
