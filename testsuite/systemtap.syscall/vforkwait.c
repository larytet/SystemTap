/* COVERAGE: vfork wait4 */
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
	
	child = vfork();
#if !(defined(__ia64__) || defined(__aarch64__))
	//staptest// vfork () = NNNN
#else
	// On RHEL5 ia64, vfork() gets turned into clone() (not
	// clone2() strangely enough).
	//staptest// [[[[vfork ()!!!!clone (.+, XXXX, XXXX, XXXX)]]]] = NNNN
#endif
	if (!child) {
		int i = 0xfffff;
		while (i > 0) i--;
		_exit(0);
	}
	wait4(child, &status, WNOHANG, NULL);
	//staptest// wait4 (NNNN, XXXX, WNOHANG, XXXX) = NNNN

	return 0;
}
