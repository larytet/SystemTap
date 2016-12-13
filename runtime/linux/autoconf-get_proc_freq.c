#if defined(__powerpc__)
#include <asm/machdep.h>

unsigned long ____autoconf_func(void)
{
    unsigned long proc_freq = 0;
    if (ppc_md.get_proc_freq)
	proc_freq = ppc_md.get_proc_freq(0);
    return proc_freq;
}
#endif
