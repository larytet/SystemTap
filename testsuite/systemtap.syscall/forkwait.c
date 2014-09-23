/* COVERAGE: fork wait4 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>

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

	return 0;
}
