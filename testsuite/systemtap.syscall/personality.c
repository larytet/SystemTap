/* COVERAGE: personality */

#include <sys/personality.h>
#include <linux/version.h>

int main()
{
    // The type of personality argument changed in upstream 
    // commit 485d5276 from ulong to uint32. This patch is
    // present in vanila kernel v2.6.35-rc2~25. But apparently
    // it got backported to rhel6 distribution kernels 2.6.32.
    // So expecting uint32 here and letting this fail on rhel5.

    personality(0xffffffff);
    //staptest// personality (0xffffffff) = NNNN

    personality(0x12345678);
    //staptest// personality (0x12345678) = NNNN

    personality(-1);
    //staptest// personality (0xffffffff) = NNNN

    return 0;
}
