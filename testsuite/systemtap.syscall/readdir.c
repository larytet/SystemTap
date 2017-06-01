/* COVERAGE: readdir old_readdir */
/* 'old_readdir' isn't a separate syscall from 'readdir'. On some
 * platforms (like i386), readdir is implemented via old_readdir. */

#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>

#define BUFSIZE 512


#ifdef __NR_readdir

long __readdir(unsigned int fd, void *dirent,
               unsigned int count) {
    return syscall(__NR_readdir, fd, dirent, count);
}


int main() {
    int fd;
    void *buf;

    fd = open(".", O_RDONLY);
    buf = malloc(BUFSIZE);

    // test normal operation

    __readdir(fd, buf, 0);
    //staptest// [[[[readdir (NNNN, XXXX, 0)!!!!ni_syscall ()]]]] = NNNN


    // test nasty calls

    __readdir(-1, buf, 0);
    //staptest// [[[[readdir (4294967295, XXXX, 0)!!!!ni_syscall ()]]]] = -NNNN

    __readdir(fd, (void *)-1, 0);
#ifdef __s390__
    //staptest// [[[[readdir (NNNN, 0x[7]?[f]+, 0)!!!!ni_syscall ()]]]] = NNNN
#else
    //staptest// [[[[readdir (NNNN, 0x[f]+, 0)!!!!ni_syscall ()]]]] = NNNN
#endif

    __readdir(fd, buf, -1);
    //staptest// [[[[readdir (NNNN, XXXX, 4294967295)!!!!ni_syscall ()]]]] = NNNN

    free(buf);
    close(fd);

    return 0;
}
#else /* __NR_readdir */
int main() {
    return 0;
}
#endif

