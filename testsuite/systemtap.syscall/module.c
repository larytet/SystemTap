/* COVERAGE: init_module finit_module delete_module */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>

#ifndef MODULE_INIT_IGNORE_MODVERSIONS
#define MODULE_INIT_IGNORE_MODVERSIONS	1
#endif
#ifndef MODULE_INIT_IGNORE_VERMAGIC
#define MODULE_INIT_IGNORE_VERMAGIC	2
#endif

// There aren't any glibc wrappers for these calls, so make our own.

static inline int
__init_module(void *module_image, unsigned long len, const char *param_values)
{
    return syscall(SYS_init_module, module_image, len, param_values);
}

#ifdef SYS_finit_module
static inline int __finit_module(int fd, const char *param_values, int flags)
{
    return syscall(SYS_finit_module, fd, param_values, flags);
}
#endif

static inline int __delete_module(const char *name, int flags)
{
    return syscall(SYS_delete_module, name, flags);
}

int main()
{
    int fd_null;

    /* Normally we try to have a sucessful system call in the syscall
     * tests. For these module calls, we're not going to bother. Why?
     *
     * 1) Laziness. We don't want to bother compiling a test module.
     *
     * 2) Permissions. If we aren't running as root, these calls will
     * fail anyway.
     *
     * So, we expect all these calls to fail.
     */

    fd_null = open("/dev/null", O_RDONLY);

    __init_module(NULL, 0, "foo=bar");
    //staptest// init_module (0x0, 0, "foo=bar") = -NNNN

#ifdef SYS_finit_module
    __finit_module(fd_null, "foo=bar", MODULE_INIT_IGNORE_MODVERSIONS);
    //staptest// finit_module (NNNN, "foo=bar", MODULE_INIT_IGNORE_MODVERSIONS) = -NNNN
#endif

    // Here we have to be careful to not remove a real module, but all
    // we can really do is pick a bizarre module name that shouldn't exist.
    __delete_module("__fAkE__sTaP__mOdUlE__", O_NONBLOCK);
    //staptest// delete_module ("__fAkE__sTaP__mOdUlE__", O_NONBLOCK) = -NNNN

    /* Limit testing. */

    __init_module((void *)-1, 0, "foo=bar");
#ifdef __s390__
    //staptest// init_module (0x[7]?[f]+, 0, "foo=bar") = -NNNN
#else
    //staptest// init_module (0x[f]+, 0, "foo=bar") = -NNNN
#endif

    // On RHEL 7 ppc64, this one can cause an OOM error.
#if 0
    __init_module(NULL, -1, "foo=bar");
#if __WORDSIZE == 64
    //staptest// init_module (0x0, 18446744073709551615, "foo=bar") = -NNNN
#else
    //staptest// init_module (0x0, 4294967295, "foo=bar") = -NNNN
#endif
#endif

    __init_module(NULL, 0, (char *)-1);
#ifdef __s390__
    //staptest// init_module (0x0, 0, 0x[7]?[f]+) = -NNNN
#else
    //staptest// init_module (0x0, 0, 0x[f]+) = -NNNN
#endif

#ifdef SYS_finit_module
    __finit_module(-1, "foo=bar", MODULE_INIT_IGNORE_MODVERSIONS);
    //staptest// finit_module (-1, "foo=bar", MODULE_INIT_IGNORE_MODVERSIONS) = -NNNN
    __finit_module(fd_null, (char *)-1, MODULE_INIT_IGNORE_MODVERSIONS);
#ifdef __s390__
    //staptest// finit_module (NNNN, 0x[7]?[f]+, MODULE_INIT_IGNORE_MODVERSIONS) = -NNNN
#else
    //staptest// finit_module (NNNN, 0x[f]+, MODULE_INIT_IGNORE_MODVERSIONS) = -NNNN
#endif
    __finit_module(fd_null, "foo=bar", -1);
    //staptest// finit_module (NNNN, "foo=bar", MODULE_INIT_[^ ]+|XXXX) = -NNNN
#endif

    __delete_module((char *)-1, O_TRUNC);
#ifdef __s390__
    //staptest// delete_module (0x[7]?[f]+, O_TRUNC) = -NNNN
#else
    //staptest// delete_module (0x[f]+, O_TRUNC) = -NNNN
#endif

    __delete_module("__fAkE__sTaP__mOdUlE__", -1);
    //staptest// delete_module ("__fAkE__sTaP__mOdUlE__", O_[^ ]+|XXXX) = -NNNN

    close(fd_null);
    return 0;
}
