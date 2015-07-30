#include <sys/types.h>

provider alpha {
	probe request__start(string, uint8_t, uint16_t, int, void *);
	probe request__one(string, uint8_t, uint32_t, int, int);
	probe client__two(int, int);
	probe client__three(int, string, pid_t, zoneid_t);
	probe input__stop(int, int, uint32_t, uint32_t, int8_t, uint8_t*, double*);
};

#ifdef DCL_AFTER_PROVIDER
typedef unsigned short int __u_short;
typedef const static unsigned short __u_c_short;
#endif

#pragma D attributes Unknown provider alpha provider
