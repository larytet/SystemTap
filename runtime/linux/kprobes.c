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

#include <linux/kprobes.h>

#ifdef DEBUG_KPROBES
#define dbug_stapkp(args...) do {					\
		_stp_dbug(__FUNCTION__, __LINE__, args);		\
	} while (0)
#define dbug_stapkp_cond(cond, args...) do {				\
		if (cond)						\
			dbug_stapkp(args);				\
	} while (0)
#else
#define dbug_stapkp(args...) ;
#define dbug_stapkp_cond(cond, args...) ;
#endif

#ifndef KRETACTIVE
#define KRETACTIVE (max(15, 6 * (int)num_possible_cpus()))
#endif

// This shouldn't happen, but check as a precaution. If we're on kver >= 2.6.30,
// then we must also have STP_ON_THE_FLY_TIMER_ENABLE (which is turned on for
// kver >= 2.6.17, see translate_pass()). This indicates that the background
// timer is available and thus that kprobes can be armed/disarmed on-the-fly.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30) \
      && !defined(STP_ON_THE_FLY_TIMER_ENABLE)
#error "STP_ON_THE_FLY_TIMER_ENABLE undefined"
#endif

// NB: this struct is set up by the stapkp_prepare_* functions prior to
// registering and zero'ed out again after each unregister
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
   struct stap_dwarf_kprobe * const kprobe;
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
stapkp_prepare_kprobe(struct stap_dwarf_probe *sdp)
{
   struct kprobe *kp = &sdp->kprobe->u.kp;
   unsigned long addr = stapkp_relocate_addr(sdp);
   if (addr == 0)
      return 1;

   kp->addr = (void *) addr;
   kp->pre_handler = &enter_kprobe_probe;

#ifdef __ia64__ // PR6028
   sdp->kprobe->dummy.addr = kp->addr;
   sdp->kprobe->dummy.pre_handler = NULL;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
   if (!sdp->probe->cond_enabled) {
      kp->flags |= KPROBE_FLAG_DISABLED;
      dbug_otf("registering as disabled (kprobe) pidx %zu\n",
               sdp->probe->index);
   }
#endif

   return 0;
}


static int
stapkp_arch_register_kprobe(struct stap_dwarf_probe *sdp)
{
   int ret = 0;
   struct kprobe *kp = &sdp->kprobe->u.kp;

#ifndef __ia64__
   ret = register_kprobe(kp);
   dbug_stapkp_cond(ret == 0, "+kprobe %p\n", kp->addr);
#else // PR6028
   ret = register_kprobe(&sdp->kprobe->dummy);
   if (ret == 0) {
      ret = register_kprobe(kp);
      if (ret != 0)
         unregister_kprobe(&sdp->kprobe->dummy);
   }
   dbug_stapkp_cond(ret == 0, "+kprobe %p\n", sdp->kprobe->dummy.addr);
   dbug_stapkp_cond(ret == 0, "+kprobe %p\n", kp->addr);
#endif

   sdp->registered_p = (ret ? 0 : 1);

   return ret;
}


static int
stapkp_register_kprobe(struct stap_dwarf_probe *sdp)
{
   int ret = stapkp_prepare_kprobe(sdp);
   if (ret == 0)
      ret = stapkp_arch_register_kprobe(sdp);
   return ret;
}


static int
stapkp_prepare_kretprobe(struct stap_dwarf_probe *sdp)
{
   struct kretprobe *krp = &sdp->kprobe->u.krp;
   unsigned long addr = stapkp_relocate_addr(sdp);
   if (addr == 0)
      return 1;

   krp->kp.addr = (void *) addr;

   if (sdp->maxactive_p)
      krp->maxactive = sdp->maxactive_val;
   else
      krp->maxactive = KRETACTIVE;

   krp->handler = &enter_kretprobe_probe;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
   if (sdp->entry_probe) {
      krp->entry_handler = &enter_kretprobe_entry_probe;
      krp->data_size = sdp->saved_longs * sizeof(int64_t) +
                       sdp->saved_strings * MAXSTRINGLEN;
   }
#endif

#ifdef __ia64__ // PR6028
   sdp->kprobe->dummy.addr = krp->kp.addr;
   sdp->kprobe->dummy.pre_handler = NULL;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
   if (!sdp->probe->cond_enabled) {
      krp->kp.flags |= KPROBE_FLAG_DISABLED;
      dbug_otf("registering as disabled (kretprobe) pidx %zu\n",
               sdp->probe->index);
   }
#endif

   return 0;
}


static int
stapkp_arch_register_kretprobe(struct stap_dwarf_probe *sdp)
{
   int ret = 0;
   struct kretprobe *krp = &sdp->kprobe->u.krp;

#ifndef __ia64__
   ret = register_kretprobe(krp);
   dbug_stapkp_cond(ret == 0, "+kretprobe %p\n", krp->kp.addr);
#else // PR6028
   ret = register_kprobe(&sdp->kprobe->dummy);
   if (ret == 0) {
      ret = register_kretprobe(krp);
      if (ret != 0)
         unregister_kprobe(&sdp->kprobe->dummy);
   }
   dbug_stapkp_cond(ret == 0, "+kprobe %p\n", sdp->kprobe->dummy.addr);
   dbug_stapkp_cond(ret == 0, "+kretprobe %p\n", krp->kp.addr);
#endif

   sdp->registered_p = (ret ? 0 : 1);

   return ret;
}


static int
stapkp_register_kretprobe(struct stap_dwarf_probe *sdp)
{
   int ret = stapkp_prepare_kretprobe(sdp);
   if (ret == 0)
      ret = stapkp_arch_register_kretprobe(sdp);
   return ret;
}


static int
stapkp_register_probe(struct stap_dwarf_probe *sdp)
{
   if (sdp->registered_p)
      return 0;

   return sdp->return_p ? stapkp_register_kretprobe(sdp)
                        : stapkp_register_kprobe(sdp);
}


static void
stapkp_add_missed(struct stap_dwarf_probe *sdp)
{
   if (sdp->return_p) {

      struct kretprobe *krp = &sdp->kprobe->u.krp;

      atomic_add(krp->nmissed, skipped_count());
#ifdef STP_TIMING
      if (krp->nmissed)
         _stp_warn ("Skipped due to missed kretprobe/1 on '%s': %d\n",
                    sdp->probe->pp, krp->nmissed);
#endif

      atomic_add(krp->kp.nmissed, skipped_count());
#ifdef STP_TIMING
      if (krp->kp.nmissed)
         _stp_warn ("Skipped due to missed kretprobe/2 on '%s': %lu\n",
                    sdp->probe->pp, krp->kp.nmissed);
#endif

   } else {

      struct kprobe *kp = &sdp->kprobe->u.kp;

      atomic_add (kp->nmissed, skipped_count());
#ifdef STP_TIMING
      if (kp->nmissed)
         _stp_warn ("Skipped due to missed kprobe on '%s': %lu\n",
                    sdp->probe->pp, kp->nmissed);
#endif
   }
}


static void
stapkp_unregister_probe(struct stap_dwarf_probe *sdp)
{
   struct stap_dwarf_kprobe *sdk = sdp->kprobe;

   if (!sdp->registered_p)
      return;

   if (sdp->return_p) {
      unregister_kretprobe (&sdk->u.krp);
      dbug_stapkp("-kretprobe %p\n", sdk->u.krp.kp.addr);
   } else {
      unregister_kprobe (&sdk->u.kp);
      dbug_stapkp("-kprobe %p\n", sdk->u.kp.addr);
   }

#if defined(__ia64__)
   unregister_kprobe (&sdk->dummy);
   dbug_stapkp("-kprobe %p\n", sdk->dummy.addr);
#endif

   sdp->registered_p = 0;

   stapkp_add_missed(sdp);

   // PR16861: kprobes may have left some things in the k[ret]probe struct.
   // Let's reset it to be sure it's safe for re-use.
   memset(sdk, 0, sizeof(struct stap_dwarf_kprobe));
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
                                 size_t nprobes, enum collect_type type)
{
   size_t i, j;

   j = 0;
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];
      struct stap_dwarf_kprobe *sdk = sdp->kprobe;

      if (!sdp->registered_p)
         continue;

      if (type == COLLECT_KPROBES && !sdp->return_p)
         stap_unreg_kprobes[j++] = &sdk->u.kp;
      else if (type == COLLECT_KRETPROBES && sdp->return_p)
         stap_unreg_kprobes[j++] = &sdk->u.krp;
#if defined(__ia64__)
      else if (type == COLLECT_DUMMYS)
         stap_unreg_kprobes[j++] = &sdk->dummy;
#endif
   }

   return j;
}

static void
stapkp_batch_unregister_probes(struct stap_dwarf_probe *probes,
                               size_t nprobes)
{
   size_t i, n;

   n = stapkp_collect_registered_probes(probes,
                                        nprobes, COLLECT_KPROBES);
   unregister_kprobes((struct kprobe **)stap_unreg_kprobes, n);
   dbug_stapkp_cond(n > 0, "-kprobe * %zd\n", n);

   n = stapkp_collect_registered_probes(probes,
                                        nprobes, COLLECT_KRETPROBES);
   unregister_kretprobes((struct kretprobe **)stap_unreg_kprobes, n);
   dbug_stapkp_cond(n > 0, "-kretprobe * %zd\n", n);

#ifdef __ia64__
   n = stapkp_collect_registered_probes(probes,
                                        nprobes, COLLECT_DUMMYS);
   unregister_kprobes((struct kprobe **)stap_unreg_kprobes, n);
   dbug_stapkp_cond(n > 0, "-kprobe * %zd\n", n);
#endif

   // Now for all of those we just unregistered, we need to update registered_p
   // and account for (and possibly report) missed hits.
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];

      if (!sdp->registered_p)
         continue;

      sdp->registered_p = 0;

      stapkp_add_missed(sdp);

      // PR16861: kprobes may have left some things in the k[ret]probe struct.
      // Let's reset it to be sure it's safe for re-use.
      memset(sdp->kprobe, 0, sizeof(struct stap_dwarf_kprobe));
   }
}

#endif /* STAPCONF_UNREGISTER_KPROBES */


static void
stapkp_unregister_probes(struct stap_dwarf_probe *probes,
                         size_t nprobes)
{
#if defined(STAPCONF_UNREGISTER_KPROBES)

   // Unregister using batch mode
   stapkp_batch_unregister_probes(probes, nprobes);

#else

   // We'll have to unregister them one by one
   size_t i;
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];

      if (!sdp->registered_p)
         continue;

      stapkp_unregister_probe(sdp);
   }

#endif
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)

static int
stapkp_enabled(struct stap_dwarf_probe *sdp)
{
   if (!sdp->registered_p)
      return 0;

   return sdp->return_p ? !kprobe_disabled(&sdp->kprobe->u.krp.kp)
                        : !kprobe_disabled(&sdp->kprobe->u.kp);
}


static int
stapkp_should_enable_probe(struct stap_dwarf_probe *sdp)
{
   return  sdp->registered_p
       && !stapkp_enabled(sdp)
       &&  sdp->probe->cond_enabled;
}


static int
stapkp_enable_probe(struct stap_dwarf_probe *sdp)
{
   int ret = 0;

   dbug_otf("enabling (k%sprobe) pidx %zu\n",
            sdp->return_p ? "ret" : "", sdp->probe->index);

   ret = sdp->return_p ? enable_kretprobe(&sdp->kprobe->u.krp)
                       : enable_kprobe(&sdp->kprobe->u.kp);

   if (ret != 0) {
      stapkp_unregister_probe(sdp);
      dbug_otf("failed to enable (k%sprobe) pidx %zu (rc %d)\n",
               sdp->return_p ? "ret" : "", sdp->probe->index, ret);
   }

   return ret;
}


static int
stapkp_should_disable_probe(struct stap_dwarf_probe *sdp)
{
   return  sdp->registered_p
       &&  stapkp_enabled(sdp)
       && !sdp->probe->cond_enabled;
}


static int
stapkp_disable_probe(struct stap_dwarf_probe *sdp)
{
   int ret = 0;

   dbug_otf("disabling (k%sprobe) pidx %zu\n",
            sdp->return_p ? "ret" : "", sdp->probe->index);

   ret = sdp->return_p ? disable_kretprobe(&sdp->kprobe->u.krp)
                       : disable_kprobe(&sdp->kprobe->u.kp);

   if (ret != 0) {
      stapkp_unregister_probe(sdp);
      dbug_otf("failed to disable (k%sprobe) pidx %zu (rc %d)\n",
               sdp->return_p ? "ret" : "", sdp->probe->index, ret);
   }

   return ret;
}


static int
stapkp_refresh_probe(struct stap_dwarf_probe *sdp)
{
   if (stapkp_should_enable_probe(sdp))
      return stapkp_enable_probe(sdp);
   if (stapkp_should_disable_probe(sdp))
      return stapkp_disable_probe(sdp);
   return 0;
}

#endif /* LINUX_VERSION_CODE >= 2.6.30 */


static int
stapkp_init(struct stap_dwarf_probe *probes,
            size_t nprobes)
{
   size_t i;
   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];
      int rc = 0;

      rc = stapkp_register_probe(sdp);
      if (rc == 1) // failed to relocate addr?
         continue; // don't fuss about it, module probably not loaded

      // NB: We keep going even if a probe failed to register (PR6749). We only
      // warn about it if it wasn't optional.
      if (rc && !sdp->optional_p) {
         _stp_warn("probe %s (address 0x%lx) registration error (rc %d)",
                   sdp->probe->pp, stapkp_relocate_addr(sdp), rc);
      }
   }

   return 0;
}


/* stapkp_refresh is called for two reasons: either a kprobe needs to be
 * enabled/disabled (modname is NULL), or a module has been loaded/unloaded and
 * kprobes need to be registered/unregistered (modname is !NULL). */
static void
stapkp_refresh(const char *modname,
               struct stap_dwarf_probe *probes,
               size_t nprobes)
{
   size_t i;

   for (i = 0; i < nprobes; i++) {

      struct stap_dwarf_probe *sdp = &probes[i];

      // was this probe's target module loaded/unloaded
      if (modname && sdp->module
            && strcmp(modname, sdp->module) == 0) {
         int rc;
         unsigned long addr = stapkp_relocate_addr(sdp);

         // module being loaded?
         if (sdp->registered_p == 0 && addr != 0)
            stapkp_register_probe(sdp);
         // module/section being unloaded?
         else if (sdp->registered_p == 1 && addr == 0)
            stapkp_unregister_probe(sdp);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
      } else if (stapkp_should_enable_probe(sdp)
              || stapkp_should_disable_probe(sdp)) {
         stapkp_refresh_probe(sdp);
#endif
      }
   }
}


static void
stapkp_exit(struct stap_dwarf_probe *probes,
            size_t nprobes)
{
   stapkp_unregister_probes(probes, nprobes);
}


#endif /* _KPROBES_C_ */
