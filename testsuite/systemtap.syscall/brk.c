/* COVERAGE: brk */

#include <unistd.h>

int main()
{
    void *pbrk;
    pbrk = sbrk(0);

    brk(pbrk);
    //staptest// brk (XXXX) = NNNN

    brk((void *)-1);
#ifdef __s390__
    //staptest// brk (0x[7]?[f]*)
#else
    //staptest// brk (0x[f]*)
#endif

    return 0;
}
