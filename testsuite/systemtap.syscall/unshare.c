/* COVERAGE: unshare */

#define _GNU_SOURCE
#include <sched.h>

int main()
{
    unshare(CLONE_FILES);
    //staptest// unshare (CLONE_FILES) = 0

    unshare(-1);
    //staptest// unshare (CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_PTRACE|CLONE_VFORK|CLONE_PARENT|CLONE_THREAD|CLONE_NEWNS|CLONE_SYSVSEM|CLONE_SETTLS|CLONE_PARENT_SETTID[^)]+) = -NNNN

    return 0;
}
