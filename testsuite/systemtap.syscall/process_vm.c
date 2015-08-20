/* COVERAGE: process_vm_readv process_vm_writev */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/wait.h>

char child_buffer[1024];

void do_child()
{
    strcpy(child_buffer, "Hello there, we're in the child.");
    while (1) {
	sleep(1);
    }
}

int main()
{
// glibc added support for process_vm_* in glibc 2.15.
#if __GLIBC_PREREQ(2, 15)
    pid_t pid = 0;
    int status;
    char buffer[1024];
    struct iovec local[1];
    struct iovec remote[1];

    pid = fork();
    if (pid == 0) {			/* child */
	do_child();
	return 0;
    }

    /* Give the child a chance to run. */
    sleep(1);

    /* Set up the iovecs. */
    local[0].iov_base = buffer;
    local[0].iov_len = 20;
    remote[0].iov_base = child_buffer;
    remote[0].iov_len = 20;

    /* Read 20 bytes from the child process. */
    process_vm_readv(pid, local, 1, remote, 1, 0);
    //staptest// process_vm_readv (NNNN, XXXX, 1, XXXX, 1, 0) = 20

    /* Change the buffer and set up the iovecs again. */
    strcpy(buffer, "From parent");
    local[0].iov_len = 12;
    remote[0].iov_len = 12;

    /* Write 12 bytes into the child process. */
    process_vm_writev(pid, local, 1, remote, 1, 0);
    //staptest// process_vm_writev (NNNN, XXXX, 1, XXXX, 1, 0) = 12
    
    /* Limit testing. */
    process_vm_readv(-1, local, 1, remote, 1, 0);
    //staptest// process_vm_readv (-1, XXXX, 1, XXXX, 1, 0) = -NNNN

    process_vm_readv(pid, (struct iovec *)-1, 1, remote, 1, 0);
#ifdef __s390__
    //staptest// process_vm_readv (NNNN, 0x[7]?[f]+, 1, XXXX, 1, 0) = -NNNN
#else
    //staptest// process_vm_readv (NNNN, 0x[f]+, 1, XXXX, 1, 0) = -NNNN
#endif

    process_vm_readv(pid, local, -1L, remote, 1, 0);
#if __WORDSIZE == 64
    //staptest// process_vm_readv (NNNN, XXXX, 18446744073709551615, XXXX, 1, 0) = -NNNN
#else
    //staptest// process_vm_readv (NNNN, XXXX, 4294967295, XXXX, 1, 0) = -NNNN
#endif

    process_vm_readv(pid, local, 1, (struct iovec *)-1, 1, 0);
#ifdef __s390__
    //staptest// process_vm_readv (NNNN, XXXX, 1, 0x[7]?[f]+, 1, 0) = -NNNN
#else
    //staptest// process_vm_readv (NNNN, XXXX, 1, 0x[f]+, 1, 0) = -NNNN
#endif

    process_vm_readv(pid, local, 1, remote, -1L, 0);
#if __WORDSIZE == 64
    //staptest// process_vm_readv (NNNN, XXXX, 1, XXXX, 18446744073709551615, 0) = -NNNN
#else
    //staptest// process_vm_readv (NNNN, XXXX, 1, XXXX, 4294967295, 0) = -NNNN
#endif

    process_vm_readv(pid, local, 1, remote, 1, -1L);
#if __WORDSIZE == 64
    //staptest// process_vm_readv (NNNN, XXXX, 1, XXXX, 1, 18446744073709551615) = -NNNN
#else
    //staptest// process_vm_readv (NNNN, XXXX, 1, XXXX, 1, 4294967295) = -NNNN
#endif

    process_vm_writev(-1, local, 1, remote, 1, 0);
    //staptest// process_vm_writev (-1, XXXX, 1, XXXX, 1, 0) = -NNNN

    process_vm_writev(pid, (struct iovec *)-1, 1, remote, 1, 0);
#ifdef __s390__
    //staptest// process_vm_writev (NNNN, 0x[7]?[f]+, 1, XXXX, 1, 0) = -NNNN
#else
    //staptest// process_vm_writev (NNNN, 0x[f]+, 1, XXXX, 1, 0) = -NNNN
#endif

    process_vm_writev(pid, local, -1L, remote, 1, 0);
#if __WORDSIZE == 64
    //staptest// process_vm_writev (NNNN, XXXX, 18446744073709551615, XXXX, 1, 0) = -NNNN
#else
    //staptest// process_vm_writev (NNNN, XXXX, 4294967295, XXXX, 1, 0) = -NNNN
#endif

    process_vm_writev(pid, local, 1, (struct iovec *)-1, 1, 0);
#ifdef __s390__
    //staptest// process_vm_writev (NNNN, XXXX, 1, 0x[7]?[f]+, 1, 0) = -NNNN
#else
    //staptest// process_vm_writev (NNNN, XXXX, 1, 0x[f]+, 1, 0) = -NNNN
#endif

    process_vm_writev(pid, local, 1, remote, -1L, 0);
#if __WORDSIZE == 64
    //staptest// process_vm_writev (NNNN, XXXX, 1, XXXX, 18446744073709551615, 0) = -NNNN
#else
    //staptest// process_vm_writev (NNNN, XXXX, 1, XXXX, 4294967295, 0) = -NNNN
#endif

    process_vm_writev(pid, local, 1, remote, 1, -1L);
#if __WORDSIZE == 64
    //staptest// process_vm_writev (NNNN, XXXX, 1, XXXX, 1, 18446744073709551615) = -NNNN
#else
    //staptest// process_vm_writev (NNNN, XXXX, 1, XXXX, 1, 4294967295) = -NNNN
#endif

    if (pid > 0) {
	(void)kill(pid, SIGKILL);	/* kill the child */

	/* Reap the child. */
	waitpid(pid, &status, 0);
    }

#endif
    return 0;
}
