/* COVERAGE: fork wait4 getpid getppid gettid */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

int main ()
{
	pid_t child;
	int status;
	
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
	wait4(child, &status, WNOHANG, NULL);
	//staptest// wait4 (NNNN, XXXX, WNOHANG, XXXX) = NNNN

	// we need syscall() otherwise glibc would eliminate the system call
	syscall(__NR_getpid);
	//staptest// getpid () = NNNN

	getppid();
	//staptest// getppid () = NNNN

	// Note: There is no glibc wrapper for this system call
	syscall(__NR_gettid);
	//staptest// gettid () = NNNN

	return 0;
}
