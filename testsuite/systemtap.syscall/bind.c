/* COVERAGE: bind */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

int main()
{
  char buf[1024];
  int sock_inet_stream;
  int sock_local;
  int fd_null;
  struct sockaddr_in sin1, sin2, sin3;
  struct sockaddr_un sun1;

  sin1.sin_family = AF_INET;
  /* this port must be unused! */
  sin1.sin_port = htons((getpid() % 32768) + 10000);
  sin1.sin_addr.s_addr = INADDR_ANY;

  sin2.sin_family = AF_INET;
  sin2.sin_port = 0;
  sin2.sin_addr.s_addr = INADDR_ANY;

  sin3.sin_family = AF_INET;
  sin3.sin_port = 0;
  /* assumes 10.255.254.253 is not a local interface address! */
  sin3.sin_addr.s_addr = htonl(0x0AFFFEFD);

  sun1.sun_family = AF_UNIX;
  strncpy(sun1.sun_path, ".", sizeof(sun1.sun_path));

  sock_inet_stream = socket(PF_INET, SOCK_STREAM, 0);
  //staptest// socket (PF_INET, SOCK_STREAM, IPPROTO_IP) = NNNN

  sock_local = socket(PF_LOCAL, SOCK_STREAM, 0);
  //staptest// socket (PF_LOCAL, SOCK_STREAM, 0) = NNNN

  fd_null = open("/dev/null", O_WRONLY);
  //staptest// [[[[open (!!!!openat (AT_FDCWD, ]]]]"/dev/null", O_WRONLY) = NNNN

  bind(-1, (struct sockaddr *)&sin1, sizeof(sin1));
  //staptest// bind (-1, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN (EBADF)

  bind(sock_inet_stream, (struct sockaddr *)-1, sizeof(struct sockaddr_in));
  //staptest// bind (NNNN, {\.\.\.}, 16) = -NNNN (EFAULT)

  bind(sock_inet_stream, (struct sockaddr *)&sin1, 3);
  //staptest// bind (NNNN, {.+}, 3) = -NNNN (EINVAL)

  bind(fd_null, (struct sockaddr *)&sin1, sizeof(sin1));
  //staptest// bind (NNNN, {AF_INET, 0.0.0.0, NNNN}, 16) = -NNNN (ENOTSOCK)

  bind(sock_inet_stream, (struct sockaddr *)&sin2, sizeof(sin2));
  //staptest// bind (NNNN, {AF_INET, 0.0.0.0, 0}, 16) = 0

  bind(sock_local, (struct sockaddr *)&sun1, sizeof(sun1));
  //staptest// bind (NNNN, {AF_UNIX, "\."}, 110) = -NNNN (EADDRINUSE)

  bind(sock_inet_stream, (struct sockaddr *)&sin3, sizeof(sin3));
  //staptest// bind (NNNN, {AF_INET, 10.255.254.253, 0}, 16) = -NNNN (EADDRNOTAVAIL)

  bind(sock_inet_stream, (struct sockaddr *)&sin1, -1);
  //staptest// bind (NNNN, {unknown .+}, -1) = -NNNN (EINVAL)

  close(sock_inet_stream);
  //staptest// close (NNNN) = 0

  close(sock_local);
  //staptest// close (NNNN) = 0

  close(fd_null);
  //staptest// close (NNNN) = 0

  return 0;
}
