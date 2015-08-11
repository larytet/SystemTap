/* COVERAGE: perf_event_open */

#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

#ifdef __NR_perf_event_open
#include <linux/perf_event.h>
// Note that the man page says you need to include the following
// file. However, if you aren't using hardware breakpoint events
// (which this testcase doesn't), you don't really need it. Some
// kernels, like 2.6.32-504.el6.s390x, don't have the header.
//
// #include <linux/hw_breakpoint.h>

static inline int
perf_event_open(struct perf_event_attr *attr, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}
#endif

int main()
{
#ifdef __NR_perf_event_open
    struct perf_event_attr pe;
    int fd;
    long long count;

    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_SOFTWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_SW_CPU_CLOCK;
    pe.disabled = 0;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    fd = perf_event_open(&pe, 0, -1, -1, 0);
    //staptest// perf_event_open (XXXX, 0, -1, -1, 0x0) = NNNN

    sleep(1);
    
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    read(fd, &count, sizeof(count));
    
    // Limit testing.

    fd = perf_event_open((struct perf_event_attr*)-1, 0, -1, -1, 0);
#ifdef __s390__
    //staptest// perf_event_open (0x[7]?[f]+, 0, -1, -1, 0x0) = -NNNN
#else
    //staptest// perf_event_open (0x[f]+, 0, -1, -1, 0x0) = -NNNN
#endif

    fd = perf_event_open(&pe, -1, -1, -1, 0);
    //staptest// perf_event_open (XXXX, -1, -1, -1, 0x0) = -NNNN

    fd = perf_event_open(&pe, 0, -1, -1, -1);
    //staptest// perf_event_open (XXXX, 0, -1, -1, PERF_FLAG_[^ ]+|XXXX) = -NNNN
    
    close(fd);
#endif
    return 0;
}
