/* COVERAGE: wait waitpid waitid */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/syscall.h>

// To test for glibc support for waitid:
//
//   _SVID_SOURCE || _XOPEN_SOURCE >= 500 ||
//   _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED
//   || /* Since glibc 2.12: */ _POSIX_C_SOURCE >= 200809L

#define GLIBC_SUPPORT \
    (_SVID_SOURCE || _XOPEN_SOURCE >= 500 ||  \
     _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED \
     || /* Since glibc 2.12: */ _POSIX_C_SOURCE >= 200809L)

int
main()
{
    pid_t child;
    int status;
#ifdef GLIBC_SUPPORT
    siginfo_t info;
#endif

    /* wait() can be implemented by:
     *
     *   waitpid(-1, &status 0);
     *
     * or:
     *
     *   wait4(-1, &status, 0, NULL);
     *
     * So, on many platforms it doesn't really exist.
     */
#ifdef __NR_wait
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
	sleep(1);
	exit(0);
    }
    wait(&status);
    //staptest// wait (XXXX) = NNNN

    /* Limit testing. */
    wait((int *)-1);
#ifdef __s390__
    //staptest// wait (0x[7]?[f]+) = -NNNN
#else
    //staptest// wait (0x[f]+) = -NNNN
#endif
#endif

    /* waitpid() can be implemented by:
     *
     *   wait4(-1, &status, 0, NULL);
     *
     * So, on many platforms it doesn't really exist.
     */
#ifdef __NR_waitpid
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
	sleep(1);
	exit(0);
    }
    waitpid(-1, &status, 0);
    //staptest// waitpid (-1, XXXX, 0x0) = NNNN

    /* Limit testing. */
    waitpid(-1, (int *)-1, WNOHANG);
#ifdef __s390__
    //staptest// waitpid (-1, 0x[7]?[f]+, WNOHANG) = NNNN
#else
    //staptest// waitpid (-1, 0x[f]+, WNOHANG) = NNNN
#endif

    waitpid(-1, &status, -1);
    //staptest// waitpid (-1, XXXX, W[^ ]+|XXXX) = -NNNN
#endif

#ifdef GLIBC_SUPPORT
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
	sleep(1);
	exit(0);
    }
    waitid(P_ALL, 0, &info, WEXITED);
    //staptest// waitid (P_ALL, 0, XXXX, WEXITED, 0x0) = 0

    /* Limit testing. */
    waitid(-1, 0, &info, 0);
    //staptest// waitid (0xffffffff, 0, XXXX, 0x0, 0x0) = NNNN

    waitid(P_ALL, -1, &info, 0);
    //staptest// waitid (P_ALL, -1, XXXX, 0x0, 0x0) = NNNN

    waitid(P_ALL, 0, (siginfo_t *)-1, 0);
#ifdef __s390__
    //staptest// waitid (P_ALL, 0, 0x[7]?[f]+, 0x0, 0x0) = NNNN
#else
    //staptest// waitid (P_ALL, 0, 0x[f]+, 0x0, 0x0) = NNNN
#endif

    waitid(P_ALL, 0, &info, -1);
    //staptest// waitid (P_ALL, 0, XXXX, W[^ ]+|XXXX, 0x0) = NNNN
#endif
    exit(0);
}
