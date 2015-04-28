/* COVERAGE: set_tid_address */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

int main()
{
    int i;

    // set_tid_address() is being called as a part of the testcase
    // initialization. To ensure that our staptests are correctly
    // matching code under test, we'll do the limit testing first.
    // Using syscall() avoids link time issues.

    // Limit testing
    syscall(__NR_set_tid_address, (int *)-1);
#ifdef __s390__
    //staptest// set_tid_address (0x[7]?[f]+) = NNNN
#else
    //staptest// set_tid_address (0x[f]+) = NNNN
#endif

    // Test normal operation
    syscall(__NR_set_tid_address, &i);
    //staptest// set_tid_address (XXXX) = NNNN

    syscall(__NR_set_tid_address, NULL);
    //staptest// set_tid_address (0x0) = NNNN

    return 0;
}
