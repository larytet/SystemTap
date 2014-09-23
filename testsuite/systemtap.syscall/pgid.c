/* COVERAGE: getpgrp getpgid setpgid */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>

int main()
{
    pid_t pgid;
    pid_t pid = getpid();

    pgid = getpgrp();
    // On some platforms (like RHEL5 ia64), getpgrp() is implemented
    // as getpgid(0).
    //staptest// [[[[getpgrp ()!!!!getpgid (0)]]]] = NNNN

    pgid = getpgid(0);
    //staptest// getpgid (0) = NNNN

    (void)getpgid(-1);
    //staptest// getpgid (-1) = -NNNN (ESRCH)

    setpgid(0, 0);
    //staptest// setpgid (0, 0) = 0

    setpgid(pid, -1);
    //staptest// setpgid (NNNN, -1) = -NNNN (EINVAL)

    setpgid(-1, pgid);
    //staptest// setpgid (-1, NNNN) = -NNNN (ESRCH)

    pgid = getpgrp();
    //staptest// [[[[getpgrp ()!!!!getpgid (0)]]]] = NNNN

    return 0;
}
