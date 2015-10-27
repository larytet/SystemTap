/* COVERAGE: gethostname uname sethostname */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/syscall.h>

#define HLEN 1024

int main()
{
    char orig_hname[HLEN];
    char *new_hname = "stap_test";
    int rc;
    struct utsname uts;

    /* glibc likes to substitute utsname() for gethostname(). So,
     * let's force gethostname()'s use (assuming the syscall actually
     * exists on this platform). */
#ifdef __NR_gethostname
    syscall(__NR_gethostname, orig_hname, sizeof(orig_hname));
    //staptest// gethostname (XXXX, NNNN) = 0

    syscall(__NR_gethostname, -1, sizeof(orig_hname));
    //staptest// gethostname (0x[f]+, NNNN) = -NNNN

    syscall(__NR_gethostname, orig_hname, -1);
    //staptest// gethostname (XXXX, -1) = -NNNN
#endif

    uname(&uts);
    //staptest// uname (XXXX) = 0

    uname((struct utsname *)-1);
#ifdef __s390__
    //staptest// uname (0x[7]?[f]+) = -NNNN
#else
    //staptest// uname (0x[f]+) = -NNNN
#endif

    sethostname((char *)-1, sizeof(new_hname));
#ifdef __s390__
    //staptest// sethostname (0x[7]?[f]+, NNNN) = -NNNN
#else
    //staptest// sethostname (0x[f]+, NNNN) = -NNNN
#endif

    sethostname(NULL, -1);
    //staptest// sethostname (0x0, -1) = -NNNN

    // Notice we aren't calling sethostname() so that it will
    // succeed. This is on purpose, since we can't guarentee that
    // uname()/gethostname() returns exactly what sethostname() wants
    // (and the machine may have more than one name if it has more
    // than one interface).
    return 0;
}
