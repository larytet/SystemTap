/* COVERAGE: getdents getdents64 */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


#define BUFSIZE 512L
struct linux_dirent64 *dirp64;
struct linux_dirent *dirp;

#ifdef __NR_getdents
static inline int __getdents(unsigned int fd, struct linux_dirent *dirp,
			     unsigned int count)
{
    return syscall(__NR_getdents, fd, dirp, count);
}
#endif

static inline int __getdents64(unsigned int fd, struct linux_dirent64 *dirp,
			       unsigned int count)
{
    return syscall(__NR_getdents64, fd, dirp, count);
}

int main() {
    int fd;
    void *buf;

    fd = open(".", O_RDONLY);
    buf = malloc(BUFSIZE);
    dirp64 = buf;
    dirp = buf;


    // --- successful calls ---

#ifdef __NR_getdents
    __getdents(fd, dirp, BUFSIZE);
    //staptest// getdents (NNNN, XXXX, 512) = NNNN
#endif

    __getdents64(fd, dirp64, BUFSIZE);
    //staptest// getdents (NNNN, XXXX, 512) = NNNN


    // --- nasty calls (getdents) ---

#ifdef __NR_getdents
    __getdents(-1, dirp, BUFSIZE);
    //staptest// getdents (-1, XXXX, 512) = NNNN

    __getdents(fd, (struct linux_dirent *)(unsigned long)-1L, BUFSIZE);
#ifdef __s390__
    //staptest// getdents (NNNN, 0x[7]?[f]+, 512) = NNNN
#else
    //staptest// getdents (NNNN, 0x[f]+, 512) = NNNN
#endif

    __getdents(fd, dirp, (unsigned int)-1);
    //staptest// getdents (NNNN, XXXX, 4294967295) = NNNN
#endif


    // --- nasty calls (getdents64) ---

    __getdents64(-1, dirp64, BUFSIZE);
    //staptest// getdents (-1, XXXX, 512) = NNNN

    __getdents64(fd, (struct linux_dirent64 *)(unsigned long)-1, BUFSIZE);
#ifdef __s390__
    //staptest// getdents (NNNN, 0x[7]?[f]+, 512) = NNNN
#else
    //staptest// getdents (NNNN, 0x[f]+, 512) = NNNN
#endif

    __getdents64(fd, dirp64, (unsigned int)-1);
    //staptest// getdents (NNNN, XXXX, 4294967295) = NNNN

    free(buf);
    close(fd);

    return 0;
}
