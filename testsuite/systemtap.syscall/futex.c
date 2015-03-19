/* COVERAGE: futex */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

static int *futex_addr;

static inline int
__futex(int *uaddr, int op, int val, const struct timespec *timeout,
	int *uaddr2, int val3)
{
    return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);
}

void do_child()
{
    *futex_addr = 0;
    __futex(futex_addr, FUTEX_WAIT, 0, NULL, NULL, 0);
}

int main()
{
    pid_t pid;
    int status;
    struct timespec t = {0,789};

    // Create a mmapped area that both the parent and child processes
    // can access.
    futex_addr = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE,
		      MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    pid = fork();
    if (pid == 0) {		/* child */
	do_child();
	exit(0);
    }

    // Wait for the child to run and wait on the futex.
    sleep(1);

    // Wake the child (which should immediately exit).
    __futex(futex_addr, FUTEX_WAKE, 1, NULL, NULL, 0);
    //staptest// futex (XXXX, FUTEX_WAKE, 1) = 1

    // Clean up the child process and the mmapped area.
    waitpid(pid, &status, 0);
    munmap(futex_addr, sizeof(int));

    /* Limit testing. */

    __futex((int *)-1, FUTEX_WAKE, 1, NULL, NULL, 0);
#ifdef __s390__
    //staptest// futex (0x[7]?[f]+, FUTEX_WAKE, 1) = NNNN
#else
    //staptest// futex (0x[f]+, FUTEX_WAKE, 1) = NNNN
#endif
    __futex(NULL, -1, 1, NULL, NULL, 0);
#ifdef __s390__
    //staptest// futex (0x0, 0x[7]?[f]+, 1) = NNNN
#else
    //staptest// futex (0x0, 0x[f]+, 1) = NNNN
#endif

    __futex(NULL, FUTEX_CMP_REQUEUE, -1, NULL, NULL, 0);
    //staptest// futex (0x0, FUTEX_CMP_REQUEUE, -1, 0, 0x0, 0) = NNNN

    __futex(NULL, FUTEX_CMP_REQUEUE, 1, (struct timespec *)-1, NULL, 0);
#if __WORDSIZE == 64
    //staptest// futex (0x0, FUTEX_CMP_REQUEUE, 1, 18446744073709551615, 0x0, 0) = NNNN
#else
#ifdef __s390__
    //staptest// futex (0x0, FUTEX_CMP_REQUEUE, 1, [[[[2147483647!!!!4294967295]]]], 0x0, 0) = NNNN
#else
    //staptest// futex (0x0, FUTEX_CMP_REQUEUE, 1, 4294967295, 0x0, 0) = NNNN
#endif
#endif

    __futex(NULL, FUTEX_CMP_REQUEUE, 1, NULL, (int *)-1, 0);
#ifdef __s390__
    //staptest// futex (0x0, FUTEX_CMP_REQUEUE, 1, 0, 0x[7]?[f]+, 0) = NNNN
#else
    //staptest// futex (0x0, FUTEX_CMP_REQUEUE, 1, 0, 0x[f]+, 0) = NNNN
#endif

    __futex(NULL, FUTEX_CMP_REQUEUE, 1, NULL, NULL, -1);
    //staptest// futex (0x0, FUTEX_CMP_REQUEUE, 1, 0, 0x0, -1) = NNNN

    /* Since the futex argstr is different based on the operation,
     * test several variants. */

#ifdef FUTEX_WAKE_BITSET
    __futex(NULL, FUTEX_WAKE_BITSET, 1, NULL, NULL, -1);
#ifdef __s390__
    //staptest// futex (0x0, FUTEX_WAKE_BITSET, 1, 0x[7]?[f]+) = NNNN
#else
    //staptest// futex (0x0, FUTEX_WAKE_BITSET, 1, 0x[f]+) = NNNN
#endif
#endif

    __futex(NULL, FUTEX_WAIT, 1, &t, NULL, 0);
    //staptest// futex (0x0, FUTEX_WAIT, 1, \[0.000000789\]) = NNNN

#ifdef FUTEX_WAIT_BITSET
    __futex(NULL, FUTEX_WAIT_BITSET, 1, &t, NULL, 0xbeef);
    //staptest// futex (0x0, FUTEX_WAIT_BITSET, 1, \[0.000000789\], 0xbeef) = NNNN
#endif

    __futex(NULL, FUTEX_REQUEUE, 1, (struct timespec *)2, NULL, -1);
    //staptest// futex (0x0, FUTEX_REQUEUE, 1, 2, 0x0) = NNNN

    __futex(NULL, FUTEX_CMP_REQUEUE, 1, (struct timespec *)2, (int *)3, -1);
    //staptest// futex (0x0, FUTEX_CMP_REQUEUE, 1, 2, 0x3, -1) = NNNN

    __futex(NULL, FUTEX_WAKE_OP, 1, (struct timespec *)2, (int *)3,
	    FUTEX_OP(FUTEX_OP_SET, 2, FUTEX_OP_CMP_EQ, 1));
    //staptest// futex (0x0, FUTEX_WAKE_OP, 1, 2, 0x3, {FUTEX_OP_SET, 2, FUTEX_OP_CMP_EQ, 1}) = NNNN

#ifdef FUTEX_WAIT_REQUEUE_PI
    __futex(NULL, FUTEX_WAIT_REQUEUE_PI, 1, &t, NULL, 0);
    //staptest// futex (0x0, FUTEX_WAIT_REQUEUE_PI, 1, \[0.000000789\], 0x0) = NNNN
#endif

    return 0;
}
