/* COVERAGE: exit */

#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>

int main ()
{
    // Note we have to use syscall here, since glibc's exit() function
    // calls exit_group() instead of exit() since glibc 2.3.
    syscall(SYS_exit, (int)-1);
    //staptest// exit (-1) =
    return 0;
}
