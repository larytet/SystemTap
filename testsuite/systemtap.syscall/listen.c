/* COVERAGE: listen */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>

int main()
{
  int sock_stream;
  int sock_dgram;
  int fd_null;

  sock_stream = socket(PF_INET, SOCK_STREAM, 0);
  //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

  sock_dgram = socket(PF_INET, SOCK_DGRAM, 0);
  //staptest// socket (PF_INET, SOCK_DGRAM, IPPROTO_IP) = NNNN

  fd_null = open("/dev/null", O_WRONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_WRONLY) = NNNN

  listen(-1, 0);
  //staptest// listen (-1, 0) = -NNNN (EBADF)

  listen(fd_null, 0);
  //staptest// listen (NNNN, 0) = -NNNN (ENOTSOCK)

  listen(sock_dgram, 0);
  //staptest// listen (NNNN, 0) = -NNNN (EOPNOTSUPP)

  listen(sock_stream, 0);
  //staptest// listen (NNNN, 0) = 0

  listen(sock_stream, -1);
  //staptest// listen (NNNN, -1) = 0

  close(sock_stream);
  //staptest// close (NNNN) = 0

  close(sock_dgram);
  //staptest// close (NNNN) = 0

  close(fd_null);
  //staptest// close (NNNN) = 0

  return 0;
}
