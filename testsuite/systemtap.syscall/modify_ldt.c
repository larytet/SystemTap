/* COVERAGE: modify_ldt */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#if defined(__NR_modify_ldt)
#include <asm/ldt.h>

static inline int
__modify_ldt(int func, void *ptr, unsigned long bytecount)
{
    return syscall(__NR_modify_ldt, func, ptr, bytecount);
}
#endif

int main()
{
#if defined(__NR_modify_ldt)
    struct user_desc desc;

    __modify_ldt(0, &desc, sizeof(desc));
    //staptest// modify_ldt (0, XXXX, NNNN) = 0

    /* Limit testing. */

    __modify_ldt(-1, &desc, sizeof(desc));
    //staptest// modify_ldt (-1, XXXX, NNNN) = NNNN

    __modify_ldt(0, (void *)-1, 0);
    //staptest// modify_ldt (0, 0x[f]+, 0) = NNNN

    __modify_ldt(0, NULL, (unsigned long)-1L);
#if __WORDSIZE == 64
    //staptest// modify_ldt (0, 0x0, 18446744073709551615) = NNNN
#else
    //staptest// modify_ldt (0, 0x0, 4294967295) = NNNN
#endif
#endif
}
