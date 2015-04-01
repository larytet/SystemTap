/* COVERAGE: mincore */

#include <unistd.h>
#include <sys/mman.h>

static unsigned char *vec;

int main()
{
    mincore((void *)0, 0, vec);
    //staptest// mincore (0x0, 0, 0x0) = 0

    mincore((void *)-1, 0, vec);
#if __s390__
    //staptest// mincore (0x[7]?[f]+, 0, 0x0)
#else
    //staptest// mincore (0x[f]+, 0, 0x0)
#endif

    mincore(0, -1, vec);
#if __WORDSIZE==64
    //staptest// mincore (0x0, 18446744073709551615, 0x0)
#else
    //staptest// mincore (0x0, 4294967295, 0x0)
#endif

    mincore(0, 0, (unsigned char *)-1);
#ifdef __s390__
    //staptest// mincore (0x0, 0, 0x[7]?[f]+)
#else
    //staptest// mincore (0x0, 0, 0x[f]+)
#endif

    return 0;
}
