/* stapbpf.cxx - SystemTap BPF loader
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2016 Red Hat, Inc.
 *
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cassert>
#include <csignal>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>
#include <limits.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include "bpfinterp.h"

extern "C" {
#include <linux/bpf.h>
#include <linux/perf_event.h>
/* Introduced in 4.1. */
#ifndef PERF_EVENT_IOC_SET_BPF
#define PERF_EVENT_IOC_SET_BPF _IOW('$', 8, __u32)
#endif
#include <libelf.h>
}

#include "config.h"
#include "../git_version.h"
#include "../version.h"
#include "../bpf-internal.h"

#ifndef EM_BPF
#define EM_BPF 0xeb9f
#endif
#ifndef R_BPF_MAP_FD
#define R_BPF_MAP_FD 1
#endif

using namespace std;

static int group_fd = -1;		// ??? Need one per cpu.
extern "C" { 
int log_level = 0;
};
static int warnings = 1;
static FILE *output_f = stdout;

static const char *module_name;
static const char *module_license;
static Elf *module_elf;

static uint32_t kernel_version;

// Sized by the contents of the "maps" section.
static bpf_map_def *map_attrs;
static std::vector<int> map_fds;

// Sized by the number of sections, so that we can easily
// look them up by st_shndx.
static std::vector<int> prog_fds;

// Programs to run at begin and end of execution.
static Elf_Data *prog_begin;
static Elf_Data *prog_end;

#define DEBUGFS		"/sys/kernel/debug/tracing/"
#define KPROBE_EVENTS	DEBUGFS "kprobe_events"

static void unregister_kprobes(const size_t nprobes);

struct kprobe_data
{
  string args;
  char type;
  int prog_fd;
  int event_id;
  int event_fd;				// ??? Need one per cpu.

  kprobe_data(char t, string s, int fd)
    : args(s), type(t), prog_fd(fd), event_id(-1), event_fd(-1)
  { }
};

static std::vector<kprobe_data> kprobes;


static void __attribute__((noreturn))
fatal(const char *str, ...)
{
  if (module_name)
    fprintf(stderr, "Error loading %s: ", module_name);

  va_list va;
  va_start(va, str);
  vfprintf(stderr, str, va);
  va_end(va);
  
  exit(1);
}

static void
fatal_sys()
{
  fatal("%s\n", strerror(errno));
}

static void
fatal_elf()
{
  fatal("%s\n", elf_errmsg(-1));
}

static int
create_group_fds()
{
  perf_event_attr peattr;

  memset(&peattr, 0, sizeof(peattr));
  peattr.size = sizeof(peattr);
  peattr.disabled = 1;

  return group_fd = perf_event_open(&peattr, -1, 0, -1, 0);
}

static void
instantiate_maps (Elf64_Shdr *shdr, Elf_Data *data)
{
  if (shdr->sh_entsize != sizeof(bpf_map_def))
    fatal("map entry size mismatch (%zu != %zu)\n",
	  (size_t)shdr->sh_entsize, sizeof(bpf_map_def));

  size_t i, n = shdr->sh_size / sizeof(bpf_map_def);
  struct bpf_map_def *attrs = static_cast<bpf_map_def *>(data->d_buf);

  map_attrs = attrs;
  map_fds.assign(n, -1);

  for (i = 0; i < n; ++i)
    {
      int fd = bpf_create_map(static_cast<bpf_map_type>(attrs[i].type),
			      attrs[i].key_size, attrs[i].value_size,
			      attrs[i].max_entries, attrs[i].map_flags);
      if (fd < 0)
	fatal("map entry %zu: %s\n", i, strerror(errno));
      map_fds[i] = fd;
    }
}

static int
prog_load(Elf_Data *data)
{
  if (data->d_size % sizeof(bpf_insn))
    fatal("program size not a multiple of %zu\n", sizeof(bpf_insn));

  int fd = bpf_prog_load(BPF_PROG_TYPE_KPROBE,
			 static_cast<bpf_insn *>(data->d_buf),
			 data->d_size, module_license, kernel_version);
  if (fd < 0)
    {
      if (bpf_log_buf[0] != 0)
	fatal("bpf program load failed: %s\n%s\n",
	      strerror(errno), bpf_log_buf);
      else
	fatal("bpf program load failed: %s\n", strerror(errno));
    }
  return fd;
}

static void
prog_relocate(Elf_Data *prog_data, Elf_Data *rel_data,
	      Elf_Data *sym_data, Elf_Data *str_data,
	      const char *prog_name, unsigned maps_idx, bool allocated)
{
  bpf_insn *insns = static_cast<bpf_insn *>(prog_data->d_buf);
  Elf64_Rel *rels = static_cast<Elf64_Rel *>(rel_data->d_buf);
  Elf64_Sym *syms = static_cast<Elf64_Sym *>(sym_data->d_buf);

  if (prog_data->d_size % sizeof(bpf_insn))
    fatal("program size not a multiple of %zu\n", sizeof(bpf_insn));
  if (rel_data->d_type != ELF_T_REL
      || rel_data->d_size % sizeof(Elf64_Rel))
    fatal("invalid reloc metadata\n");
  if (sym_data->d_type != ELF_T_SYM
      || sym_data->d_size % sizeof(Elf64_Sym))
    fatal("invalid symbol metadata\n");

  size_t psize = prog_data->d_size;
  size_t nrels = rel_data->d_size / sizeof(Elf64_Rel);
  size_t nsyms = sym_data->d_size / sizeof(Elf64_Sym);

  for (size_t i = 0; i < nrels; ++i)
    {
      uint32_t sym = ELF64_R_SYM(rels[i].r_info);
      uint32_t type = ELF64_R_TYPE(rels[i].r_info);
      unsigned long long r_ofs = rels[i].r_offset;
      size_t fd_idx;

      if (type != R_BPF_MAP_FD)
	fatal("invalid relocation type %u\n", type);
      if (sym >= nsyms)
	fatal("invalid symbol index %u\n", sym);
      if (r_ofs >= psize || r_ofs % sizeof(bpf_insn))
	fatal("invalid relocation offset at %s+%llu\n", prog_name, r_ofs);

      if (sym >= nsyms)
	fatal("invalid relocation symbol %u\n", sym);
      if (syms[sym].st_shndx != maps_idx
	  || syms[sym].st_value % sizeof(bpf_map_def)
	  || (fd_idx = syms[sym].st_value / sizeof(bpf_map_def),
	      fd_idx >= map_fds.size()))
	{
	  const char *name = "";
	  if (syms[sym].st_name < str_data->d_size)
	    name = static_cast<char *>(str_data->d_buf) + syms[sym].st_name;
	  if (*name)
	    fatal("symbol %s does not reference a map\n", name);
	  else
	    fatal("symbol %u does not reference a map\n", sym);
	}

      bpf_insn *insn = insns + (r_ofs / sizeof(bpf_insn));
      if (insn->code != (BPF_LD | BPF_IMM | BPF_DW))
	fatal("invalid relocation insn at %s+%llu\n", prog_name, r_ofs);

      insn->src_reg = BPF_PSEUDO_MAP_FD;
      insn->imm = (allocated ? map_fds[fd_idx] : fd_idx);
    }
}

static void
maybe_collect_kprobe(const char *name, unsigned name_idx,
		     unsigned fd_idx, Elf64_Addr offset)
{
  char type;
  string arg;

  if (strncmp(name, "kprobe/", 7) == 0)
    {
      string line;
      const char *stext = NULL;
      type = 'p';
      name += 7;

      ifstream syms("/proc/kallsyms");
      if (!syms)
        fatal("error opening /proc/kallsyms: %s\n", strerror(errno));

      // get value of symbol _stext and add it to the offset found in name.
      while (getline(syms, line))
        {
          const char *l = line.c_str();
          if (strncmp(l + 19, "_stext", 6) == 0)
            {
              stext = l;
              break;
            }
        }

      if (stext == NULL)
        fatal("could not find _stext in /proc/kallsyms");

      unsigned long addr = strtoul(stext, NULL, 16);
      addr += strtoul(name, NULL, 16);
      stringstream ss;
      ss << "0x" << hex << addr;
      arg = ss.str();
    }
  else if (strncmp(name, "kretprobe/", 10) == 0)
    type = 'r', arg = name + 10; 
  else
    return;

  int fd = -1;
  if (fd_idx >= prog_fds.size() || (fd = prog_fds[fd_idx]) < 0)
    fatal("probe %u section %u not loaded\n", name_idx, fd_idx);
  if (offset != 0)
    fatal("probe %u offset non-zero\n", name_idx);

  kprobes.push_back(kprobe_data(type, arg, fd));
}

static void
kprobe_collect_from_syms(Elf_Data *sym_data, Elf_Data *str_data)
{
  Elf64_Sym *syms = static_cast<Elf64_Sym *>(sym_data->d_buf);
  size_t nsyms = sym_data->d_type / sizeof(Elf64_Sym);

  if (sym_data->d_type != ELF_T_SYM
      || sym_data->d_size % sizeof(Elf64_Sym))
    fatal("invalid kprobes symbol metadata\n");

  for (size_t i = 0; i < nsyms; ++i)
    {
      const char *name;
      if (syms[i].st_name < str_data->d_size)
	name = static_cast<char *>(str_data->d_buf) + syms[i].st_name;
      else
	fatal("symbol %u has invalid string index\n", i);
      maybe_collect_kprobe(name, i, syms[i].st_shndx, syms[i].st_value);
    }
}

static void
register_kprobes()
{
  size_t nprobes = kprobes.size();
  if (nprobes == 0)
    return;
    
  int fd = open(KPROBE_EVENTS, O_WRONLY);
  if (fd < 0)
    fatal("Error opening %s: %s\n", KPROBE_EVENTS, strerror(errno));

  const int pid = getpid();

  for (size_t i = 0; i < nprobes; ++i)
    {
      kprobe_data &k = kprobes[i];
      char msgbuf[128];
      
      ssize_t olen = snprintf(msgbuf, sizeof(msgbuf), "%c:p%d_%zu %s",
			      k.type, pid, i, k.args.c_str());
      if ((size_t)olen >= sizeof(msgbuf))
	{
	  fprintf(stderr, "Buffer overflow creating probe %zu\n", i);
	  if (i == 0)
	    goto fail_0;
	  nprobes = i - 1;
	  goto fail_n;
	}

      if (log_level > 1)
        fprintf(stderr, "Associating probe %zu with kprobe %s\n", i, msgbuf);
      
      ssize_t wlen = write(fd, msgbuf, olen);
      if (wlen != olen)
	{
	  fprintf(stderr, "Error creating probe %zu: %s\n",
		  i, strerror(errno));
	  if (i == 0)
	    goto fail_0;
	  nprobes = i - 1;
	  goto fail_n;
	}
    }
  close(fd);

  for (size_t i = 0; i < nprobes; ++i)
    {
      char fnbuf[PATH_MAX];
      ssize_t len = snprintf(fnbuf, sizeof(fnbuf),
			     DEBUGFS "events/kprobes/p%d_%zu/id", pid, i);
      if ((size_t)len >= sizeof(bpf_log_buf))
	{
	  fprintf(stderr, "Buffer overflow creating probe %zu\n", i);
	  goto fail_n;
	}

      fd = open(fnbuf, O_RDONLY);
      if (fd < 0)
	{
	  fprintf(stderr, "Error opening probe event id %zu: %s\n",
		  i, strerror(errno));
	  goto fail_n;
	}

      char msgbuf[128];
      len = read(fd, msgbuf, sizeof(msgbuf) - 1);
      if (len < 0)
	{
	  fprintf(stderr, "Error reading probe event id %zu: %s\n",
		  i, strerror(errno));
	  goto fail_n;
	}
      close(fd);

      msgbuf[len] = 0;
      kprobes[i].event_id = atoi(msgbuf);
    }

  // ??? Iterate to enable on all cpus, each with a different group_fd.
  {
    perf_event_attr peattr;

    memset(&peattr, 0, sizeof(peattr));
    peattr.size = sizeof(peattr);
    peattr.type = PERF_TYPE_TRACEPOINT;
    peattr.sample_type = PERF_SAMPLE_RAW;
    peattr.sample_period = 1;
    peattr.wakeup_events = 1;

    for (size_t i = 0; i < nprobes; ++i)
      {
	kprobe_data &k = kprobes[i];
        peattr.config = k.event_id;

        fd = perf_event_open(&peattr, -1, 0, group_fd, 0);
        if (fd < 0)
	  {
	    fprintf(stderr, "Error opening probe id %zu: %s\n",
		    i, strerror(errno));
	    goto fail_n;
	  }
        k.event_fd = fd;

        if (ioctl(fd, PERF_EVENT_IOC_SET_BPF, k.prog_fd) < 0)
	  {
	    fprintf(stderr, "Error installing bpf for probe id %zu: %s\n",
		    i, strerror(errno));
	    goto fail_n;
	  }
      }
  }
  return;

 fail_n:
  unregister_kprobes(nprobes);
 fail_0:
  exit(1);
}

static void
unregister_kprobes(const size_t nprobes)
{
  if (nprobes == 0)
    return;

  int fd = open(DEBUGFS "kprobe_events", O_WRONLY);
  if (fd < 0)
    return;


  const int pid = getpid();
  for (size_t i = 0; i < nprobes; ++i)
    {
      close(kprobes[i].event_fd);

      char msgbuf[128];
      ssize_t olen = snprintf(msgbuf, sizeof(msgbuf), "-:p%d_%zu",
			      pid, i);
      ssize_t wlen = write(fd, msgbuf, olen);
      if (wlen < 0)
	fprintf(stderr, "Error removing probe %zu: %s\n",
		i, strerror(errno));
    }
  close(fd);
}

static void
init_internal_globals()
{
  using namespace bpf;

  int key = globals::EXIT;
  long val = 0;

  if (bpf_update_elem(map_fds[globals::internal_map_idx],
                     (void*)&key, (void*)&val, BPF_EXIST) != 0)
    fatal("Error updating pid: %s\n", errno);
}

static void
load_bpf_file(const char *module)
{
  module_name = module;
  int fd = open(module, O_RDONLY);
  if (fd < 0)
    fatal_sys();

  elf_version(EV_CURRENT);

  Elf *elf = elf_begin(fd, ELF_C_READ_MMAP_PRIVATE, NULL);
  if (elf == NULL)
    fatal_elf();
  module_elf = elf;

  Elf64_Ehdr *ehdr = elf64_getehdr(elf);
  if (ehdr == NULL)
    fatal_elf();

  // Byte order should match the host, since we're loading locally.
  {
    const char *end_str;
    switch (ehdr->e_ident[EI_DATA])
      {
      case ELFDATA2MSB:
	if (__BYTE_ORDER == __BIG_ENDIAN)
	  break;
	end_str = "MSB";
	goto err_endian;
      case ELFDATA2LSB:
	if (__BYTE_ORDER == __LITTLE_ENDIAN)
	  break;
	end_str = "LSB";
	goto err_endian;
      case ELFCLASSNONE:
	end_str = "none";
	goto err_endian;
      default:
	end_str = "unknown";
      err_endian:
	fatal("incorrect byte ordering: %s\n", end_str);
      }
  }

  // Tiny bit of sanity checking on the rest of the header.  Since LLVM
  // began by producing files with EM_NONE, accept that too.
  if (ehdr->e_machine != EM_NONE && ehdr->e_machine != EM_BPF)
    fatal("incorrect machine type: %d\n", ehdr->e_machine);

  unsigned shnum = ehdr->e_shnum;
  prog_fds.assign(shnum, -1);

  std::vector<Elf64_Shdr *> shdrs(shnum, NULL);
  std::vector<Elf_Data *> sh_data(shnum, NULL);
  std::vector<const char *> sh_name(shnum, NULL);
  unsigned maps_idx = 0;
  unsigned version_idx = 0;
  unsigned license_idx = 0;
  unsigned kprobes_idx = 0;
  unsigned begin_idx = 0;
  unsigned end_idx = 0;

  // First pass to identify special sections, and make sure
  // all data is readable.
  for (unsigned i = 1; i < shnum; ++i)
    {
      Elf_Scn *scn = elf_getscn(elf, i);
      if (!scn)
	fatal_elf();

      Elf64_Shdr *shdr = elf64_getshdr(scn);
      if (!shdr)
	fatal_elf();

      const char *shname = elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name);
      if (!shname)
	fatal_elf();

      // We need not consider any empty sections.
      if (shdr->sh_size == 0 || !*shname)
	continue;

      Elf_Data *data = elf_getdata(scn, NULL);
      if (data == NULL)
	fatal_elf();

      shdrs[i] = shdr;
      sh_name[i] = shname;
      sh_data[i] = data;

      if (strcmp(shname, "license") == 0)
	license_idx = i;
      else if (strcmp(shname, "version") == 0)
	version_idx = i;
      else if (strcmp(shname, "maps") == 0)
	maps_idx = i;
      else if (strcmp(shname, "kprobes") == 0)
	kprobes_idx = i;
      else if (strcmp(shname, "stap_begin") == 0)
	begin_idx = i;
      else if (strcmp(shname, "stap_end") == 0)
	end_idx = i;
    }

  // Two special sections are not optional.
  if (license_idx != 0)
    module_license = static_cast<char *>(sh_data[license_idx]->d_buf);
  else
    fatal("missing license section\n");
  if (version_idx != 0)
    {
      unsigned long long size = shdrs[version_idx]->sh_size;
      if (size != 4)
	fatal("invalid version size (%llu)\n", size);
      memcpy(&kernel_version, sh_data[version_idx]->d_buf, 4);
    }
  else
    fatal("missing version section\n");

  // Create bpf maps as required.
  if (maps_idx != 0)
    instantiate_maps(shdrs[maps_idx], sh_data[maps_idx]);

  // Relocate all programs that require it.
  for (unsigned i = 1; i < shnum; ++i)
    {
      Elf64_Shdr *rel_hdr = shdrs[i];
      if (rel_hdr == NULL || rel_hdr->sh_type != SHT_REL)
	continue;

      unsigned progi = rel_hdr->sh_info;
      if (progi == 0 || progi >= shnum)
	fatal("invalid section info %u->%u\n", i, progi);
      Elf64_Shdr *prog_hdr = shdrs[progi];

      unsigned symi = rel_hdr->sh_link;
      if (symi == 0 || symi >= shnum)
	fatal("invalid section link %u->%u\n", i, symi);
      Elf64_Shdr *sym_hdr = shdrs[symi];

      unsigned stri = sym_hdr->sh_link;
      if (stri == 0 || stri >= shnum)
	fatal("invalid section link %u->%u\n", symi, stri);

      if (prog_hdr->sh_flags & SHF_EXECINSTR)
	prog_relocate(sh_data[progi], sh_data[i], sh_data[symi],
		      sh_data[stri], sh_name[progi], maps_idx,
		      prog_hdr->sh_flags & SHF_ALLOC);
    }

  // Load all programs that require it.
  for (unsigned i = 1; i < shnum; ++i)
    {
      Elf64_Shdr *shdr = shdrs[i];
      if ((shdr->sh_flags & SHF_ALLOC) && (shdr->sh_flags & SHF_EXECINSTR))
	prog_fds[i] = prog_load(sh_data[i]);
    }

  // Remember begin and end probes.
  if (begin_idx)
    {
      Elf64_Shdr *shdr = shdrs[begin_idx];
      if (shdr->sh_flags & SHF_EXECINSTR)
	prog_begin = sh_data[begin_idx];
    }
  if (end_idx)
    {
      Elf64_Shdr *shdr = shdrs[end_idx];
      if (shdr->sh_flags & SHF_EXECINSTR)
	prog_end = sh_data[end_idx];
    }

  // Record all kprobes.
  if (kprobes_idx != 0)
    {
      // The Preferred Systemtap Way puts kprobe strings into a symbol
      // table, so that multiple kprobes can reference the same program.

      // ??? We don't really have to have a separate kprobe symbol table;
      // we could pull kprobes out of the main symbol table too.  This
      // would probably make it easier for llvm-bpf folks to transition.
      // One would only need to create symbol aliases with custom asm names.

      Elf64_Shdr *sym_hdr = shdrs[kprobes_idx];
      if (sym_hdr->sh_type != SHT_SYMTAB)
	fatal("invalid section type for kprobes section\n");

      unsigned stri = sym_hdr->sh_link;
      if (stri == 0 || stri >= shnum)
	fatal("invalid section link %u->%u\n", kprobes_idx, stri);

      kprobe_collect_from_syms(sh_data[kprobes_idx], sh_data[stri]);
    }
  else
    {
      // The original llvm-bpf way puts kprobe strings into the
      // section name.  Each kprobe has its own program.
      for (unsigned i = 1; i < shnum; ++i)
	maybe_collect_kprobe(sh_name[i], i, i, 0);
    }
}

static int
get_exit_status()
{
  int key = bpf::globals::EXIT;
  long val = 0;

  if (bpf_lookup_elem
       (map_fds[bpf::globals::internal_map_idx], &key, &val) != 0)
    fatal("error during bpf map lookup: %s\n", strerror(errno));

  return val;
}

static void
print_trace_output(pthread_t main_thread)
{
  // Output from printf statements within probe handlers is sent to
  // debugfs via trace_printk. Get this output and copy it to output_f.
  string modname(module_name);
  size_t pos = modname.find_last_of("/");
  if (pos != string::npos)
    modname = modname.substr(pos + 1);

  string start_tag = "<" + modname.erase(modname.size() - 3).erase(4, 1) + ">";
  string end_tag = start_tag;
  end_tag.insert(1, "/");

  while (1)
    {
      ifstream trace_pipe(DEBUGFS "trace_pipe");
      if (!trace_pipe)
        fatal("error opening trace_pipe: %s\n", strerror(errno));

      bool start_tag_seen = false;
      string line, buf;
      while (getline(trace_pipe, line))
        {
          line += "\n";
          if (!start_tag_seen)
            {
              pos = line.find(start_tag);
              if (pos != string::npos)
                {
                  start_tag_seen = true;
                  line = line.substr(pos + start_tag.size(), string::npos);
                }
            }
          if (start_tag_seen)
            {
              pos = line.find(end_tag);
              if (pos != string::npos)
                {
                  line = line.substr(0, pos);
                  start_tag_seen = false;

                  // exit() causes "" to be written to trace_pipe. If
                  // "" is seen and the exit flag is set, wake up main
                  // thread to begin program shutdown.
                  if (line == "" && buf == "" && get_exit_status())
                    {
                      pthread_kill(main_thread, SIGINT);
                      return;
                    }
                }

              buf += line;
              if (fwrite(buf.c_str(), sizeof(char), buf.size(), output_f)
                   != buf.size())
                fatal("error writing to output file: %s\n", strerror(errno));

              fflush(output_f);
              buf = "";
            }
        }
      // If another process opens trace_pipe then we may read EOF. In this
      // case simply reopen the file and continue parsing.
    }
}

static void
usage(const char *argv0)
{
  printf("Usage: %s [-v][-w][-V][-h] [-o FILE] <bpf-file>\n"
	 "  -h, --help       Show this help text\n"
	 "  -v, --verbose    Increase verbosity\n"
	 "  -V, --version    Show version\n"
	 "  -w               Suppress warnings\n"
	 "  -o FILE          Send output to FILE\n",
	 argv0);
}


void
sigint(int s)
{
  // suppress any subsequent SIGINTs that may come from stap parent process
  signal(s, SIG_IGN);

  // set exit flag
  int key = bpf::globals::EXIT;
  long val = 1;

  if (bpf_update_elem
       (map_fds[bpf::globals::internal_map_idx], &key, &val, 0) != 0)
     fatal("error during bpf map update: %s\n", strerror(errno));
}

int
main(int argc, char **argv)
{
  static const option long_opts[] = {
    { "help", 0, NULL, 'h' },
    { "verbose", 0, NULL, 'v' },
    { "version", 0, NULL, 'V' },
  };

  int rc;

  while ((rc = getopt_long(argc, argv, "hvVwo:", long_opts, NULL)) >= 0)
    switch (rc)
      {
      case 'v':
        log_level++;
        break;
      case 'w':
        warnings = 0;
        break;

      case 'o':
	output_f = fopen(optarg, "w");
	if (output_f == NULL)
	  {
	    fprintf(stderr, "Error opening %s for output: %s\n",
		    optarg, strerror(errno));
	    return 1;
	  }
	break;

      case 'V':
        printf("Systemtap BPF loader/runner (version %s, %s)\n"
	       "Copyright (C) 2016 Red Hat, Inc. and others\n" // PRERELEASE
               "This is free software; "
	       "see the source for copying conditions.\n",
	       VERSION, STAP_EXTENDED_VERSION);
	return 0;

      case 'h':
	usage(argv[0]);
	return 0;

      default:
      do_usage:
	usage(argv[0]);
	return 1;
      }
  if (optind != argc - 1)
    goto do_usage;

  load_bpf_file(argv[optind]);
  init_internal_globals();

  if (create_group_fds() < 0)
    fatal("Error creating perf event group: %s\n", strerror(errno));

  register_kprobes();

  // Run the begin probes.
  if (prog_begin)
    {
      bpf_context *c = bpf_context_create(map_fds.size(), map_attrs);

      bpf_interpret(c, prog_begin->d_size / sizeof(bpf_insn),
		    static_cast<bpf_insn *>(prog_begin->d_buf));

      bpf_context_export(c, map_fds.data());
      bpf_context_free(c);
    }

  // Now that the begin probe has run, enable the kprobes.
  ioctl(group_fd, PERF_EVENT_IOC_ENABLE, 0);

  // Wait for ^C; read BPF_OUTPUT events, copying them to output_f.
  signal(SIGINT, (sighandler_t)sigint);
  signal(SIGTERM, (sighandler_t)sigint);
  std::thread(print_trace_output, pthread_self()).detach();

  while (!get_exit_status())
    pause();
    
  // Disable the kprobes before deregistering and running exit probes.
  ioctl(group_fd, PERF_EVENT_IOC_DISABLE, 0);
  close(group_fd);

  // Unregister all probes.
  unregister_kprobes(kprobes.size());

  // Run the end+error probes.
  if (prog_end)
    {
      bpf_context *c = bpf_context_create(map_fds.size(), map_attrs);
      bpf_context_import(c, map_fds.data());
      bpf_interpret(c, prog_end->d_size / sizeof(bpf_insn),
		    static_cast<bpf_insn *>(prog_end->d_buf));
      bpf_context_free(c);
    }

  elf_end(module_elf);
  return 0;
}
