/* COVERAGE: shutdown */

#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main()
{
  char buf[1024];
  int s;
  int fd_null;
  struct sockaddr_in sin1;

  sin1.sin_family = AF_INET;
  /* this port must be unused! */
  sin1.sin_port = htons((getpid() % 32768) + 10000);
  sin1.sin_addr.s_addr = INADDR_ANY;

  fd_null = open("/dev/null", O_WRONLY);

  shutdown(-1, SHUT_RD);
  //staptest// shutdown (-1, SHUT_RD) = -NNNN (EBADF)

  shutdown(fd_null, SHUT_WR);
  //staptest// shutdown (NNNN, SHUT_WR) = -NNNN (ENOTSOCK)

  s = socket(PF_INET, SOCK_STREAM, 0);
  //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

  shutdown(s, SHUT_RDWR);
  //staptest// shutdown (NNNN, SHUT_RDWR) = -NNNN (ENOTCONN)

  close(s);
  //staptest// close (NNNN) = 0
  
  s = socket(PF_INET, SOCK_STREAM, 0);
  //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

  shutdown(s, -1);
  //staptest// shutdown (NNNN, 0xffffffff) = -NNNN (EINVAL)

  close(s);
  //staptest// close (NNNN) = 0

  close(fd_null);
  //staptest// close (NNNN) = 0

  return 0;
}
