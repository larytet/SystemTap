/* COVERAGE: set_mempolicy get_mempolicy */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/mempolicy.h>

#if defined(__NR_get_mempolicy) && defined(__NR_set_mempolicy)
// Since we don't want to require the libnuma development and library
// packages, we'll define our own routines.

static inline int
get_mempolicy(int *mode, unsigned long *nodemask, unsigned long maxnode,
	      unsigned long addr, unsigned long flags)
{
    return syscall(__NR_get_mempolicy, mode, nodemask, maxnode, addr, flags);
}

static inline int
set_mempolicy(int mode, unsigned long *nodemask, unsigned long maxnode)
{
    return syscall(__NR_set_mempolicy, mode, nodemask, maxnode);
}

int main(int argc, char **argv)
{
    int mode;

    // ENOSYS expected on i686 kernels, so expect it.

    // Get the default policy. Notice we're not specifying a node
    // mask. Figuring out the proper size without the numa library is
    // tricky, so don't bother. 
    get_mempolicy(&mode, NULL, 0, 0, 0);
    //staptest// [[[[get_mempolicy (XXXX, 0x0, 0, 0x0, 0x0) = 0!!!!ni_syscall () = -38 (ENOSYS)]]]]

    set_mempolicy(MPOL_DEFAULT, NULL, 0);
    //staptest// [[[[set_mempolicy (MPOL_DEFAULT, 0x0, 0)!!!!ni_syscall ()]]]] = NNNN

    /* Limit testing. */

    get_mempolicy((int *)-1, NULL, 0, 0, MPOL_F_NODE);
    //staptest// [[[[get_mempolicy (0x[f]+, 0x0, 0, 0x0, MPOL_F_NODE)!!!!ni_syscall ()]]]] = NNNN

    get_mempolicy(NULL, (unsigned long *)-1, 0, 0, MPOL_F_NODE | MPOL_F_ADDR);
    //staptest// [[[[get_mempolicy (0x0, 0x[f]+, 0, 0x0, MPOL_F_NODE|MPOL_F_ADDR)!!!!ni_syscall ()]]]] = NNNN

    get_mempolicy(NULL, NULL, -1, 0, MPOL_F_ADDR);
#if __WORDSIZE == 64
    //staptest// [[[[get_mempolicy (0x0, 0x0, 18446744073709551615, 0x0, MPOL_F_ADDR)!!!!ni_syscall ()]]]] = NNNN
#else
    //staptest// [[[[get_mempolicy (0x0, 0x0, 4294967295, 0x0, MPOL_F_ADDR)!!!!ni_syscall ()]]]] = NNNN
#endif

    get_mempolicy(NULL, NULL, 0, 0, -1);
    //staptest// [[[[get_mempolicy (0x0, 0x0, 0, 0x0, MPOL[^ ]+|XXXX)!!!!ni_syscall ()]]]] = NNNN

    set_mempolicy(-1, NULL, 0);
    //staptest// [[[[set_mempolicy (0x[f]+, 0x0, 0)!!!!ni_syscall ()]]]] = NNNN

#ifdef MPOL_F_STATIC_NODES
    set_mempolicy(MPOL_F_STATIC_NODES | MPOL_DEFAULT, NULL, 0);
    //staptest// [[[[set_mempolicy (MPOL_F_STATIC_NODES|MPOL_DEFAULT, 0x0, 0)!!!!ni_syscall ()]]]] = NNNN
#endif

    set_mempolicy(MPOL_PREFERRED, (unsigned long *)-1, 0);
    //staptest// [[[[set_mempolicy (MPOL_PREFERRED, 0x[f]+, 0)!!!!ni_syscall ()]]]] = NNNN

    set_mempolicy(MPOL_BIND, NULL, -1);
#if __WORDSIZE == 64
    //staptest// [[[[set_mempolicy (MPOL_BIND, 0x0, 18446744073709551615)!!!!ni_syscall ()]]]] = NNNN
#else
    //staptest// [[[[set_mempolicy (MPOL_BIND, 0x0, 4294967295)!!!!ni_syscall ()]]]] = NNNN
#endif

    return 0;
}
#else
int main()
{
    return 0;
}
#endif
