/* COVERAGE: chroot */

#include <unistd.h>

int main()
{
    chroot("/tmp");
    //staptest// chroot ("/tmp") = NNNN

    chroot((const char *)-1);
#ifdef __s390__
    //staptest// chroot ([7]?[f]+) = NNNN
#else
    //staptest// chroot ([f]+) = NNNN
#endif

    return 0;
}
