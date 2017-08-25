/* COVERAGE: execve */
#include <unistd.h>

int main()
{
    char *newargv[] = { "/bin/true", "a", "b", "cde", NULL };
    char *newenv[] = { "FOO=10", "BAR=20", NULL };

    /* Limit testing */
    execve((char *)-1, NULL, NULL);
#ifdef __s390__
    //staptest// execve (0x[7]?[f]+, \[\], \[\]) = -NNNN (EFAULT)
#else
    //staptest// execve (0x[f]+, \[\], \[\]) = -NNNN (EFAULT)
#endif

    execve(NULL, (char **)-1, NULL);
#ifdef __s390__
    //staptest// execve (0x0, \[0x[7]?[f]+\], \[\]) = -NNNN (EFAULT)
#else
    //staptest// execve (0x0, \[0x[f]+\], \[\]) = -NNNN (EFAULT)
#endif

    execve(NULL, NULL, (char **)-1);
#ifdef __s390__
    //staptest// execve (0x0, \[\], \[0x[7]?[f]+\]) = -NNNN
#else
    //staptest// execve (0x0, \[\], \[0x[f]+\]) = -NNNN
#endif

    /* Regular testing. */
    execve(newargv[0], newargv, newenv);
    //staptest// execve ("/bin/true", \["/bin/true", "a", "b", "cde"\], \["FOO=10", "BAR=20"\]) = NNNN

    return 0;
}

