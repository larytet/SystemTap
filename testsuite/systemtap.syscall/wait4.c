/* COVERAGE: wait4 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

static long get_pid_max(void)
{
    long pid_max = 32768;		/* default */

    FILE *fp = fopen("/proc/sys/kernel/pid_max", "r");
    if (fp) {
	fscanf(fp, "%ld", &pid_max);
	fclose(fp);
    }
    return pid_max;
}

int
main()
{
    pid_t epid = get_pid_max() + 1;
    pid_t child;

    wait4(epid, NULL, 0, NULL);
    //staptest// wait4 (NNNN, 0x0, 0x0, 0x0) = -NNNN (ECHILD)

    wait4(-1, NULL, 0, NULL);
    //staptest// wait4 (-1, 0x0, 0x0, 0x0) = -NNNN (ECHILD)

    child = fork();
#if !defined(__ia64__)
    // Sometimes glibc substitutes a clone() call for a fork()
    // call (verified with strace).
    //staptest// [[[[fork ()!!!!clone (.+, XXXX, XXXX, XXXX)]]]] = NNNN
#else
    // On RHEL5 ia64, fork() gets turned into clone2().
    //staptest// [[[[fork ()!!!!clone2 (.+, XXXX, XXXX, XXXX, XXXX)]]]] = NNNN
#endif
    if (!child) {
	int i = 0xfffff;
	while (i > 0) i--;
	exit(0);
    }
    wait4(child, (int *)-1, 0, NULL);
#ifdef __s390__
    //staptest// wait4 (NNNN, 0x[7]?[f]+, 0x0, 0x0) = -NNNN (EFAULT)
#else
    //staptest// wait4 (NNNN, 0x[f]+, 0x0, 0x0) = -NNNN (EFAULT)
#endif

    // Just in case the failing wait4() call above didn't clean up...
    wait4(child, 0, 0, NULL);
    //staptest// wait4 (NNNN, 0x0, 0x0, 0x0)

    child = fork();
#if !defined(__ia64__)
    // Sometimes glibc substitutes a clone() call for a fork()
    // call (verified with strace).
    //staptest// [[[[fork ()!!!!clone (.+, XXXX, XXXX, XXXX)]]]] = NNNN
#else
    // On RHEL5 ia64, fork() gets turned into clone2().
    //staptest// [[[[fork ()!!!!clone2 (.+, XXXX, XXXX, XXXX, XXXX)]]]] = NNNN
#endif
    if (!child) {
	int i = 0xfffff;
	while (i > 0) i--;
	exit(0);
    }

    // A -1 options value ends up looking like all options are set
    // (plus a few extra).
    wait4(child, NULL, -1, NULL);
    //staptest// wait4 (NNNN, 0x0, .+|XXXX, 0x0) = -NNNN (EINVAL)

    // Just in case the failing wait4() call above didn't clean up...
    wait4(child, 0, 0, NULL);
    //staptest// wait4 (NNNN, 0x0, 0x0, 0x0)

    child = fork();
#if !defined(__ia64__)
    // Sometimes glibc substitutes a clone() call for a fork()
    // call (verified with strace).
    //staptest// [[[[fork ()!!!!clone (.+, XXXX, XXXX, XXXX)]]]] = NNNN
#else
    // On RHEL5 ia64, fork() gets turned into clone2().
    //staptest// [[[[fork ()!!!!clone2 (.+, XXXX, XXXX, XXXX, XXXX)]]]] = NNNN
#endif
    if (!child) {
	int i = 0xfffff;
	while (i > 0) i--;
	exit(0);
    }

    wait4(child, NULL, 0, (struct rusage *)-1);
#ifdef __s390__
    //staptest// wait4 (NNNN, 0x0, 0x0, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// wait4 (NNNN, 0x0, 0x0, 0x[f]+) = -NNNN (EFAULT)
#endif

    // Just in case the failing wait4() call above didn't clean up...
    wait4(child, 0, 0, NULL);
    //staptest// wait4 (NNNN, 0x0, 0x0, 0x0)

    exit(0);
}
