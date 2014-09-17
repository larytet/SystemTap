/* COVERAGE: getgroups */
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>
#include <sys/param.h>

int main()
{
    gid_t gidset[NGROUPS];

    getgroups(NGROUPS, gidset);
    //staptest// getgroups (NNNN, XXXX) = NNNN

    getgroups(-1, gidset);
    //staptest// getgroups (-1, XXXX) = -NNNN

    getgroups(NGROUPS, (gid_t *)-1);
#ifdef __s390__
    //staptest// getgroups (NNNN, 0x[7]?[f]+) = -NNNN (EFAULT)
#else
    //staptest// getgroups (NNNN, 0x[f]+) = -NNNN (EFAULT)
#endif
}
