/* COVERAGE: execveat */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#if !defined(SYS_execveat) && defined(__NR_execveat)
#define SYS_execveat __NR_execveat
#endif

#ifdef SYS_execveat
static inline int
__execveat(int dirfd, const char *filename, char *const argv[],
	   char *const envp[], int flags)
{
    return syscall(SYS_execveat, dirfd, filename, argv, envp, flags);
}
#endif

int main()
{
#ifdef SYS_execveat
    char *newargv[] = { "/bin/true", "a", "b", "cde", NULL };
    char *newenv[] = { "FOO=10", "BAR=20", NULL };

    /* Limit testing */
    __execveat(-1, (char *)-1, NULL, NULL, 0);
    //staptest// [[[[execveat (-1, 0x[f]+, \[\], \[\], 0x0)!!!!ni_syscall ()]]]] = -NNNN

    __execveat(-1, "/bin/true", (char **)-1, NULL, 0);
    //staptest// [[[[execveat (-1, "/bin/true", \[0x[f]+\], \[\], 0x0)!!!!ni_syscall ()]]]] = -NNNN

    __execveat(-1, "/bin/true", NULL, (char **)-1, 0);
    //staptest// [[[[execveat (-1, "/bin/true", \[\], \[0x[f]+\], 0x0)!!!!ni_syscall ()]]]] = -NNNN

    __execveat(-1, "/bin/true", NULL, NULL, -1);
    //staptest// [[[[execveat (-1, "/bin/true", \[\], \[\], AT_[^ ]+|XXXX)!!!!ni_syscall ()]]]] = -NNNN

    __execveat(AT_FDCWD, "", NULL, NULL, 0);
    //staptest// [[[[execveat (AT_FDCWD, "", \[\], \[\], 0x0)!!!!ni_syscall ()]]]] = -NNNN

    /* Regular testing. */
    __execveat(-1, newargv[0], newargv, newenv, 0);
    //staptest// [[[[execveat (-1, "/bin/true", \["/bin/true", "a", "b", "cde"\], \["FOO=10", "BAR=20"\], 0x0)!!!!ni_syscall ()]]]] = NNNN
#endif

    return 0;
}

