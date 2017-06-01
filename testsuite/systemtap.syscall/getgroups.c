/* COVERAGE: getgroups getgroups16 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>
#include <sys/param.h>
#include <sys/syscall.h>

int main()
{
    gid_t gidset[NGROUPS];

    getgroups(NGROUPS, gidset);
    //staptest// getgroups (NNNN, XXXX) = NNNN

    getgroups(-1, gidset);
    //staptest// getgroups (-1, XXXX) = -NNNN

    getgroups(NGROUPS, (gid_t *)-1);
#ifdef __s390__
    //staptest// getgroups (NNNN, 0x[7]?[f]+) = NNNN
#else
    //staptest// getgroups (NNNN, 0x[f]+) = NNNN
#endif

#if (__WORDSIZE != 64) && defined(SYS_getgroups)
  syscall(SYS_getgroups, NGROUPS, gidset);
  //staptest// getgroups (NNNN, XXXX) = NNNN

  syscall(SYS_getgroups, -1, gidset);
  //staptest// getgroups (-1, XXXX) = NNNN

  syscall(SYS_getgroups, NGROUPS, (gid_t *)-1);
#ifdef __s390__
  //staptest// getgroups (NNNN, 0x[7]?[f]+) = NNNN
#else
  //staptest// getgroups (NNNN, 0x[f]+) = NNNN
#endif
#endif
  return 0;
}
