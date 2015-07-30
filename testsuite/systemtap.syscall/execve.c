/* COVERAGE: execve */
#include <unistd.h>

int main()
{
    char *newargv[] = { "/bin/true", "a", "b", "cde", NULL };
    char *newenv[] = { "FOO=10", "BAR=20", NULL };

    /* Limit testing */
    execve((char *)-1, NULL, NULL);
#ifdef __s390__
    //staptest// execve ([7]?[f]+, \[\], \[/\* 0 vars \*/\]) = -NNNN (EFAULT)
#else
    //staptest// execve ([f]+, \[\], \[/\* 0 vars \*/\]) = -NNNN (EFAULT)
#endif

    execve(NULL, (char **)-1, NULL);
#ifdef __s390__
    //staptest// execve ( *(null), \[0x[7]?[f]+\], \[/\* 0 vars \*/\]) = -NNNN (EFAULT)
#else
    //staptest// execve ( *(null), \[0x[f]+\], \[/\* 0 vars \*/\]) = -NNNN (EFAULT)
#endif

    execve(NULL, NULL, (char **)-1);
    //staptest// execve ( *(null), \[\], \[/\* 0 vars \*/\]) = -NNNN

    /* Regular testing. */
    execve(newargv[0], newargv, newenv);
    //staptest// execve ("/bin/true", \["/bin/true", "a", "b", "cde"\], \[/\* 2 vars \*/\]) = NNNN

    return 0;
}

