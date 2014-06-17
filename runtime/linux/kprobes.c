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


static int
stapkp_init(struct stap_dwarf_probe *probes,
            struct stap_dwarf_kprobe *kprobes,
            size_t nprobes)
{
   int rc = 0;

   size_t i;
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];
      struct stap_dwarf_kprobe *kp = &kprobes[i];
      const char *probe_point;

      unsigned long relocated_addr
         = _stp_kmodule_relocate(sdp->module, sdp->section, sdp->address);

      if (relocated_addr == 0)
         continue; // quietly; assume module is absent

      probe_point = sdp->probe->pp; // for error messages

      if (sdp->return_p) {

         kp->u.krp.kp.addr = (void *) relocated_addr;
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

         // should it be disabled right away?
#ifdef STP_ON_THE_FLY
         if (rc == 0 && !sdp->probe->cond_enabled) {
            rc = disable_kretprobe (& kp->u.krp);
            if (rc != 0)
               unregister_kretprobe (& kp->u.krp);
            else
               dbug_otf("disabled (kretprobe) pidx %zu\n", sdp->probe->index);
         }
#endif

      } else { // !sdp->return_p

         // to ensure safeness of bspcache, always use aggr_kprobe on ia64
         kp->u.kp.addr = (void *) relocated_addr;
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

         // should it be disabled right away?
#ifdef STP_ON_THE_FLY
         if (rc == 0 && !sdp->probe->cond_enabled) {
            rc = disable_kprobe (& kp->u.kp);
            if (rc != 0)
               unregister_kprobe (& kp->u.kp);
            else
               dbug_otf("disabled (kprobe) pidx %zu\n", sdp->probe->index);
         }
#endif
      }

      if (rc) { // PR6749: tolerate a failed register_*probe.
         sdp->registered_p = 0;
         if (!sdp->optional_p)
            _stp_warn ("probe %s (address 0x%lx) registration error (rc %d)",
                       probe_point, (unsigned long) relocated_addr, rc);
         rc = 0; // continue with other probes
         // XXX: shall we increment numskipped?
      } else
         sdp->registered_p = 1;

      // the enabled_p field is now in agreement with cond_enabled (if registered)
#ifdef STP_ON_THE_FLY
      sdp->enabled_p = !sdp->registered_p ? 0 : sdp->probe->cond_enabled;
#endif

   } // for loop

   return rc;
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

      unsigned long relocated_addr
         = _stp_kmodule_relocate (sdp->module, sdp->section, sdp->address);

      int rc;

      // new module arrived?
      if (sdp->registered_p == 0 && relocated_addr != 0) {

         if (sdp->return_p) {
            kp->u.krp.kp.addr = (void *) relocated_addr;
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

#ifdef __ia64__
            // to ensure safeness of bspcache, always use aggr_kprobe on ia64
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

#ifdef STP_ON_THE_FLY
            // should it be disabled right away?
            if (rc == 0 && !sdp->probe->cond_enabled) {
               rc = disable_kretprobe (& kp->u.krp);
               if (rc != 0)
                  unregister_kretprobe (& kp->u.krp);
               else
                  dbug_otf("disabled (kretprobe) pidx %zu\n",
                           sdp->probe->index);
            }
#endif
         } else {

            kp->u.kp.addr = (void *) relocated_addr;
            kp->u.kp.pre_handler = &enter_kprobe_probe;

#ifdef __ia64__
            // to ensure safeness of bspcache, always use aggr_kprobe on ia64
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

#ifdef STP_ON_THE_FLY
            // should it be disabled right away?
            if (rc == 0 && !sdp->probe->cond_enabled) {
               rc = disable_kprobe (& kp->u.kp);
               if (rc != 0)
                  unregister_kprobe (& kp->u.kp);
               else
                  dbug_otf("disabled (kprobe) pidx %zu\n",
                           sdp->probe->index);
            }
#endif
         }

         if (rc == 0)
            sdp->registered_p = 1;

      // old module disappeared?
      } else if (sdp->registered_p == 1 && relocated_addr == 0) {

         if (sdp->return_p) {

            unregister_kretprobe (&kp->u.krp);
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

            unregister_kprobe (&kp->u.kp);
            atomic_add (kp->u.kp.nmissed, skipped_count());

#ifdef STP_TIMING
            if (kp->u.kp.nmissed)
               _stp_warn ("Skipped due to missed kprobe on '%s': %lu\n",
                          sdp->probe->pp, kp->u.kp.nmissed);
#endif

         }
#if defined(__ia64__)
         unregister_kprobe (&kp->dummy);
#endif

         sdp->registered_p = 0;

#ifdef STP_ON_THE_FLY

      // does it need to be enabled?
      } else if (!sdp->enabled_p && sdp->probe->cond_enabled) {

         if (sdp->return_p) {
            if (enable_kretprobe(&kp->u.krp) != 0)
               unregister_kretprobe(&kp->u.krp);
            else
               dbug_otf("enabling (kretprobe) pidx %zu\n", sdp->probe->index);
         } else {
            if (enable_kprobe(&kp->u.kp) != 0)
               unregister_kprobe(&kp->u.kp);
            else
               dbug_otf("enabling (kprobe) pidx %zu\n", sdp->probe->index);
         }

      // does it need to be disabled?
      } else if (sdp->enabled_p && !sdp->probe->cond_enabled) {

         if (sdp->return_p) {
            if (disable_kretprobe(&kp->u.krp) != 0)
               unregister_kretprobe(&kp->u.krp);
            else
               dbug_otf("disabling (kretprobe) pidx %zu\n", sdp->probe->index);
         } else {
            if (disable_kprobe(&kp->u.kp) != 0)
               unregister_kprobe(&kp->u.kp);
            else
               dbug_otf("disabling (kprobe) pidx %zu\n", sdp->probe->index);
         }
#endif // STP_ON_THE_FLY
      }

      // the enabled_p field is now in agreement with cond_enabled (if
      // successfully registered)
#ifdef STP_ON_THE_FLY
      sdp->enabled_p = !sdp->registered_p ? 0 : sdp->probe->cond_enabled;
#endif

   } // for loop
}

#endif /* _KPROBES_C_ */
