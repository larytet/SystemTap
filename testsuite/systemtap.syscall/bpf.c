/* COVERAGE: bpf */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#ifdef __NR_bpf

#include <linux/bpf.h>

static inline int __bpf(int cmd, union bpf_attr *attr, unsigned int size)
{
    return syscall(__NR_bpf, cmd, attr, size);
}

int main()
{
    int fd;

    union bpf_attr attr = {
        .map_type = BPF_MAP_TYPE_HASH,
        .key_size = 4,
        .value_size = 4,
        .max_entries = 2
    };

    fd = __bpf(BPF_MAP_CREATE, &attr, sizeof(attr));
    //staptest// [[[[bpf (BPF_MAP_CREATE, XXXX, NNNN) = NNNN!!!!ni_syscall () = -NNNN]]]]
    close(fd);

    // Limit testing

    __bpf(-1, NULL, 0);
    //staptest// [[[[bpf (0x[f]+, 0x0, 0)!!!!ni_syscall ()]]]] = -NNNN

    __bpf(0, (union bpf_attr *)-1, 0);
    //staptest// [[[[bpf (BPF_MAP_CREATE, 0x[f]+, 0)!!!!ni_syscall ()]]]] = -NNNN

    __bpf(0, NULL, -1);
    //staptest// [[[[bpf (BPF_MAP_CREATE, 0x0, 4294967295)!!!!ni_syscall ()]]]] = -NNNN

    return 0;
}
#else
int main()
{
    return 0;
}
#endif
