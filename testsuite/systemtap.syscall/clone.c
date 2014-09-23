/* COVERAGE: clone wait4 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/wait.h>

static int child_fn(void *arg)
{
    exit(0);
}

#define STACK_SIZE (1024 * 1024)    /* Stack size for cloned child */

int
main()
{
    char *stack;
    char *stackTop;
    pid_t pid;

    stack = malloc(STACK_SIZE);
    stackTop = stack + STACK_SIZE;	/* Assume stack grows downward */

#if defined(__ia64__)
    pid = __clone2(child_fn, stack, STACK_SIZE, SIGCHLD);
    //staptest// clone2 (0x0|SIGCHLD, XXXX, 0x100000, XXXX, XXXX)
#else
    pid = clone(child_fn, stackTop, SIGCHLD, NULL);
    //staptest// clone (0x0|SIGCHLD, XXXX, XXXX, XXXX)
#endif
    wait4(pid, NULL, 0, NULL);		/* Wait for child */
    //staptest// wait4 (NNNN, 0x0, 0, 0x0) = NNNN

#define FLAG_ALL (CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|SIGCHLD)
#if defined(__ia64__)
    pid = __clone2(child_fn, stack, STACK_SIZE, FLAG_ALL);
    //staptest// clone2 (CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|SIGCHLD, XXXX, 0x100000, XXXX, XXXX)
#else
    pid = clone(child_fn, stackTop, FLAG_ALL, NULL);
    //staptest// clone (CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|SIGCHLD, XXXX, XXXX, XXXX)
#endif
    wait4(pid, NULL, 0, NULL);		/* Wait for child */
    //staptest// wait4 (NNNN, 0x0, 0, 0x0) = NNNN

    return 0;
}
