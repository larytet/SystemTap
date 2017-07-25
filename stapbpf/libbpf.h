/* eBPF mini library */
#ifndef __LIBBPF_H
#define __LIBBPF_H

struct bpf_insn;

int bpf_create_map(enum bpf_map_type map_type, unsigned key_size,
		   unsigned value_size, unsigned max_entries, unsigned flags);
int bpf_update_elem(int fd, void *key, void *value, unsigned long long flags);
int bpf_lookup_elem(int fd, void *key, void *value);
int bpf_delete_elem(int fd, void *key);
int bpf_get_next_key(int fd, void *key, void *next_key);

int bpf_prog_load(enum bpf_prog_type prog_type,
		  const struct bpf_insn *insns, int insn_len,
		  const char *license, int kern_version);

int bpf_obj_pin(int fd, const char *pathname);
int bpf_obj_get(const char *pathname);

#define LOG_BUF_SIZE 65536
extern char bpf_log_buf[LOG_BUF_SIZE];

/* a helper structure used by eBPF C program
 * to describe map attributes to elf_bpf loader
 */
struct bpf_map_def {
        unsigned int type;
        unsigned int key_size;
        unsigned int value_size;
        unsigned int max_entries;
        unsigned int map_flags;
};

/* create RAW socket and bind to interface 'name' */
int open_raw_sock(const char *name);

struct perf_event_attr;
int perf_event_open(struct perf_event_attr *attr, int pid, int cpu,
		    int group_fd, unsigned long flags);
#endif
