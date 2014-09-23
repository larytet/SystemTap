/* COVERAGE: syslog */

#include <stdio.h>
#include <sys/klog.h>

int main()
{
    char buf[1024];

    /* The glibc wrapper function around the kernel's syslog syscall
     * is called klogctl(), to avoid conflicts with syslog(3). */

    klogctl(0 /* SYSLOG_ACTION_CLOSE */, NULL, 0);
    //staptest// syslog (0, 0x0, 0)

    /* NB: we can't test SYSLOG_ACTION_READ here, because it can
       block in the case of an empty kernel kmsg buffer. */

    klogctl(3 /* SYSLOG_ACTION_READ_ALL */, buf, sizeof(buf) - 1);
    //staptest// syslog (3, XXXX, 1023)

    klogctl(-1, buf, sizeof(buf) - 1);
    //staptest// syslog (-1, XXXX, 1023)

    klogctl(0 /* SYSLOG_ACTION_CLOSE */, (char *)-1, 0);
#ifdef __s390__
    //staptest// syslog (0, 0x[7]?[f]+, 0)
#else
    //staptest// syslog (0, 0x[f]+, 0)
#endif

    klogctl(0 /* SYSLOG_ACTION_CLOSE */, NULL, -1);
    //staptest// syslog (0, 0x0, -1)

    return 0;
}
