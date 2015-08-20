/* COVERAGE: vhangup */

#include <unistd.h>
#include <sys/types.h>
#include <linux/capability.h>
#include <sys/syscall.h>

#define capget(x,y) syscall(__NR_capget,x,y)
#define capset(x,y) syscall(__NR_capset,x,y)

static struct __user_cap_header_struct header;
static struct __user_cap_data_struct data;

int main()
{
    // Ensure vhangup() won't be able to succed.
    header.version = _LINUX_CAPABILITY_VERSION;
    header.pid = getpid();
    capget(&header, &data);
    data.effective &= ~(1 << CAP_SYS_TTY_CONFIG);
    capset(&header, &data);

    // Test vhangup()
    vhangup();
    //staptest// vhangup () = -NNNN (EPERM)

    return 0;
}
