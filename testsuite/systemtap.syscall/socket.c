/* COVERAGE: socket */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

int main()
{
  int s;

  s = socket(PF_LOCAL, SOCK_STREAM, 0);
  //staptest// socket (PF_LOCAL, SOCK_STREAM, 0) = NNNN

  close(s);
  //staptest// close (NNNN) = 0

  socket(0, SOCK_STREAM, 0);
  //staptest// socket (PF_UNSPEC, SOCK_STREAM, 0) = -NNNN (EAFNOSUPPORT)

  socket(PF_INET, 75, IPPROTO_IP);
  //staptest// socket (PF_INET, UNKNOWN VALUE: NNNN, IPPROTO_IP) = -NNNN (EINVAL)

  s = socket(PF_LOCAL, SOCK_DGRAM, 0);
  //staptest// socket (PF_LOCAL, SOCK_DGRAM, 0) = NNNN

  close(s);
  //staptest// close (NNNN) = 0

  s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  //staptest// socket (PF_INET, SOCK_DGRAM, IPPROTO_IP) = NNNN

  close(s);
  //staptest// close (NNNN) = 0

  s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  //staptest// socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP) = NNNN

  close(s);
  //staptest// close (NNNN) = 0

  socket(PF_INET, SOCK_STREAM, IPPROTO_UDP);
  //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_UDP) = -NNNN (EPROTONOSUPPORT)

  socket(PF_INET, SOCK_DGRAM, IPPROTO_TCP);
  //staptest// socket (PF_INET, SOCK_DGRAM, IPPROTO_TCP) = -NNNN (EPROTONOSUPPORT)

  s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_TCP) = NNNN

  close(s);
  //staptest// close (NNNN) = 0

  socket(PF_INET, SOCK_STREAM, IPPROTO_ICMP);
  //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_ICMP) = -NNNN (EPROTONOSUPPORT)

#ifdef SOCK_CLOEXEC
  s = socket(PF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_IP);
  //staptest// socket (PF_INET, SOCK_STREAM|SOCK_CLOEXEC, IPPROTO_IP) = NNNN

  close(s);
  //staptest// close (NNNN) = 0
#endif

#ifdef SOCK_NONBLOCK
  s = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_IP);
  //staptest// socket (PF_INET, SOCK_STREAM|SOCK_NONBLOCK, IPPROTO_IP) = NNNN

  close(s);
  //staptest// close (NNNN) = 0
#endif

  return 0;
}
