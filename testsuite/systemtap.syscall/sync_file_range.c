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
    lseek(fd, 8096, SEEK_CUR);
    write(fd, string1, sizeof(string1) - 1);

    sync_file_range(fd, 4096, 4096, SYNC_FILE_RANGE_WRITE);
    //staptest// sync_file_range (NNNN, 4096, 4096, SYNC_FILE_RANGE_WRITE) = 0

    /* Limit testing. */

    sync_file_range(-1, 4096, 4096, SYNC_FILE_RANGE_WRITE);
    //staptest// sync_file_range (-1, 4096, 4096, SYNC_FILE_RANGE_WRITE) = NNNN

    sync_file_range(fd, -1, 4096, SYNC_FILE_RANGE_WAIT_BEFORE);
    //staptest// sync_file_range (NNNN, -1, 4096, SYNC_FILE_RANGE_WAIT_BEFORE) = NNNN

    sync_file_range(fd, 0x12345678deadbeefLL, 4096, SYNC_FILE_RANGE_WRITE);
    //staptest// sync_file_range (NNNN, 1311768468603649775, 4096, SYNC_FILE_RANGE_WRITE) = NNNN

    sync_file_range(fd, 4096, -1, SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
    //staptest// sync_file_range (NNNN, 4096, -1, SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER) = NNNN

    sync_file_range(fd, 4096, 0x12345678deadbeefLL, SYNC_FILE_RANGE_WRITE);
    //staptest// sync_file_range (NNNN, 4096, 1311768468603649775, SYNC_FILE_RANGE_WRITE) = NNNN

    sync_file_range(fd, 4096, 4096, -1);
    //staptest// sync_file_range (NNNN, 4096, 4096, SYNC_[^ ]+|XXXX) = NNNN

    close(fd);
    return 0;
}
