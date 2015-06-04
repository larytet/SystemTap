/* COVERAGE: getdents getdents64 */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


#define BUFSIZE 512
struct linux_dirent64 *dirp64;
struct linux_dirent *dirp;

int main() {
    int fd;
    void *buf;

    fd = open(".", O_RDONLY);
    buf = malloc(BUFSIZE);
    dirp64 = buf;
    dirp = buf;


    // --- successful calls ---

#ifdef __NR_getdents
    syscall(__NR_getdents, fd, &dirp, BUFSIZE);
    //staptest// getdents (NNNN, XXXX, 512) = NNNN
#endif

    syscall(__NR_getdents64, fd, &dirp64, BUFSIZE);
    //staptest// getdents (NNNN, XXXX, 512) = NNNN


    // --- nasty calls (getdents) ---

#ifdef __NR_getdents
    syscall(__NR_getdents, -1, &dirp, BUFSIZE);
    //staptest// getdents (-1, XXXX, 512) = NNNN

    syscall(__NR_getdents, fd, -1, BUFSIZE);
#ifdef __s390__
    //staptest// getdents (NNNN, 0x[7]?[f]+, 512) = NNNN
#else
    //staptest// getdents (NNNN, 0x[f]+, 512) = NNNN
#endif

    syscall(__NR_getdents, fd, &dirp, (unsigned int)-1);
    //staptest// getdents (NNNN, XXXX, 4294967295) = NNNN
#endif


    // --- nasty calls (getdents64) ---

    syscall(__NR_getdents64, -1, &dirp64, BUFSIZE);
    //staptest// getdents (-1, XXXX, 512) = NNNN

    syscall(__NR_getdents64, fd, -1, BUFSIZE);
#ifdef __s390__
    //staptest// getdents (NNNN, 0x[7]?[f]+, 512) = NNNN
#else
    //staptest// getdents (NNNN, 0x[f]+, 512) = NNNN
#endif

    syscall(__NR_getdents64, fd, &dirp64, (unsigned int)-1);
    //staptest// getdents (NNNN, XXXX, 4294967295) = NNNN

    free(buf);
    close(fd);

    return 0;
}
