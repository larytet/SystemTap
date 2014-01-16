/* COVERAGE: getpgrp getpgid setpgid */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>

int main()
{
    pid_t pgid;

    pgid = getpgrp();
    // On some platforms (like RHEL5 ia64), getpgrp() is implemented
    // as getpgid(0).
    //staptest// [[[[getpgrp ()!!!!getpgid (0)]]]] = NNNN

    pgid = getpgid(0);
    //staptest// getpgid (0) = NNNN

    setpgid(0, 0);
    //staptest// setpgid (0, 0) = 0

    pgid = getpgrp();
    //staptest// [[[[getpgrp ()!!!!getpgid (0)]]]] = NNNN

    return 0;
}
