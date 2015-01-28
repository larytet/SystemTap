/* COVERAGE: nfsservctl */

#include <string.h>
#include <netinet/in.h>
#include <linux/nfs.h>
#include <linux/nfsd/nfsfh.h>
#include <linux/nfsd/syscall.h>
#include <sys/syscall.h>

// Note that since linux 3.1, the nfsservctl() syscall no longer exists.

int main()
{
#ifdef SYS_nfsservctl
    struct nfsctl_arg arg;
    union nfsctl_res res;

    /* Since we don't really want the export to succeed, try
     * exporting a path that shouldn't exist. Even if it happened to
     * exist, more fields need to be filled out in the u_export
     * structure for the call to succeed. */
    strncpy(arg.u.u_export.ex_path, "/__fAkE_pAtH__/__nO_eXiSt__",
	    sizeof(arg.u.u_export.ex_path));
    nfsservctl(NFSCTL_EXPORT, &arg, &res);
    //staptest// [[[[nfsservctl (NFSCTL_EXPORT, XXXX, XXXX)!!!!ni_syscall ()]]]] = -NNNN

    /* Limit testing. */
    nfsservctl(-1, &arg, &res);
    //staptest// [[[[nfsservctl (0xffffffff, XXXX, XXXX)!!!!ni_syscall ()]]]] = -NNNN

    nfsservctl(NFSCTL_EXPORT, (struct nfsctl_arg *)-1, &res);
    //staptest// [[[[nfsservctl (NFSCTL_EXPORT, 0x[f]+, XXXX)!!!!ni_syscall ()]]]] = -NNNN

    nfsservctl(NFSCTL_EXPORT, &arg, (union nfsctl_res *)-1);
    //staptest// [[[[nfsservctl (NFSCTL_EXPORT, XXXX, 0x[f]+)!!!!ni_syscall ()]]]] = -NNNN
#endif
    return 0;
}
