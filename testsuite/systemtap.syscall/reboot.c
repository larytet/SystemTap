/* COVERAGE: reboot */

#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>

int main()
{
    // Since we may or may not be run as root, these commands could
    // succeed or fail. So, ignore most of the return values.

    reboot(RB_ENABLE_CAD);
    //staptest// reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_CAD_ON, XXXX)

    reboot(RB_DISABLE_CAD);
    //staptest// reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_CAD_OFF, XXXX)

    reboot(-1);
    //staptest// reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, 0xffffffff, XXXX)
}
