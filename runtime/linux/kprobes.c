/* -*- linux-c -*-
 * Common functions for using kprobes
 * Copyright (C) 2014 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _KPROBES_C_
#define _KPROBES_C_

// Warn of misconfigured kernels
#if !defined(CONFIG_KPROBES)
#error "Need CONFIG_KPROBES!"
#endif

#ifndef KRETACTIVE
#define KRETACTIVE (max(15, 6 * (int)num_possible_cpus()))
#endif


struct stap_dwarf_kprobe {
   union { struct kprobe kp; struct kretprobe krp; } u;
   #ifdef __ia64__
   // PR6028: We register a second dummy probe at the same address so that the
   // kernel uses aggr_kprobe. This is needed ensure that the bspcache is always
   // valid.
   struct kprobe dummy;
   #endif
};


struct stap_dwarf_probe {
   const unsigned return_p:1;
   const unsigned maxactive_p:1;
   const unsigned optional_p:1;
   unsigned registered_p:1;
#ifdef STP_ON_THE_FLY
   unsigned enabled_p:1;
#endif
   const unsigned short maxactive_val;

   // data saved in the kretprobe_instance packet
   const unsigned short saved_longs;
   const unsigned short saved_strings;

   // These macros declare the module and section strings as either const char[]
   // or const char * const. Their actual types are determined at translate-time
   // in dwarf_derived_probe_group::emit_module_decls().
   STAP_DWARF_PROBE_STR_module;
   STAP_DWARF_PROBE_STR_section;

   const unsigned long address;
   const struct stap_probe * const probe;
   const struct stap_probe * const entry_probe;
};


// Forward declare the master entry functions (stap-generated)
static int
enter_kprobe_probe(struct kprobe *inst,
                   struct pt_regs *regs);
static int
enter_kretprobe_common(struct kretprobe_instance *inst,
                       struct pt_regs *regs, int entry);

// Helper entry functions for kretprobes
static int
enter_kretprobe_probe(struct kretprobe_instance *inst,
                      struct pt_regs *regs)
{
   return enter_kretprobe_common(inst, regs, 0);
}

static int
enter_kretprobe_entry_probe(struct kretprobe_instance *inst,
                            struct pt_regs *regs)
{
   return enter_kretprobe_common(inst, regs, 1);
}


#endif /* _KPROBES_C_ */
