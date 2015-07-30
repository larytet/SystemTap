/* COVERAGE: restart_syscall */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

int main()
{
    syscall(__NR_restart_syscall);
    //staptest// restart_syscall () = NNNN

    return 0;
}
