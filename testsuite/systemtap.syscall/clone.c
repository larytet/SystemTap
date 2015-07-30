/* COVERAGE: clone wait4 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/syscall.h>

static int child_fn(void *arg)
{
    exit(0);
}

#define STACK_SIZE (1024 * 1024)    /* Stack size for cloned child */

/* For limit testing, we need the raw syscalls. */
#if defined(__ia64__) && defined(SYS_clone2)
static inline int
__sys_clone2(int flags, void *ustack_base, size_t ustack_size,
	     void *parent_tidptr, void *child_tidptr, void *tls)
{
    return syscall(SYS_clone2, flags, ustack_base, ustack_size, parent_tidptr,
		   child_tidptr, tls);
}
#endif

#ifdef SYS_clone
// The main kernel config file says that clone comes from the "ABI
// hall of shame". Here's why. clone differs in the order of arguments
// between architectures. Here's the kernel code:
//
//    #ifdef CONFIG_CLONE_BACKWARDS
//    SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
//    		 int __user *, parent_tidptr,
//    		 int, tls_val,
//    		 int __user *, child_tidptr)
//    #elif defined(CONFIG_CLONE_BACKWARDS2)
//    SYSCALL_DEFINE5(clone, unsigned long, newsp, unsigned long, clone_flags,
//    		 int __user *, parent_tidptr,
//    		 int __user *, child_tidptr,
//    		 int, tls_val)
//    #elif defined(CONFIG_CLONE_BACKWARDS3)
//    SYSCALL_DEFINE6(clone, unsigned long, clone_flags, unsigned long, newsp,
//    		int, stack_size,
//    		int __user *, parent_tidptr,
//    		int __user *, child_tidptr,
//    		int, tls_val)
//    #else
//    SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
//    		 int __user *, parent_tidptr,
//    		 int __user *, child_tidptr,
//    		 int, tls_val)
//    #endif
//
// Note that systemtap actually probes do_fork(), which doesn't get
// the tls_val.

#if defined(__s390__) || defined(__s390x__)
#define CLONE_BACKWARDS2
#endif
#if defined(__powerpc__) || defined(__powerpc64__) \
    || defined(__arm__) || defined(__aarch64__) \
    || defined(__i386__)
#define CLONE_BACKWARDS
#endif

static inline long
__clone(unsigned long flags, void *child_stack, void *ptid, void *ctid,
	int tls_val)
{
#ifdef CLONE_BACKWARDS
    return syscall(SYS_clone, flags, child_stack, ptid, tls_val, ctid);
#elif defined(CLONE_BACKWARDS2)
    return syscall(SYS_clone, child_stack, flags, ptid, ctid, tls_val);
#elif defined(CLONE_BACKWARDS3)
    return syscall(SYS_clone, flags, child_stack, 0, ptid, ctid, tls_val);
#else
    return syscall(SYS_clone, flags, child_stack, ptid, ctid, tls_val);
#endif
}
#endif

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
    //staptest// clone2 (0x0|SIGCHLD, XXXX, 0x100000, XXXX, XXXX) = NNNN
#else
    pid = clone(child_fn, stackTop, SIGCHLD, NULL);
    //staptest// clone (0x0|SIGCHLD, XXXX, XXXX, XXXX) = NNNN
#endif
    wait4(pid, NULL, 0, NULL);		/* Wait for child */
    //staptest// wait4 (NNNN, 0x0, 0x0, 0x0) = NNNN

#if defined(__ia64__)
    pid = __clone2(child_fn, stack, STACK_SIZE, SIGCHLD);
    //staptest// clone2 (0x0|SIGCHLD, XXXX, 0x100000, XXXX, XXXX) = NNNN
#else
    pid = clone(child_fn, stackTop, SIGCHLD, NULL);
    //staptest// clone (0x0|SIGCHLD, XXXX, XXXX, XXXX) = NNNN
#endif
    wait4(pid, NULL, 0, NULL);		/* Wait for child */
    //staptest// wait4 (NNNN, 0x0, 0x0, 0x0) = NNNN

    /* Limit testing. BTW, we want all these calls to fail, since we
     * aren't cleaning up the child processes. */

#if defined(__ia64__) && defined(SYS_clone2)
    // Keep sending an invalid flag value so all calls will fail.
    pid = __sys_clone2(-1, NULL, 0, NULL, NULL, NULL);
    //staptest// clone2 (CLONE_[^ ]+|XXXX, 0x0, 0x0, 0x0, 0x0) = -NNNN

    pid = __sys_clone2(-1, (void *)-1, 0, NULL, NULL, NULL);
    //staptest// clone2 (CLONE_[^ ]+|XXXX, 0x[f]+, 0x0, 0x0, 0x0) = -NNNN

    pid = __sys_clone2(-1, NULL, -1, NULL, NULL, NULL);
    //staptest// clone2 (CLONE_[^ ]+|XXXX, 0x0, 0x[f]+, 0x0, 0x0) = -NNNN

    pid = __sys_clone2(-1, NULL, 0, (void *)-1, NULL, NULL);
    //staptest// clone2 (CLONE_[^ ]+|XXXX, 0x0, 0x0, 0x[f]+, 0x0) = -NNNN

    pid = __sys_clone2(-1, NULL, 0, NULL, (void *)-1, NULL);
    //staptest// clone2 (CLONE_[^ ]+|XXXX, 0x0, 0x0, 0x0, 0x[f]+) = -NNNN
#endif

#ifdef SYS_clone
    // We've got a bit of a problem here. On RHEL6, if you don't
    // pass a stack pointer, the kernel uses a register value as a
    // stack pointer. So, we'll pass an invalid value (1) for a stack
    // pointer. Sigh, except that rawhide
    // (4.0.0-0.rc3.git0.1.fc23.x86_64) needs a real stack pointer or
    // this test program will crash.

    pid = __clone((unsigned long)-1, stackTop, NULL, NULL, 0);
    // On a 32-bit kernel, the full clone flags will overflow
    // MAXSTRINGLEN.
    //staptest// clone (CLONE_[^ ]+[[[[ ?!!!!|XXXX, XXXX, 0x0, 0x0]]]]?) = -NNNN

    // Keep sending an invalid flag value so all calls will fail. But,
    // sending a -1 will cause all the flags to be set, overflowing
    // MAXSTRINGLEN. So, we'll use a different invalid flag value. You
    // can't specify CLONE_NEWNS (new mount namespace group) and CLONE_FS
    // (share filesystem info between processes) together.

    // Unfortunately, on 4.0.0-0.rc3.git0.1.fc23.x86_64 passing -1 for
    // a stack pointer causes the test executable to get a
    // segmentation fault. So, we'll skip this one.
    //
    //   pid = __clone(CLONE_FS|CLONE_NEWNS, (void *)-1, NULL, NULL, 0);

    pid = __clone(CLONE_FS|CLONE_NEWNS, stackTop, (void *)-1, NULL, 0);
#ifdef __s390__
    //staptest// clone (CLONE_FS|CLONE_NEWNS, XXXX, 0x[7]?[f]+, 0x0) = -NNNN
#else
    //staptest// clone (CLONE_FS|CLONE_NEWNS, XXXX, 0x[f]+, 0x0) = -NNNN
#endif

    pid = __clone(CLONE_FS|CLONE_NEWNS, stackTop, NULL, (void *)-1, 0);
#ifdef __s390__
    //staptest// clone (CLONE_FS|CLONE_NEWNS, XXXX, 0x0, 0x[7]?[f]+) = -NNNN
#else
    //staptest// clone (CLONE_FS|CLONE_NEWNS, XXXX, 0x0, 0x[f]+) = -NNNN
#endif
#endif

    return 0;
}
