/* COVERAGE: get_robust_list set_robust_list */

#define _GNU_SOURCE
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline long
__get_robust_list(int pid, struct robust_list_head **lhp, size_t *len)
{
    return syscall(__NR_get_robust_list, pid, lhp, len);
}

static inline long
__set_robust_list(struct robust_list_head *lhp, size_t len)
{
    return syscall(__NR_set_robust_list, lhp, len);
}

int main()
{
    int pid;
    struct robust_list_head *lhp;
    size_t len;

    __get_robust_list(getpid(), &lhp, &len);
    //staptest// get_robust_list (NNNN, XXXX, XXXX) = 0

    __set_robust_list(lhp, len);
    //staptest// set_robust_list (XXXX, NNNN) = 0

    // Limit testing

    __get_robust_list(-1, &lhp, &len);
    //staptest// get_robust_list (-1, XXXX, XXXX) = -NNNN

    __get_robust_list(0, (struct robust_list_head **)-1, &len);
#ifdef __s390__
    //staptest// get_robust_list (0, 0x[7]?[f]+, XXXX) = -NNNN
#else
    //staptest// get_robust_list (0, 0x[f]+, XXXX) = -NNNN
#endif

    __get_robust_list(0, &lhp, (size_t *)-1);
#ifdef __s390__
    //staptest// get_robust_list (0, XXXX, 0x[7]?[f]+) = -NNNN
#else
    //staptest// get_robust_list (0, XXXX, 0x[f]+) = -NNNN
#endif

    __set_robust_list((struct robust_list_head *)-1, 0);
#ifdef __s390__
    //staptest// set_robust_list (0x[7]?[f]+, 0) = -NNNN
#else
    //staptest// set_robust_list (0x[f]+, 0) = -NNNN
#endif

    __set_robust_list(0, -1);
#if __WORDSIZE == 64
    //staptest// set_robust_list (0x0, 18446744073709551615) = -NNNN
#else
    //staptest// set_robust_list (0x0, 4294967295) = -NNNN
#endif

    return 0;
}
