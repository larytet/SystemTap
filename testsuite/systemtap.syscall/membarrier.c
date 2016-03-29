/* COVERAGE: membarrier */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

// If SYS_membarrier is defined, let's assume that linux/membarrier.h
// exists.
#ifdef SYS_membarrier
#include <linux/membarrier.h>

static inline int membarrier(int cmd, int flags)
{
    return syscall(SYS_membarrier, cmd, flags);
}
#endif

int main()
{
#ifdef SYS_membarrier
    membarrier(MEMBARRIER_CMD_QUERY, 0);
    //staptest// membarrier (MEMBARRIER_CMD_QUERY, 0) = NNNN

    // Note that this functional test doesn't really do anything.
    membarrier(MEMBARRIER_CMD_SHARED, 0);
    //staptest// membarrier (MEMBARRIER_CMD_SHARED, 0) = 0

    membarrier(-1, 0);
    //staptest// membarrier (0xffffffff, 0) = NNNN

    membarrier(MEMBARRIER_CMD_QUERY, -1);
    //staptest// membarrier (MEMBARRIER_CMD_QUERY, -1) = NNNN
#endif
    return 0;
}
