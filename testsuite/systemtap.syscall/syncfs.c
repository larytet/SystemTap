/* COVERAGE: sync_file_range sync_file_range2 sync syncfs */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>

int main()
{
    int fd;
    char *string1 = "Hello world";

    sync();
    //staptest// sync () = 0

    // Create a test file.
    fd = creat("foobar", S_IREAD|S_IWRITE);
    write(fd, string1, sizeof(string1) - 1);

    // We use syscall() to avoid link time problems
#ifdef __NR_syncfs
    syscall(__NR_syncfs, fd);
    //staptest// syncfs (NNNN) = 0

    syscall(__NR_syncfs, (int)-1);
    //staptest// syncfs (-1) = -NNNN
#endif

    close(fd);
    return 0;
}
