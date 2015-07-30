/* COVERAGE: getuid16 geteuid16 getgid16 getegid16 setuid16 setresuid16 */
/* COVERAGE: getresuid16 setgid16 setresgid16 getresgid16 setreuid16 setregid16 */
/* COVERAGE: setfsuid16 setfsgid16 */

/* These are all obsolete 16-bit calls that are still there for compatibility. */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/syscall.h>

int main ()
{
#if __WORDSIZE != 64
	uid_t uid, ruid, euid, suid;
	gid_t gid, rgid, egid, sgid;

	uid = syscall(__NR_getuid);
	//staptest// getuid () = NNNN

	uid = syscall(__NR_geteuid);
	//staptest// geteuid () = NNNN

	gid = syscall(__NR_getgid);
	//staptest// getgid () = NNNN

	gid = syscall(__NR_getegid);
	//staptest// getegid () = NNNN

	syscall(__NR_setuid, 4096);
	//staptest// setuid (4096) = NNNN

	syscall(__NR_setresuid, -1, 4097, -1);
	//staptest// setresuid (-1, 4097, -1) = NNNN

	syscall(__NR_getresuid, &ruid, &euid, &suid);
	//staptest// getresuid (XXXX, XXXX, XXXX) = NNNN

	syscall(__NR_setgid, 4098);
	//staptest// setgid (4098) = NNNN

	syscall(__NR_setresgid, -1, 4099, -1);
	//staptest// setresgid (-1, 4099, -1) = NNNN
	
	syscall(__NR_getresgid, &rgid, &egid, &sgid);
	//staptest// getresgid (XXXX, XXXX, XXXX) = NNNN

	syscall(__NR_setreuid, -1, 5000);
	//staptest// setreuid (-1, 5000) = NNNN

	syscall(__NR_setreuid, 5001, -1);
	//staptest// setreuid (5001, -1) = NNNN

	syscall(__NR_setregid, -1, 5002);
	//staptest// setregid (-1, 5002) = NNNN

	syscall(__NR_setregid, 5003, -1);
	//staptest// setregid (5003, -1) = NNNN

	syscall(__NR_setfsuid, 5004);
	//staptest// setfsuid (5004) = NNNN
	
	syscall(__NR_setfsgid, 5005);
	//staptest// setfsgid (5005) = NNNN
#endif

	return 0;
}	
