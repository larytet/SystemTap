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


static unsigned long
stapkp_relocate_addr(struct stap_dwarf_probe *sdp)
{
   return _stp_kmodule_relocate(sdp->module, sdp->section, sdp->address);
}


static int
stapkp_register_probe(struct stap_dwarf_probe *sdp,
                      struct stap_dwarf_kprobe *kp)
{
   int rc = 0;

   unsigned long addr = stapkp_relocate_addr(sdp);

   if (addr == 0)
      return 0; // quietly; assume module is absent

   if (sdp->return_p) {

      kp->u.krp.kp.addr = (void *) addr;
      if (sdp->maxactive_p) {
         kp->u.krp.maxactive = sdp->maxactive_val;
      } else {
         kp->u.krp.maxactive = KRETACTIVE;
      }

      kp->u.krp.handler = &enter_kretprobe_probe;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
      if (sdp->entry_probe) {
         kp->u.krp.entry_handler = &enter_kretprobe_entry_probe;
         kp->u.krp.data_size = sdp->saved_longs * sizeof(int64_t) +
                               sdp->saved_strings * MAXSTRINGLEN;
      }
#endif

      // to ensure safeness of bspcache, always use aggr_kprobe on ia64
#ifdef __ia64__
      kp->dummy.addr = kp->u.krp.kp.addr;
      kp->dummy.pre_handler = NULL;
      rc = register_kprobe (& kp->dummy);
      if (rc == 0) {
         rc = register_kretprobe (& kp->u.krp);
         if (rc != 0)
            unregister_kprobe (& kp->dummy);
      }
#else
      rc = register_kretprobe (& kp->u.krp);
#endif

   } else { // !sdp->return_p

      // to ensure safeness of bspcache, always use aggr_kprobe on ia64
      kp->u.kp.addr = (void *) addr;
      kp->u.kp.pre_handler = &enter_kprobe_probe;

#ifdef __ia64__
      kp->dummy.addr = kp->u.kp.addr;
      kp->dummy.pre_handler = NULL;
      rc = register_kprobe (& kp->dummy);
      if (rc == 0) {
         rc = register_kprobe (& kp->u.kp);
         if (rc != 0)
            unregister_kprobe (& kp->dummy);
      }
#else
      rc = register_kprobe (& kp->u.kp);
#endif

   }

   if (rc)
      sdp->registered_p = 0;
   else
      sdp->registered_p = 1;

#ifdef STP_ON_THE_FLY
   // kprobes are enabled by default upon registration
   sdp->enabled_p = sdp->registered_p;
#endif

   return rc;
}


static void
stapkp_add_missed(struct stap_dwarf_probe *sdp,
                  struct stap_dwarf_kprobe *kp)
{
   if (sdp->return_p) {

      atomic_add (kp->u.krp.nmissed, skipped_count());

#ifdef STP_TIMING
      if (kp->u.krp.nmissed)
         _stp_warn ("Skipped due to missed kretprobe/1 on '%s': %d\n",
                    sdp->probe->pp, kp->u.krp.nmissed);
#endif

      atomic_add (kp->u.krp.kp.nmissed, skipped_count());

#ifdef STP_TIMING
      if (kp->u.krp.kp.nmissed)
         _stp_warn ("Skipped due to missed kretprobe/2 on '%s': %lu\n",
                    sdp->probe->pp, kp->u.krp.kp.nmissed);
#endif

   } else {

      atomic_add (kp->u.kp.nmissed, skipped_count());

#ifdef STP_TIMING
      if (kp->u.kp.nmissed)
         _stp_warn ("Skipped due to missed kprobe on '%s': %lu\n",
                    sdp->probe->pp, kp->u.kp.nmissed);
#endif
   }
}


static void
stapkp_unregister_probe(struct stap_dwarf_probe *sdp,
                        struct stap_dwarf_kprobe *kp)
{
   if (!sdp->registered_p)
      return;

   if (sdp->return_p)
      unregister_kretprobe (&kp->u.krp);
   else
      unregister_kprobe (&kp->u.kp);

#if defined(__ia64__)
   unregister_kprobe (&kp->dummy);
#endif

   sdp->registered_p = 0;
#ifdef STP_ON_THE_FLY
   sdp->enabled_p = 0;
#endif

   stapkp_add_missed(sdp, kp);
}


#if defined(STAPCONF_UNREGISTER_KPROBES)

// The actual size is set later on in
// dwarf_derived_probe_group::emit_module_decls().
static void * stap_unreg_kprobes[];

enum collect_type {
   COLLECT_KPROBES,
#if defined(__ia64__)
   COLLECT_DUMMYS,
#endif
   COLLECT_KRETPROBES
};

static size_t
stapkp_collect_registered_probes(struct stap_dwarf_probe *probes,
                                 struct stap_dwarf_kprobe *kprobes,
                                 size_t nprobes, enum collect_type type)
{
   size_t i, j;

   j = 0;
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];
      struct stap_dwarf_kprobe *kp = &kprobes[i];

      if (!sdp->registered_p)
         continue;

      if (type == COLLECT_KPROBES && !sdp->return_p)
         stap_unreg_kprobes[j++] = &kp->u.kp;
      else if (type == COLLECT_KRETPROBES && sdp->return_p)
         stap_unreg_kprobes[j++] = &kp->u.krp;
#if defined(__ia64__)
      else if (type == COLLECT_DUMMYS)
         stap_unreg_kprobes[j++] = &kp->dummy;
#endif
   }

   return j;
}

static void
stapkp_batch_unregister_probes(struct stap_dwarf_probe *probes,
                               struct stap_dwarf_kprobe *kprobes,
                               size_t nprobes)
{
   size_t i, n;

   n = stapkp_collect_registered_probes(probes, kprobes,
                                        nprobes, COLLECT_KPROBES);
   unregister_kprobes((struct kprobe **)stap_unreg_kprobes, n);

   n = stapkp_collect_registered_probes(probes, kprobes,
                                        nprobes, COLLECT_KRETPROBES);
   unregister_kretprobes((struct kretprobe **)stap_unreg_kprobes, n);

#ifdef __ia64__
   n = stapkp_collect_registered_probes(probes, kprobes,
                                        nprobes, COLLECT_DUMMYS);
   unregister_kprobes((struct kprobe **)stap_unreg_kprobes, n);
#endif

   // Now for all of those we just unregistered, we need to update registered_p
   // and account for (and possibly report) missed hits.
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];
      struct stap_dwarf_kprobe *kp = &kprobes[i];

      if (!sdp->registered_p)
         continue;

      sdp->registered_p = 0;
#ifdef STP_ON_THE_FLY
      sdp->enabled_p = 0;
#endif

      stapkp_add_missed(sdp, kp);
   }
}

#endif /* STAPCONF_UNREGISTER_KPROBES */


static void
stapkp_unregister_probes(struct stap_dwarf_probe *probes,
                         struct stap_dwarf_kprobe *kprobes,
                         size_t nprobes)
{
#if defined(STAPCONF_UNREGISTER_KPROBES)

   // Unregister using batch mode
   stapkp_batch_unregister_probes(probes, kprobes, nprobes);

#else

   // We'll have to unregister them one by one
   size_t i;
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];
      struct stap_dwarf_kprobe *kp = &kprobes[i];

      if (!sdp->registered_p)
         continue;

      stapkp_unregister_probe(sdp, kp);
   }

#endif
}


#ifdef STP_ON_THE_FLY

static void
stapkp_refresh_probe(struct stap_dwarf_probe *sdp,
                     struct stap_dwarf_kprobe *kp)
{
   if (!sdp->registered_p)
      return;

   // does it need to be enabled?
   if (!sdp->enabled_p && sdp->probe->cond_enabled) {

      if (sdp->return_p) {
         if (enable_kretprobe(&kp->u.krp) != 0) {
            unregister_kretprobe(&kp->u.krp);
            sdp->registered_p = 0;
         } else
            dbug_otf("enabling (kretprobe) pidx %zu\n", sdp->probe->index);
      } else {
         if (enable_kprobe(&kp->u.kp) != 0) {
            unregister_kprobe(&kp->u.kp);
            sdp->registered_p = 0;
         } else
            dbug_otf("enabling (kprobe) pidx %zu\n", sdp->probe->index);
      }

      if (sdp->registered_p)
         sdp->enabled_p = 1;

   // does it need to be disabled?
   } else if (sdp->enabled_p && !sdp->probe->cond_enabled) {

      if (sdp->return_p) {
         if (disable_kretprobe(&kp->u.krp) != 0) {
            unregister_kretprobe(&kp->u.krp);
            sdp->registered_p = 0;
         } else
            dbug_otf("disabling (kretprobe) pidx %zu\n", sdp->probe->index);
      } else {
         if (disable_kprobe(&kp->u.kp) != 0) {
            unregister_kprobe(&kp->u.kp);
            sdp->registered_p = 0;
         } else
            dbug_otf("disabling (kprobe) pidx %zu\n", sdp->probe->index);
      }

      sdp->enabled_p = 0;
   }
}

#endif


static int
stapkp_init(struct stap_dwarf_probe *probes,
            struct stap_dwarf_kprobe *kprobes,
            size_t nprobes)
{
   size_t i;
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];
      struct stap_dwarf_kprobe *kp = &kprobes[i];
      int rc = 0;

      rc = stapkp_register_probe(sdp, kp);
      // XXX: check if it should be disabled right away

      // NB: We keep going even if a probe failed to register (PR6749). We only
      // warn about it if it wasn't optional.
      if (rc && !sdp->optional_p) {
         _stp_warn("probe %s (address 0x%lx) registration error (rc %d)",
                   sdp->probe->pp, stapkp_relocate_addr(sdp), rc);
      }
   }

   return 0;
}


static void
stapkp_refresh(struct stap_dwarf_probe *probes,
               struct stap_dwarf_kprobe *kprobes,
               size_t nprobes)
{
   size_t i;

   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];
      struct stap_dwarf_kprobe *kp = &kprobes[i];

      unsigned long addr = stapkp_relocate_addr(sdp);

      // new module arrived?
      if (sdp->registered_p == 0 && addr != 0) {

         stapkp_register_probe(sdp, kp);
         // XXX: check if it should be disabled right away

      // old module disappeared?
      } else if (sdp->registered_p == 1 && addr == 0) {

         stapkp_unregister_probe(sdp, kp);

#ifdef STP_ON_THE_FLY

      // does it need to be enabled/disabled?
      } else if ((!sdp->enabled_p && sdp->probe->cond_enabled)
              || (sdp->enabled_p && !sdp->probe->cond_enabled)) {

         stapkp_refresh_probe(sdp, kp);

#endif
      }
   }
}


static void
stapkp_exit(struct stap_dwarf_probe *probes,
            struct stap_dwarf_kprobe *kprobes,
            size_t nprobes)
{
   stapkp_unregister_probes(probes, kprobes, nprobes);
}


#endif /* _KPROBES_C_ */
