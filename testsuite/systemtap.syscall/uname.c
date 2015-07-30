/* COVERAGE: uname olduname oldolduname newuname */

/* Sigh. In the i386 syscall table you'll see this:
 *
 * ===
 * # The format is:
 * # <number> <abi> <name> <entry point> <compat entry point>
 * 59	i386	oldolduname		sys_olduname
 * 109	i386	olduname		sys_uname
 * 122	i386	uname			sys_newuname
 * ===
 *
 * So, we have to try to test all the ones that exist on this system.
 */
#define _GNU_SOURCE

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/utsname.h>

/* We'll need to define our own syscall wrappers, so we know which
 * syscall we are calling.
 */
#ifdef __NR_uname
static inline int __uname(struct new_utsname *buf)
{
    return syscall(__NR_uname, buf);
}
#endif

#ifdef __NR_olduname
static inline int __olduname(struct old_utsname *buf)
{
    return syscall(__NR_olduname, buf);
}
#endif

#ifdef __NR_oldolduname
static inline int __oldolduname(struct oldold_utsname *buf)
{
    return syscall(__NR_oldolduname, buf);
}
#endif

int main()
{
    struct oldold_utsname oldold_uts;

#ifdef __NR_uname
    {
	struct new_utsname new_uts;
	__uname(&new_uts);
	//staptest// uname (XXXX) = 0

	__uname((struct new_utsname *)-1);
#ifdef __s390__
	//staptest// uname (0x[7]?[f]+) = -NNNN
#else
	//staptest// uname (0x[f]+) = -NNNN
#endif
    }
#endif

#ifdef __NR_olduname
    {
	struct old_utsname old_uts;
	__olduname(&old_uts);
	//staptest// [[[[uname (XXXX)!!!!ni_syscall ()]]]] = NNNN

	__olduname((struct old_utsname *)-1);
#ifdef __s390__
	//staptest// [[[[uname (0x[7]?[f]+)!!!!ni_syscall ()]]]] = -NNNN
#else
	//staptest// [[[[uname (0x[f]+)!!!!ni_syscall ()]]]] = -NNNN
#endif
    }
#endif

#ifdef __NR_oldolduname
    {
	struct oldold_utsname oldold_uts;
	__oldolduname(&oldold_uts);
	//staptest// [[[[uname (XXXX)!!!!ni_syscall ()]]]] = NNNN

	__oldolduname((struct oldold_utsname *)-1);
#ifdef __s390__
	//staptest// [[[[uname (0x[7]?[f]+)!!!!ni_syscall ()]]]] = -NNNN
#else
	//staptest// [[[[uname (0x[f]+)!!!!ni_syscall ()]]]] = -NNNN
#endif
    }
#endif

    return 0;
}
