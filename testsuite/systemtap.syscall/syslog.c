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

    klogctl(2 /* SYSLOG_ACTION_READ */, buf, sizeof(buf) - 1);
    //staptest// syslog (2, XXXX, 1023)

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
