/* COVERAGE: lookup_dcookie */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#define LENGTH 255

// lookup_dcookie() is a special-purpose system call, currently 
// used only by the oprofile profiler. Used by v0.4 <= opcontrol <= v0.9.
// http://oprofile.sourceforge.net/doc/internals/sample-file-generation.html

// from oprofile 0.9 source (daemon/opd_cookie.h):
typedef unsigned long long cookie_t;

// from oprofile 0.9 source (daemon/opd_cookie.c):
#if (defined(__powerpc__) && !defined(__powerpc64__)) || defined(__hppa__)\
        || (defined(__s390__) && !defined(__s390x__)) \
        || (defined(__mips__) && (_MIPS_SIM == _MIPS_SIM_ABI32) \
            && defined(__MIPSEB__)) \
        || (defined(__arm__) && defined(__ARM_EABI__) \
            && defined(__ARMEB__))
static inline int lookup_dcookie(cookie_t cookie, char * buf, size_t size)
{
        return syscall(__NR_lookup_dcookie, (unsigned long)(cookie >> 32),
                       (unsigned long)(cookie & 0xffffffff), buf, size);
}
#elif (defined(__mips__) && (_MIPS_SIM == _MIPS_SIM_ABI32)) \
        || (defined(__arm__) && defined(__ARM_EABI__)) \
        || (defined(__tile__) && !defined(__LP64__))
static inline int lookup_dcookie(cookie_t cookie, char * buf, size_t size)
{
        return syscall(__NR_lookup_dcookie,
                       (unsigned long)(cookie & 0xffffffff),
                       (unsigned long)(cookie >> 32), buf, size);
}
#else
static inline int lookup_dcookie(cookie_t cookie, char * buf, size_t size)
{
        return syscall(__NR_lookup_dcookie, cookie, buf, size);
}
#endif

int main()
{
    char buf[LENGTH];
    cookie_t c = 0;

    lookup_dcookie(0x12345678abcdefabLL, (char *)0x11223344, LENGTH);
    //staptest// lookup_dcookie (0x12345678abcdefab, 0x11223344, 0xff) = -NNNN

    lookup_dcookie(-1, (char *)0, 0);
    //staptest// lookup_dcookie (0xffffffffffffffff, 0x0, 0x0) = -NNNN

    lookup_dcookie(0, (char *)-1, 0);
#ifdef __s390__
    //staptest// lookup_dcookie (0x0, 0x[7]?[f]+, 0x0) = -NNNN
#else
    //staptest// lookup_dcookie (0x0, 0x[f]+, 0x0) = -NNNN
#endif

    lookup_dcookie(0, (char *)0, -1);
    //staptest// lookup_dcookie (0x0, 0x0, 0x[f]+) = -NNNN


    return 0;
}
