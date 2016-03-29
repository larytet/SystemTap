#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>

int main ()
{
  int fd_init, fd_in, fd_out;
  int off = 0;
  char buf [20];
  memset(buf, 9, sizeof(buf));

	/* create a file with something in it */
  fd_init = creat("foobar",S_IREAD|S_IWRITE);
	write(fd_init, buf, sizeof(buf));
	fsync(fd_init);
	close(fd_init);

  fd_in = open("foobar", S_IREAD);
  fd_out = creat("foobar2",S_IREAD|S_IWRITE|S_IRWXO);

  /* Test variations of the copy_file range to make sure that the syscall tapset
   * prints out the correct values
   * note: the 22 == EINVAL */
  copy_file_range(fd_in, &off, fd_out, &off, sizeof(buf), 0);
  //staptest// copy_file_range (NNNN, XXXX, NNNN, XXXX, 20, NNNN) = -?22

  copy_file_range(-1, &off, fd_out, &off, sizeof(buf), 0);
  //staptest// copy_file_range (-1, XXXX, NNNN, XXXX, 20, NNNN) = NNNN (EBADF)

  copy_file_range(fd_in, (off_t*)-1, fd_out, &off, sizeof(buf), 0);
  //staptest// copy_file_range (NNNN, 0x[7]?[f]+, NNNN, XXXX, 20, NNNN)

  copy_file_range(fd_in, &off, -1, &off, sizeof(buf), 0);
  //staptest// copy_file_range (NNNN, XXXX, -1, XXXX, 20, NNNN) = NNNN (EBADF)

  copy_file_range(fd_in, &off, fd_out, (off_t*)-1, sizeof(buf), 0);
  //staptest// copy_file_range (NNNN, XXXX, NNNN, 0x[7]?[f]+, 20, NNNN)

  copy_file_range(fd_in, &off, fd_out, &off, 123456789, 0);
  //staptest// copy_file_range (NNNN, XXXX, NNNN, XXXX, 123456789, NNNN)

  /* Note: flags is unused and should be set to 0, otherwise an error occurs.
   * this may change if the syscall is developed */
  copy_file_range(fd_in, &off, fd_out, &off, sizeof(buf), -1);
  //staptest// copy_file_range (NNNN, XXXX, NNNN, XXXX, 20, 0) = NNNN (EINVAL)

	close(fd_out);
	close(fd_in);
	unlink("foobar");
	unlink("foobar2");

  return 0;
}
