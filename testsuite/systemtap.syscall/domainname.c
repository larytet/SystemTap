/* COVERAGE: setdomainname getdomainname */

#include <unistd.h>
#include <linux/utsname.h>
#include <stdio.h>

#define MAX_NAME_LEN __NEW_UTS_LEN
static char domain_name[MAX_NAME_LEN];

int main() {
    // The backend for getdomainname() appears to be sys_uname(). This is true
    // except for alpha and sparc. Alpha is a history and no available sparc
    // HW to test on. So skipping this instead of writing a testcase for blind.

    getdomainname(domain_name, sizeof(domain_name));

    // Notice we aren't calling setdomainname() so that it will succeed.
    // This is on purpose. We don't want to change the domainname.
    //
    // setdomainname(domain_name, sizeof(domain_name));

    setdomainname((const char *)-1, sizeof(domain_name));
#ifdef __s390__
    //staptest// setdomainname (0x[7]?[f]+, NNNN) = -NNNN
#else
    //staptest// setdomainname (0x[f]+, NNNN) = -NNNN
#endif

    setdomainname(domain_name, -1);
    //staptest// setdomainname ("[[[[[a-zA-Z0-9\.-]+!!!!(none)]]]]", -1) = -NNNN

    return 0;
}
