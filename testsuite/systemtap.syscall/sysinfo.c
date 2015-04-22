/* COVERAGE: sysinfo */

#include <sys/sysinfo.h>

int main()
{
    struct sysinfo si;

    sysinfo(&si);
    //staptest// sysinfo ({uptime=NNNN, loads=[NNNN, NNNN, NNNN], totalram=NNNN, freeram=NNNN, sharedram=NNNN, bufferram=NNNN, totalswap=NNNN, freeswap=NNNN, procs=NNNN}) = 0

    sysinfo((struct sysinfo *)-1);
#ifdef __s390__
    //staptest// sysinfo (0x[7]?[f]+) = -NNNN
#else
    //staptest// sysinfo (0x[f]+) = -NNNN
#endif


    return 0;
}
