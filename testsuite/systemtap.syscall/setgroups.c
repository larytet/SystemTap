/* COVERAGE: setgroups setgroups16 */
#define _GNU_SOURCE
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>
#include <sys/param.h>
#include <sys/syscall.h>

int main()
{
    gid_t gidset[NGROUPS];
    size_t ngroups;

    ngroups = getgroups(NGROUPS, gidset);
    //staptest// getgroups (NNNN, XXXX) = NNNN

    setgroups(ngroups, gidset);
    //staptest// setgroups (NNNN, XXXX) = NNNN

    setgroups(-1, gidset);
    //staptest// setgroups (-1, XXXX) = -NNNN

    setgroups(ngroups, (gid_t *)-1);
#ifdef __s390__
    //staptest// setgroups (NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// setgroups (NNNN, 0x[f]+) = NNNN
#endif

#if (__WORDSIZE != 64) && defined(SYS_setgroups)
    syscall(SYS_setgroups, ngroups, gidset);
    //staptest// setgroups (NNNN, XXXX) = NNNN

    syscall(SYS_setgroups, -1, gidset);
    //staptest// setgroups (-1, XXXX) = -NNNN

    syscall(SYS_setgroups, ngroups, (gid_t *)-1);
#ifdef __s390__
    //staptest// setgroups (NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// setgroups (NNNN, 0x[f]+) = NNNN
#endif
#endif
}
