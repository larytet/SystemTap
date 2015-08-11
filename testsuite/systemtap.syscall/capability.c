/* COVERAGE: capget capset */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include <sys/types.h>
#include <unistd.h>

#include <linux/capability.h>

#define capget(x,y) syscall(__NR_capget,x,y)
#define capset(x,y) syscall(__NR_capset,x,y)

static struct __user_cap_header_struct header;
static struct __user_cap_data_struct data;

int main()
{
    header.version = _LINUX_CAPABILITY_VERSION;
    header.pid = getpid();
    capget(&header, &data);
    //staptest// capget (XXXX, XXXX) = 0

    capset(&header, &data);
    //staptest// capset (XXXX, XXXX) = 0

    capget((cap_user_header_t)-1, 0);
#ifdef __s390__
    //staptest// capget (0x[7]?[f]+, 0x0) = NNNN (EFAULT)
#else
    //staptest// capget (0x[f]+, 0x0) = NNNN (EFAULT)
#endif

    capget(0, (cap_user_data_t)-1);
#ifdef __s390__
    //staptest// capget (0x0, 0x[7]?[f]+) = NNNN (EFAULT)
#else
    //staptest// capget (0x0, 0x[f]+) = NNNN (EFAULT)
#endif

    capset((cap_user_header_t)-1, 0);
#ifdef __s390__
    //staptest// capset (0x[7]?[f]+, 0x0) = NNNN (EFAULT)
#else
    //staptest// capset (0x[f]+, 0x0) = NNNN (EFAULT)
#endif

    capset(0, (cap_user_data_t)-1);
#ifdef __s390__
    //staptest// capset (0x0, 0x[7]?[f]+) = NNNN (EFAULT)
#else
    //staptest// capset (0x0, 0x[f]+) = NNNN (EFAULT)
#endif

    return 0;
}
