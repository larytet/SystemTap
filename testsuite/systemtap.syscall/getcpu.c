/* COVERAGE: getcpu */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#ifdef __NR_getcpu

static inline int __getcpu(unsigned *cpu, unsigned *node, void *tcache)
{
    return syscall(__NR_getcpu, cpu, node, tcache);
}

int main()
{
    unsigned cpu, node;
    /* Since the getcpu_cache structure isn't exported to userspace,
     * just allocate a big buffer. */
    char buf[1024 * 1024];

    __getcpu(&cpu, &node, &buf);
    //staptest// getcpu (XXXX, XXXX, XXXX) = 0

    /* Limit testing. */
    __getcpu((unsigned *)-1, &node, &buf);
#ifdef __s390__
    //staptest// getcpu (0x[7]?[f]+, XXXX, XXXX) = NNNN
#else
    //staptest// getcpu (0x[f]+, XXXX, XXXX) = NNNN
#endif

    __getcpu(&cpu, (unsigned *)-1, &buf);
#ifdef __s390__
    //staptest// getcpu (XXXX, 0x[7]?[f]+, XXXX) = NNNN
#else
    //staptest// getcpu (XXXX, 0x[f]+, XXXX) = NNNN
#endif

    __getcpu(&cpu, &node, (void *)-1);
#ifdef __s390__
    //staptest// getcpu (XXXX, XXXX, 0x[7]?[f]+) = NNNN
#else
    //staptest// getcpu (XXXX, XXXX, 0x[f]+) = NNNN
#endif
    
    return 0;
}
#else
int main()
{
    return 0;
}
#endif
