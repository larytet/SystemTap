/* 
 *
 *
 */

#include <sys/types.h>
#define Type1 unsigned int
#define Type2 int
#define Type3 char *

provider gamma {
	/* value, value, value, value, value */
	probe probe_1(value, uint8_value, uint16_value, value, void *);
	/* value, value, value, value, result */
	probe probe_2(value, uint8_value, uint32_value, value, int);
	/* client value, value */
	probe probe_3(value, int);
	/* value, value, value, zone id */
	probe probe_4(value, value, uint_value, uint_value);
	/* value */
	probe probe_5(int);
	/* value, value, value, value */
	probe probe_6(uint32_value, uint32_value, void *, string);
	/* value, value, value, value */
	probe probe_7(uint32_value, uint32_value, void *, string);
	/* value, event value, value* */
	probe probe_8(value, uint8_value, void *);
	/* value, value, value, value, value, value, value */
	probe probe_9(value, value, uint32_value, uint32_value, int8_value, uint8_t*, double*);
        probe probe_10(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, Type1);
        probe probe_11(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, Type2);
        probe probe_12(Type3);
	probe probe_13(char *value, unsigned long value,
                               unsigned long value, unsigned long value);
	probe probe_14(void *, const char *, const char *, int);
	probe probe_15(void * a, const char * b, int c, const char * d);
};

#pragma D attributes provider


