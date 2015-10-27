/* COVERAGE: pivot_root */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

int main()
{

    // We are not really going to let the syscall succeed.
    // Just sanity check it:

    syscall(__NR_pivot_root, "/tmp", "/");
    //staptest// pivot_root ("/tmp", "/") = NNNN

    syscall(__NR_pivot_root, (const char *)-1, "/tmp");
#ifdef __s390__
    //staptest// pivot_root (0x[7]?[f]+, "/tmp") = NNNN
#else
    //staptest// pivot_root (0x[f]+, "/tmp") = NNNN
#endif

    syscall(__NR_pivot_root, "/tmp", (const char *)-1);
#ifdef __s390__
    //staptest// pivot_root ("/tmp", 0x[7]?[f]+) = NNNN
#else
    //staptest// pivot_root ("/tmp", 0x[f]+) = NNNN
#endif

    return 0;
}
