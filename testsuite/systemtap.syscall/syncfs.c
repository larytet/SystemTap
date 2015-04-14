/* COVERAGE: sync_file_range sync_file_range2 */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main()
{
    int fd;
    char *string1 = "Hello world";

    // Create a test file.
    fd = creat("foobar", S_IREAD|S_IWRITE);
    write(fd, string1, sizeof(string1) - 1);


    syncfs(fd);
    //staptest// syncfs (NNNN) = 0

    /* Limit testing. */
    syncfs(-1);
    //staptest// syncfs (-1) = -NNNN

    close(fd);
    return 0;
}
