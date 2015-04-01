/* COVERAGE: chroot */

#include <unistd.h>

int main()
{
    chroot("/tmp");
    //staptest// chroot ("/tmp") = 0

    chroot((const char *)-1);
#ifdef __s390__
    //staptest// chroot ([7]?[f]+)
#else
    //staptest// chroot ([f]+)
#endif

    return 0;
}
