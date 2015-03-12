/* COVERAGE: mbind migrate_pages move_pages */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <linux/mempolicy.h>
#include <sys/types.h>
#define TEST_PAGES 2


#ifdef __NR_mbind
int __mbind(void *start, unsigned long len, int mode,
            unsigned long *nmask, unsigned long maxnode, unsigned flags) {
    return syscall(__NR_mbind, start, len, mode, nmask, maxnode, flags);
}
#endif

#ifdef __NR_migrate_pages
long __migrate_pages(pid_t pid, unsigned long maxnode,
                     const unsigned long *old_nodes,
                     const unsigned long *new_nodes) {
    return syscall(__NR_migrate_pages, pid, maxnode, old_nodes, new_nodes);
}
#endif

#ifdef __NR_move_pages
long __move_pages(pid_t pid, unsigned long nr_pages, const void **pages,
                  const int *nodes, int *status, int flags) {
    return syscall(__NR_move_pages, pid, nr_pages, pages, nodes, status, flags);
}
#endif


int main()
{
    unsigned long *p;
    unsigned long len = 100;
    const unsigned long node0 = 1;
    const void *pages[TEST_PAGES] = { 0 };
    int status[TEST_PAGES];

// ---- normal operation --------------

#ifdef __NR_mbind
    p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    __mbind(p, len, MPOL_DEFAULT, NULL, 0, 0);
    //staptest// [[[[mbind (XXXX, 100, MPOL_DEFAULT, 0x0, 0, 0x0)!!!!ni_syscall ()]]]] = NNNN
#endif

#ifdef __NR_move_pages
    __move_pages(0, TEST_PAGES, pages, NULL, status, 0);
    //staptest// [[[[move_pages (0, 2, XXXX, 0x0, XXXX, 0x0) = 0!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

#ifdef __NR_migrate_pages
    __migrate_pages(0, 1, &node0, &node0);
    //staptest// [[[[migrate_pages (0, 1, XXXX, XXXX) = 0!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif


// ---- ugly calls --------------------

#ifdef __NR_mbind
    __mbind((void *)-1, 0, 0, NULL, 0, 0);
    //staptest// [[[[mbind (0x[f]+, 0, MPOL_DEFAULT, 0x0, 0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __mbind(NULL, -1, 0, NULL, 0, 0);
#if __WORDSIZE == 64
    //staptest// [[[[mbind (0x0, 18446744073709551615, MPOL_DEFAULT, 0x0, 0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#else
    //staptest// [[[[mbind (0x0, 4294967295, MPOL_DEFAULT, 0x0, 0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

    __mbind(NULL, 0, -1, NULL, 0, 0);
    //staptest// [[[[mbind (0x0, 0, 0x[f]+, 0x0, 0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __mbind(NULL, 0, 0, (unsigned long *)-1, 0, 0);
    //staptest// [[[[mbind (0x0, 0, MPOL_DEFAULT, 0x[f]+, 0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __mbind(NULL, 0, 0, NULL, -1, 0);
#if __WORDSIZE == 64
    //staptest// [[[[mbind (0x0, 0, MPOL_DEFAULT, 0x0, 18446744073709551615, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#else
    //staptest// [[[[mbind (0x0, 0, MPOL_DEFAULT, 0x0, 4294967295, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

    __mbind(NULL, 0, 0, NULL, 0, -1);
    //staptest// [[[[mbind (0x0, 0, MPOL_DEFAULT, 0x0, 0, MPOL_F_NODE|MPOL_F_ADDR[[[[!!!!|MPOL_F_MEMS_ALLOWED]]]]|XXXX)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

#ifdef MPOL_F_STATIC_NODES
    __mbind(NULL, 0, MPOL_F_STATIC_NODES|MPOL_PREFERRED, NULL, 0, 0);
    //staptest// [[[[mbind (0x0, 0, MPOL_F_STATIC_NODES|MPOL_PREFERRED, 0x0, 0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

    __mbind(NULL, 0, 0, NULL, 0, MPOL_F_ADDR);
    //staptest// [[[[mbind (0x0, 0, MPOL_DEFAULT, 0x0, 0, MPOL_F_ADDR)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

#ifdef __NR_move_pages
    __move_pages(-1, 0, NULL, NULL, NULL, 0);
    //staptest// [[[[move_pages (-1, 0, 0x0, 0x0, 0x0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __move_pages(0, -1, NULL, NULL, NULL, 0);
#if __WORDSIZE == 64
    //staptest// [[[[move_pages (0, 18446744073709551615, 0x0, 0x0, 0x0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#else
    //staptest// [[[[move_pages (0, 4294967295, 0x0, 0x0, 0x0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

    __move_pages(0, 0, (const void **)-1, NULL, NULL, 0);
    //staptest// [[[[move_pages (0, 0, 0x[f]+, 0x0, 0x0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __move_pages(0, 0, NULL, (const int *)-1, NULL, 0);
    //staptest// [[[[move_pages (0, 0, 0x0, 0x[f]+, 0x0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __move_pages(0, 0, NULL, NULL, (int *)-1, 0);
    //staptest// [[[[move_pages (0, 0, 0x0, 0x0, 0x[f]+, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __move_pages(0, 0, NULL, NULL, NULL, -1);
    //staptest// [[[[move_pages (0, 0, 0x0, 0x0, 0x0, MPOL_F_NODE|MPOL_F_ADDR[[[[!!!!|MPOL_F_MEMS_ALLOWED]]]]|XXXX)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __move_pages(0, 0, NULL, NULL, NULL, MPOL_F_ADDR);
    //staptest// [[[[move_pages (0, 0, 0x0, 0x0, 0x0, MPOL_F_ADDR)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

#ifdef __NR_migrate_pages
    __migrate_pages(-1, 0, NULL, NULL);
    //staptest// [[[[migrate_pages (-1, 0, 0x0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __migrate_pages(0, -1, NULL, NULL);
#if __WORDSIZE == 64
    //staptest// [[[[migrate_pages (0, 18446744073709551615, 0x0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#else
    //staptest// [[[[migrate_pages (0, 4294967295, 0x0, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

    __migrate_pages(0, 0, (const unsigned long *)-1, NULL);
    //staptest// [[[[migrate_pages (0, 0, 0x[f]+, 0x0)!!!!ni_syscall () = NNNN (ENOSYS)]]]]

    __migrate_pages(0, 0, NULL, (const unsigned long *)-1);
    //staptest// [[[[migrate_pages (0, 0, 0x0, 0x[f]+)!!!!ni_syscall () = NNNN (ENOSYS)]]]]
#endif

    return 0;

}
