/* COVERAGE: get_thread_area set_thread_area */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/unistd.h>

#if defined __NR_get_thread_area && defined __NR_set_thread_area

#include <asm/ldt.h>

static inline int
__get_thread_area(struct user_desc *u_info)
{
    return syscall(__NR_get_thread_area, u_info);
}

static inline int
__set_thread_area(struct user_desc *u_info)
{
    return syscall(__NR_set_thread_area, u_info);
}

int
main()
{
    struct user_desc ud;
    ud.entry_number = -1;
    __set_thread_area(&ud);
    //staptest// [[[[set_thread_area ({entry_number=XXXX, base_addr=XXXX, limit=XXXX, seg_32bit=XXXX, contents=XXXX, read_exec_only=XXXX, limit_in_pages=XXXX, seg_not_present=XXXX, useable=XXXX[[[[, lm=XXXX!!!!]]]]})!!!!ni_syscall ()]]]] = NNNN

    __get_thread_area(&ud);
    //staptest// [[[[get_thread_area ({entry_number=XXXX, base_addr=XXXX, limit=XXXX, seg_32bit=XXXX, contents=XXXX, read_exec_only=XXXX, limit_in_pages=XXXX, seg_not_present=XXXX, useable=XXXX[[[[, lm=XXXX!!!!]]]]})!!!!ni_syscall ()]]]] = NNNN

    // Limit testing

    __set_thread_area((struct user_desc *)-1);
    //staptest// [[[[set_thread_area (0x[f]+)!!!!ni_syscall ()]]]] = NNNN

    __get_thread_area((struct user_desc *)-1);
    //staptest// [[[[get_thread_area (0x[f]+)!!!!ni_syscall ()]]]] = NNNN

    return 0;
}
#else
int
main()
{
    return 0;
}
#endif
