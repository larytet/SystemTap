/* COVERAGE: flock */
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int fd;
char filename[100];


int main() {
    sprintf(filename, "flock-tst.%d", getpid());
    fd = creat(filename, 0644);
    if (fd < 0) {
        printf("ERROR");
        return 1;
    }

    // --- test normal operation ---

    flock(fd, LOCK_SH);
    //staptest// flock (NNNN, LOCK_SH) = 0

    flock(fd, LOCK_UN);
    //staptest// flock (NNNN, LOCK_UN) = 0

    flock(fd, LOCK_EX);
    //staptest// flock (NNNN, LOCK_EX) = 0

    flock(fd, LOCK_UN);
    //staptest// flock (NNNN, LOCK_UN) = 0

    // --- now try nasty things ---

    flock(-1, LOCK_SH);
    //staptest// flock (-1, LOCK_SH) = NNNN (EBADF)

    flock(fd, -1);
    //staptest// flock (NNNN, LOCK_[^ ]+|XXXX) = NNNN

    // The above should do LOCK_SH in fact.
    // So let's go ahead and call LOCK_UN at the end:
    flock(fd, LOCK_UN);

    close(fd);
    unlink(filename);
}
