// C++ interface to dwfl
// Copyright (C) 2005-2015 Red Hat Inc.
// Copyright (C) 2005-2007 Intel Corporation.
// Copyright (C) 2008 James.Bottomley@HansenPartnership.com
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "dwflpp.h"
#include "config.h"
#include <cxxabi.h>
#include "staptree.h"
#include "elaborate.h"
#include "tapsets.h"
#include "task_finder.h"
#include "translate.h"
#include "session.h"
#include "util.h"
#include "buildrun.h"
#include "dwarf_wrappers.h"
#include "auto_free.h"
#include "hash.h"
#include "rpm_finder.h"
#include "setupdwfl.h"

#include <cstdlib>
#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cstdarg>
#include <cassert>
#include <iomanip>
#include <cerrno>

extern "C" {
#include <fcntl.h>
#include <elfutils/libdwfl.h>
#include <elfutils/libdw.h>
#include <dwarf.h>
#include <elf.h>
#include <obstack.h>
#include <regex.h>
#include <glob.h>
#include <fnmatch.h>
#include <stdio.h>
#include <sys/types.h>

#include "loc2c.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
}

// Older glibc elf.h don't know about this new constant.
#ifndef STB_GNU_UNIQUE
#define STB_GNU_UNIQUE  10
#endif


// debug flag to compare to the uncached version from libdw
// #define DEBUG_DWFLPP_GETSCOPES 1


using namespace std;
using namespace __gnu_cxx;


static string TOK_KERNEL("kernel");


// RAII style tracker for obstack pool, used because of complex exception flows
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
struct obstack_tracker
{
  struct obstack* p;
  obstack_tracker(struct obstack*p): p(p) {
    obstack_init (p);
  }
  ~obstack_tracker() {
    obstack_free (this->p, 0);
  }
};


dwflpp::dwflpp(systemtap_session & session, const string& name, bool kernel_p):
  sess(session), module(NULL), module_bias(0), mod_info(NULL),
  module_start(0), module_end(0), cu(NULL), dwfl(NULL),
  module_dwarf(NULL), function(NULL), blacklist_func(), blacklist_func_ret(),
  blacklist_file(),  blacklist_enabled(false)
{
  if (kernel_p)
    setup_kernel(name, session);
  else
    {
      vector<string> modules;
      modules.push_back(name);
      setup_user(modules);
    }
}

dwflpp::dwflpp(systemtap_session & session, const vector<string>& names,
	       bool kernel_p):
  sess(session), module(NULL), module_bias(0), mod_info(NULL),
  module_start(0), module_end(0), cu(NULL), dwfl(NULL),
  module_dwarf(NULL), function(NULL), blacklist_enabled(false)
{
  if (kernel_p)
    setup_kernel(names);
  else
    setup_user(names);
}

dwflpp::~dwflpp()
{
  delete_map(module_cu_cache);
  delete_map(cu_function_cache);
  delete_map(mod_function_cache);
  delete_map(cu_inl_function_cache);
  delete_map(global_alias_cache);
  delete_map(cu_die_parent_cache);

  cu_lines_cache_t::iterator i;
  for (i = cu_lines_cache.begin(); i != cu_lines_cache.end(); ++i)
    delete_map(*i->second);
  delete_map(cu_lines_cache);

  if (dwfl)
    dwfl_end(dwfl);
  // NB: don't "delete mod_info;", as that may be shared
  // between dwflpp instances, and are stored in
  // session.module_cache[] anyway.
}


module_cache::~module_cache ()
{
  delete_map(cache);
}


void
dwflpp::get_module_dwarf(bool required, bool report)
{
  module_dwarf = dwfl_module_getdwarf(module, &module_bias);
  mod_info->dwarf_status = (module_dwarf ? info_present : info_absent);
  if (!module_dwarf && report)
    {
      string msg = _("cannot find ");
      if (module_name == "")
        msg += "kernel";
      else
        msg += string("module ") + module_name;
      msg += " debuginfo";

      int i = dwfl_errno();
      if (i)
        msg += string(": ") + dwfl_errmsg (i);

      msg += " [man warning::debuginfo]";

      /* add module_name to list to find rpm */
      find_debug_rpms(sess, module_name.c_str());

      if (required)
        throw SEMANTIC_ERROR (msg);
      else
        sess.print_warning(msg);
    }
}


void
dwflpp::focus_on_module(Dwfl_Module * m, module_info * mi)
{
  module = m;
  mod_info = mi;
  if (m)
    {
      module_name = dwfl_module_info(module, NULL, &module_start, &module_end,
                                     NULL, NULL, NULL, NULL) ?: "module";
    }
  else
    {
      assert(mi && mi->name && mi->name == TOK_KERNEL);
      module_name = mi->name;
      module_start = 0;
      module_end = 0;
      module_bias = mi->bias;
    }

  // Reset existing pointers and names

  module_dwarf = NULL;

  cu = NULL;

  function_name.clear();
  function = NULL;
}


void
dwflpp::focus_on_cu(Dwarf_Die * c)
{
  assert(c);
  assert(module);

  cu = c;

  // Reset existing pointers and names
  function_name.clear();
  function = NULL;
}


string
dwflpp::cu_name(void)
{
  return dwarf_diename(cu) ?: "<unknown source>";
}


void
dwflpp::focus_on_function(Dwarf_Die * f)
{
  assert(f);
  assert(module);
  assert(cu);

  function = f;
  function_name = dwarf_diename(function) ?: "function";
}


/* Return the Dwarf_Die for the given address in the current module.
 * The address should be in the module address address space (this
 * function will take care of any dw bias).
 */
Dwarf_Die *
dwflpp::query_cu_containing_address(Dwarf_Addr a)
{
  Dwarf_Addr bias;
  assert(dwfl);
  assert(module);
  get_module_dwarf();

  Dwarf_Die* cudie = dwfl_module_addrdie(module, a, &bias);
  assert(bias == module_bias);
  return cudie;
}


bool
dwflpp::module_name_matches(const string& pattern)
{
  bool t = (fnmatch(pattern.c_str(), module_name.c_str(), 0) == 0);
  if (t && sess.verbose>3)
    clog << _F("pattern '%s' matches module '%s'\n",
               pattern.c_str(), module_name.c_str());
  if (!t && sess.verbose>4)
    clog << _F("pattern '%s' does not match module '%s'\n",
               pattern.c_str(), module_name.c_str());

  return t;
}


bool
dwflpp::name_has_wildcard (const string& pattern)
{
  return (pattern.find('*') != string::npos ||
          pattern.find('?') != string::npos ||
          pattern.find('[') != string::npos);
}


bool
dwflpp::module_name_final_match(const string& pattern)
{
  // Assume module_name_matches().  Can there be any more matches?
  // Not unless the pattern is a wildcard, since module names are
  // presumed unique.
  return !name_has_wildcard(pattern);
}


bool
dwflpp::function_name_matches_pattern(const string& name, const string& pattern)
{
  bool t = (fnmatch(pattern.c_str(), name.c_str(), 0) == 0);
  if (t && sess.verbose>3)
    clog << _F("pattern '%s' matches function '%s'\n", pattern.c_str(), name.c_str());
  return t;
}


bool
dwflpp::function_name_matches(const string& pattern)
{
  assert(function);
  return function_name_matches_pattern(function_name, pattern);
}


bool
dwflpp::function_scope_matches(const vector<string>& scopes)
{
  // walk up the containing scopes
  Dwarf_Die* die = function;
  for (int i = scopes.size() - 1; i >= 0; --i)
    {
      die = get_parent_scope(die);

      // check if this scope matches, and prepend it if so
      // NB: a NULL die is the global scope, compared as ""
      string name = dwarf_diename(die) ?: "";
      if (name_has_wildcard(scopes[i]) ?
          function_name_matches_pattern(name, scopes[i]) :
          name == scopes[i])
        function_name = name + "::" + function_name;
      else
        return false;

      // make sure there's no more if we're at the global scope
      if (!die && i > 0)
        return false;
    }
  return true;
}


void
dwflpp::setup_kernel(const string& name, systemtap_session & s, bool debuginfo_needed)
{
  if (! sess.module_cache)
    sess.module_cache = new module_cache ();

  unsigned offline_search_matches = 0;
  dwfl = setup_dwfl_kernel(name, &offline_search_matches, sess);

  if (offline_search_matches < 1)
    {
      if (debuginfo_needed) {
        // Suggest a likely kernel dir to find debuginfo rpm for
        string dir = string(sess.sysroot + "/lib/modules/" + sess.kernel_release );
        find_debug_rpms(sess, dir.c_str());
      }
      throw SEMANTIC_ERROR (_F("missing %s kernel/module debuginfo [man warning::debuginfo] under '%s'",
                                sess.architecture.c_str(), sess.kernel_build_tree.c_str()));
    }

  if (dwfl != NULL)
    {
      ptrdiff_t off = 0;
      do
        {
          assert_no_interrupts();
          off = dwfl_getmodules (dwfl, &add_module_build_id_to_hash, &s, off);
        }
      while (off > 0);
      DWFL_ASSERT("dwfl_getmodules", off == 0);
    }

  build_kernel_blacklist();
}

void
dwflpp::setup_kernel(const vector<string> &names, bool debuginfo_needed)
{
  if (! sess.module_cache)
    sess.module_cache = new module_cache ();

  unsigned offline_search_matches = 0;
  set<string> offline_search_names(names.begin(), names.end());
  dwfl = setup_dwfl_kernel(offline_search_names,
			   &offline_search_matches,
			   sess);

  if (offline_search_matches < offline_search_names.size())
    {
      if (debuginfo_needed) {
        // Suggest a likely kernel dir to find debuginfo rpm for
        string dir = string(sess.sysroot + "/lib/modules/" + sess.kernel_release );
        find_debug_rpms(sess, dir.c_str());
      }
      throw SEMANTIC_ERROR (_F("missing %s kernel/module debuginfo [man warning::debuginfo] under '%s'",
                               sess.architecture.c_str(), sess.kernel_build_tree.c_str()));
    }

  build_kernel_blacklist();
}


void
dwflpp::setup_user(const vector<string>& modules, bool debuginfo_needed)
{
  if (! sess.module_cache)
    sess.module_cache = new module_cache ();

  vector<string>::const_iterator it = modules.begin();
  dwfl = setup_dwfl_user(it, modules.end(), debuginfo_needed, sess);
  if (debuginfo_needed && it != modules.end())
    DWFL_ASSERT (string(_F("missing process %s %s debuginfo",
                           (*it).c_str(), sess.architecture.c_str())),
                           dwfl);

  build_user_blacklist();
}

template<> void
dwflpp::iterate_over_modules<void>(int (*callback)(Dwfl_Module*,
                                                   void**,
                                                   const char*,
                                                   Dwarf_Addr,
                                                   void*),
                                   void *data)
{
  dwfl_getmodules (dwfl, callback, data, 0);

  // Don't complain if we exited dwfl_getmodules early.
  // This could be a $target variable error that will be
  // reported soon anyway.
  // DWFL_ASSERT("dwfl_getmodules", off == 0);

  // PR6864 XXX: For dwarfless case (if .../vmlinux is missing), then the
  // "kernel" module is not reported in the loop above.  However, we
  // may be able to make do with symbol table data.
}


template<> void
dwflpp::iterate_over_cus<void>(int (*callback)(Dwarf_Die*, void*),
                               void *data,
                               bool want_types)
{
  get_module_dwarf(false);
  Dwarf *dw = module_dwarf;
  if (!dw) return;

  vector<Dwarf_Die>* v = module_cu_cache[dw];
  if (v == 0)
    {
      v = new vector<Dwarf_Die>;
      module_cu_cache[dw] = v;

      Dwarf_Off off = 0;
      size_t cuhl;
      Dwarf_Off noff;
      while (dwarf_nextcu (dw, off, &noff, &cuhl, NULL, NULL, NULL) == 0)
        {
          assert_no_interrupts();
          Dwarf_Die die_mem;
          Dwarf_Die *die;
          die = dwarf_offdie (dw, off + cuhl, &die_mem);
          /* Skip partial units. */
          if (dwarf_tag (die) == DW_TAG_compile_unit)
            v->push_back (*die); /* copy */
          off = noff;
        }
    }

  if (want_types && module_tus_read.find(dw) == module_tus_read.end())
    {
      // Process type units.
      Dwarf_Off off = 0;
      size_t cuhl;
      Dwarf_Off noff;
      uint64_t type_signature;
      while (dwarf_next_unit (dw, off, &noff, &cuhl, NULL, NULL, NULL, NULL,
			      &type_signature, NULL) == 0)
	{
          assert_no_interrupts();
          Dwarf_Die die_mem;
          Dwarf_Die *die;
          die = dwarf_offdie_types (dw, off + cuhl, &die_mem);
          /* Skip partial units. */
          if (dwarf_tag (die) == DW_TAG_type_unit)
            v->push_back (*die); /* copy */
          off = noff;
	}
      module_tus_read.insert(dw);
    }

  for (vector<Dwarf_Die>::iterator i = v->begin(); i != v->end(); ++i)
    {
      int rc = (*callback)(&*i, data);
      assert_no_interrupts();
      if (rc != DWARF_CB_OK)
        break;
    }
}


bool
dwflpp::func_is_inline()
{
  assert (function);
  return dwarf_func_inline (function) != 0;
}


bool
dwflpp::func_is_exported()
{
  const char *name = dwarf_linkage_name (function) ?: dwarf_diename (function);

  assert (function);

  int syms = dwfl_module_getsymtab (module);
  DWFL_ASSERT (_("Getting symbols"), syms >= 0);

  for (int i = 0; i < syms; i++)
    {
      GElf_Sym sym;
      GElf_Word shndxp;
      const char *symname = dwfl_module_getsym(module, i, &sym, &shndxp);
      if (symname
	  && strcmp (name, symname) == 0)
	{
	  if (GELF_ST_TYPE(sym.st_info) == STT_FUNC
	      && (GELF_ST_BIND(sym.st_info) == STB_GLOBAL
		  || GELF_ST_BIND(sym.st_info) == STB_WEAK
		  || GELF_ST_BIND(sym.st_info) == STB_GNU_UNIQUE))
	    return true;
	  else
	    return false;
	}
    }
  return false;
}

void
dwflpp::cache_inline_instances (Dwarf_Die* die)
{
  // If this is an inline instance, link it back to its origin
  Dwarf_Die origin;
  if (dwarf_tag(die) == DW_TAG_inlined_subroutine &&
      dwarf_attr_die(die, DW_AT_abstract_origin, &origin))
    {
      vector<Dwarf_Die>*& v = cu_inl_function_cache[origin.addr];
      if (!v)
        v = new vector<Dwarf_Die>;
      v->push_back(*die);
    }

  // Recurse through other scopes that may contain inlines
  Dwarf_Die child, import;
  if (dwarf_child(die, &child) == 0)
    do
      {
        switch (dwarf_tag (&child))
          {
          // tags that could contain inlines
          case DW_TAG_compile_unit:
          case DW_TAG_module:
          case DW_TAG_lexical_block:
          case DW_TAG_with_stmt:
          case DW_TAG_catch_block:
          case DW_TAG_try_block:
          case DW_TAG_entry_point:
          case DW_TAG_inlined_subroutine:
          case DW_TAG_subprogram:
            cache_inline_instances(&child);
            break;

          // imported dies should be followed
          case DW_TAG_imported_unit:
            if (dwarf_attr_die(&child, DW_AT_import, &import))
              cache_inline_instances(&import);
            break;

          // nothing to do for other tags
          default:
            break;
          }
      }
    while (dwarf_siblingof(&child, &child) == 0);
}


template<> void
dwflpp::iterate_over_inline_instances<void>(int (*callback)(Dwarf_Die*, void*),
                                            void *data)
{
  assert (function);
  assert (func_is_inline ());

  if (cu_inl_function_cache_done.insert(cu->addr).second)
    cache_inline_instances(cu);

  vector<Dwarf_Die>* v = cu_inl_function_cache[function->addr];
  if (!v)
    return;

  for (vector<Dwarf_Die>::iterator i = v->begin(); i != v->end(); ++i)
    {
      int rc = (*callback)(&*i, data);
      assert_no_interrupts();
      if (rc != DWARF_CB_OK)
        break;
    }
}


void
dwflpp::cache_die_parents(cu_die_parent_cache_t* parents, Dwarf_Die* die)
{
  // Record and recurse through DIEs we care about
  Dwarf_Die child, import;
  if (dwarf_child(die, &child) == 0)
    do
      {
        switch (dwarf_tag (&child))
          {
          // normal tags to recurse
          case DW_TAG_compile_unit:
          case DW_TAG_module:
          case DW_TAG_lexical_block:
          case DW_TAG_with_stmt:
          case DW_TAG_catch_block:
          case DW_TAG_try_block:
          case DW_TAG_entry_point:
          case DW_TAG_inlined_subroutine:
          case DW_TAG_subprogram:
          case DW_TAG_namespace:
          case DW_TAG_class_type:
          case DW_TAG_structure_type:
            parents->insert(make_pair(child.addr, *die));
            cache_die_parents(parents, &child);
            break;

          // record only, nothing to recurse
          case DW_TAG_label:
            parents->insert(make_pair(child.addr, *die));
            break;

          // imported dies should be followed
          case DW_TAG_imported_unit:
            if (dwarf_attr_die(&child, DW_AT_import, &import))
              {
                parents->insert(make_pair(import.addr, *die));
                cache_die_parents(parents, &import);
              }
            break;

          // nothing to do for other tags
          default:
            break;
          }
      }
    while (dwarf_siblingof(&child, &child) == 0);
}


cu_die_parent_cache_t*
dwflpp::get_die_parents()
{
  assert (cu);

  cu_die_parent_cache_t *& parents = cu_die_parent_cache[cu->addr];
  if (!parents)
    {
      parents = new cu_die_parent_cache_t;
      cache_die_parents(parents, cu);
      if (sess.verbose > 4)
        clog << _F("die parent cache %s:%s size %zu", module_name.c_str(),
                   cu_name().c_str(), parents->size()) << endl;
    }
  return parents;
}


vector<Dwarf_Die>
dwflpp::getscopes_die(Dwarf_Die* die)
{
  cu_die_parent_cache_t *parents = get_die_parents();

  vector<Dwarf_Die> scopes;
  Dwarf_Die *scope = die;
  cu_die_parent_cache_t::iterator it;
  do
    {
      scopes.push_back(*scope);
      it = parents->find(scope->addr);
      scope = &it->second;
    }
  while (it != parents->end());

#ifdef DEBUG_DWFLPP_GETSCOPES
  Dwarf_Die *dscopes = NULL;
  int nscopes = dwarf_getscopes_die(die, &dscopes);

  assert(nscopes == (int)scopes.size());
  for (unsigned i = 0; i < scopes.size(); ++i)
    assert(scopes[i].addr == dscopes[i].addr);
  free(dscopes);
#endif

  return scopes;
}


std::vector<Dwarf_Die>
dwflpp::getscopes(Dwarf_Die* die)
{
  cu_die_parent_cache_t *parents = get_die_parents();

  vector<Dwarf_Die> scopes;

  Dwarf_Die origin;
  Dwarf_Die *scope = die;
  cu_die_parent_cache_t::iterator it;
  do
    {
      scopes.push_back(*scope);
      if (dwarf_tag(scope) == DW_TAG_inlined_subroutine &&
          dwarf_attr_die(scope, DW_AT_abstract_origin, &origin))
        scope = &origin;

      it = parents->find(scope->addr);
      scope = &it->second;
    }
  while (it != parents->end());

#ifdef DEBUG_DWFLPP_GETSCOPES
  // there isn't an exact libdw equivalent, but if dwarf_getscopes on the
  // entrypc returns the same first die, then all the scopes should match
  Dwarf_Addr pc;
  if (die_entrypc(die, &pc))
    {
      Dwarf_Die *dscopes = NULL;
      int nscopes = dwarf_getscopes(cu, pc, &dscopes);
      if (nscopes > 0 && dscopes[0].addr == die->addr)
        {
          assert(nscopes == (int)scopes.size());
          for (unsigned i = 0; i < scopes.size(); ++i)
            assert(scopes[i].addr == dscopes[i].addr);
        }
      free(dscopes);
    }
#endif

  return scopes;
}


std::vector<Dwarf_Die>
dwflpp::getscopes(Dwarf_Addr pc)
{
  // The die_parent_cache doesn't help us without knowing where the pc is
  // contained, so we have to do this one the old fashioned way.

  assert (cu);

  vector<Dwarf_Die> scopes;

  Dwarf_Die* dwarf_scopes;
  int nscopes = dwarf_getscopes(cu, pc, &dwarf_scopes);
  if (nscopes > 0)
    {
      scopes.assign(dwarf_scopes, dwarf_scopes + nscopes);
      free(dwarf_scopes);
    }

#ifdef DEBUG_DWFLPP_GETSCOPES
  // check that getscopes on the starting die gets the same result
  if (!scopes.empty())
    {
      vector<Dwarf_Die> other = getscopes(&scopes[0]);
      assert(scopes.size() == other.size());
      for (unsigned i = 0; i < scopes.size(); ++i)
        assert(scopes[i].addr == other[i].addr);
    }
#endif

  return scopes;
}


Dwarf_Die*
dwflpp::get_parent_scope(Dwarf_Die* die)
{
  Dwarf_Die specification;
  if (dwarf_attr_die(die, DW_AT_specification, &specification))
    die = &specification;

  cu_die_parent_cache_t *parents = get_die_parents();
  cu_die_parent_cache_t::iterator it = parents->find(die->addr);
  while (it != parents->end())
    {
      Dwarf_Die* scope = &it->second;
      switch (dwarf_tag (scope))
        {
        case DW_TAG_namespace:
        case DW_TAG_class_type:
        case DW_TAG_structure_type:
          return scope;

        default:
          break;
        }
      it = parents->find(scope->addr);
    }
  return NULL;
}

static const char*
cache_type_prefix(Dwarf_Die* type)
{
  switch (dwarf_tag(type))
    {
    case DW_TAG_enumeration_type:
      return "enum ";
    case DW_TAG_structure_type:
    case DW_TAG_class_type:
      // treating struct/class as equals
      return "struct ";
    case DW_TAG_union_type:
      return "union ";
    }
  return "";
}

/* GCC might generate a struct/class without DW_AT_declaration,
   but that only contains members which have DW_AT_declaration
   set.  We aren't interested in those.  PR14434 (GCC bug #54181).  */
static bool
has_only_decl_members (Dwarf_Die *die)
{
  Dwarf_Die child, import;
  if (dwarf_child(die, &child) != 0)
    return false; /* no members */

  do
    {
      if (! dwarf_hasattr(&child, DW_AT_declaration))
	return false; /* real member found.  */
      int tag = dwarf_tag(&child);
      if ((tag == DW_TAG_namespace
           || tag == DW_TAG_structure_type
           || tag == DW_TAG_class_type)
          && ! has_only_decl_members (&child))
	return false; /* real grand child member found.  */

      // Unlikely to ever happen, but if there is an imported unit
      // then check its children as if they are children of this DIE.
      if (tag == DW_TAG_imported_unit
	  && dwarf_attr_die(&child, DW_AT_import, &import)
	  && ! has_only_decl_members (&import))
	return false;
    }
  while (dwarf_siblingof(&child, &child) == 0);

  return true; /* Tried all children and grandchildren. */
}

int
dwflpp::global_alias_caching_callback(Dwarf_Die *die, bool has_inner_types,
                                      const string& prefix, cu_type_cache_t *cache)
{
  const char *name = dwarf_diename(die);

  if (!name || dwarf_hasattr(die, DW_AT_declaration)
      || has_only_decl_members(die))
    return DWARF_CB_OK;

  int tag = dwarf_tag(die);
  if (has_inner_types && (tag == DW_TAG_namespace
                          || tag == DW_TAG_structure_type
                          || tag == DW_TAG_class_type))
    iterate_over_types(die, has_inner_types, prefix + name + "::",
                       global_alias_caching_callback, cache);

  if (tag != DW_TAG_namespace)
    {
      string type_name = prefix + cache_type_prefix(die) + name;
      if (cache->find(type_name) == cache->end())
        (*cache)[type_name] = *die;
    }

  return DWARF_CB_OK;
}

int
dwflpp::global_alias_caching_callback_cus(Dwarf_Die *die, dwflpp *dw)
{
  mod_cu_type_cache_t *global_alias_cache;
  global_alias_cache = &dw->global_alias_cache;

  cu_type_cache_t *v = (*global_alias_cache)[die->addr];
  if (v != 0)
    return DWARF_CB_OK;

  v = new cu_type_cache_t;
  (*global_alias_cache)[die->addr] = v;
  iterate_over_globals(die, global_alias_caching_callback, v);

  return DWARF_CB_OK;
}

Dwarf_Die *
dwflpp::declaration_resolve_other_cus(const string& name)
{
  iterate_over_cus(global_alias_caching_callback_cus, this, true);
  for (mod_cu_type_cache_t::iterator i = global_alias_cache.begin();
         i != global_alias_cache.end(); ++i)
    {
      cu_type_cache_t *v = (*i).second;
      if (v->find(name) != v->end())
        return & ((*v)[name]);
    }

  return NULL;
}

Dwarf_Die *
dwflpp::declaration_resolve(const string& name)
{
  cu_type_cache_t *v = global_alias_cache[cu->addr];
  if (v == 0) // need to build the cache, just once per encountered module/cu
    {
      v = new cu_type_cache_t;
      global_alias_cache[cu->addr] = v;
      iterate_over_globals(cu, global_alias_caching_callback, v);
      if (sess.verbose > 4)
        clog << _F("global alias cache %s:%s size %zu", module_name.c_str(),
                   cu_name().c_str(), v->size()) << endl;
    }

  // XXX: it may be desirable to search other modules' declarations
  // too, in case a module/shared-library processes a
  // forward-declared pointer type only, where the actual definition
  // may only be in vmlinux or the application.

  if (v->find(name) == v->end())
    return declaration_resolve_other_cus(name);

  return & ((*v)[name]);
}

Dwarf_Die *
dwflpp::declaration_resolve(Dwarf_Die *type)
{
  const char* name = dwarf_diename(type);
  if (!name)
    return NULL;

  string type_name = cache_type_prefix(type) + string(name);
  return declaration_resolve(type_name);
}


int
dwflpp::cu_function_caching_callback (Dwarf_Die* func, cu_function_cache_t *v)
{
  const char *name = dwarf_diename(func);
  if (!name)
    return DWARF_CB_OK;

  v->insert(make_pair(name, *func));
  return DWARF_CB_OK;
}


int
dwflpp::mod_function_caching_callback (Dwarf_Die* cu, cu_function_cache_t *v)
{
  // need to cast callback to func which accepts void*
  dwarf_getfuncs (cu, (int (*)(Dwarf_Die*, void*))cu_function_caching_callback,
                  v, 0);
  return DWARF_CB_OK;
}


template<> int
dwflpp::iterate_over_functions<void>(int (*callback)(Dwarf_Die*, void*),
                                     void *data, const string& function)
{
  int rc = DWARF_CB_OK;
  assert (module);
  assert (cu);

  cu_function_cache_t *v = cu_function_cache[cu->addr];
  if (v == 0)
    {
      v = new cu_function_cache_t;
      cu_function_cache[cu->addr] = v;
      // need to cast callback to func which accepts void*
      dwarf_getfuncs (cu, (int (*)(Dwarf_Die*, void*))cu_function_caching_callback,
                      v, 0);
      if (sess.verbose > 4)
        clog << _F("function cache %s:%s size %zu", module_name.c_str(),
                   cu_name().c_str(), v->size()) << endl;
      mod_info->update_symtab(v);
    }

  cu_function_cache_t::iterator it;
  cu_function_cache_range_t range = v->equal_range(function);
  if (range.first != range.second)
    {
      for (it = range.first; it != range.second; ++it)
        {
          Dwarf_Die& die = it->second;
          if (sess.verbose > 4)
            clog << _F("function cache %s:%s hit %s", module_name.c_str(),
                       cu_name().c_str(), function.c_str()) << endl;  
          rc = (*callback)(& die, data);
          if (rc != DWARF_CB_OK) break;
        }
    }
  else if (startswith(function, "_Z"))
    {
      // C++ names are mangled starting with a "_Z" prefix.  Most of the time
      // we can discover the mangled name from a die's MIPS_linkage_name
      // attribute, so we read that to match against the user's function
      // pattern.  Note that this isn't perfect, as not all will have that
      // attribute (notably ctors and dtors), but we do what we can...
      for (it = v->begin(); it != v->end(); ++it)
        {
          if (pending_interrupts) return DWARF_CB_ABORT;
          Dwarf_Die& die = it->second;
          const char* linkage_name = NULL;
          if ((linkage_name = dwarf_linkage_name (&die))
              && function_name_matches_pattern (linkage_name, function))
            {
              if (sess.verbose > 4)
                clog << _F("function cache %s:%s match %s vs %s", module_name.c_str(),
                           cu_name().c_str(), linkage_name, function.c_str()) << endl;

              rc = (*callback)(& die, data);
              if (rc != DWARF_CB_OK) break;
            }
        }
    }
  else if (name_has_wildcard (function))
    {
      for (it = v->begin(); it != v->end(); ++it)
        {
          if (pending_interrupts) return DWARF_CB_ABORT;
          const string& func_name = it->first;
          Dwarf_Die& die = it->second;
          if (function_name_matches_pattern (func_name, function))
            {
              if (sess.verbose > 4)
                clog << _F("function cache %s:%s match %s vs %s", module_name.c_str(),
                           cu_name().c_str(), func_name.c_str(), function.c_str()) << endl;

              rc = (*callback)(& die, data);
              if (rc != DWARF_CB_OK) break;
            }
        }
    }
  else // not a linkage name or wildcard and no match in this CU
    {
      // do nothing
    }
  return rc;
}


template<> int
dwflpp::iterate_single_function<void>(int (*callback)(Dwarf_Die*, void*),
                                      void *data, const string& function)
{
  int rc = DWARF_CB_OK;
  assert (module);

  get_module_dwarf(false);
  if (!module_dwarf)
    return rc;

  cu_function_cache_t *v = mod_function_cache[module_dwarf];
  if (v == 0)
    {
      v = new cu_function_cache_t;
      mod_function_cache[module_dwarf] = v;
      iterate_over_cus (mod_function_caching_callback, v, false);
      if (sess.verbose > 4)
        clog << _F("module function cache %s size %zu", module_name.c_str(),
                   v->size()) << endl;
      mod_info->update_symtab(v);
    }

  cu_function_cache_t::iterator it;
  cu_function_cache_range_t range = v->equal_range(function);
  if (range.first != range.second)
    {
      for (it = range.first; it != range.second; ++it)
        {
          Dwarf_Die cu_mem;
          Dwarf_Die& die = it->second;
          if (sess.verbose > 4)
            clog << _F("module function cache %s hit %s", module_name.c_str(),
                       function.c_str()) << endl;

          // since we're iterating out of cu-context, we need each focus
          focus_on_cu(dwarf_diecu(&die, &cu_mem, NULL, NULL));

          rc = (*callback)(& die, data);
          if (rc != DWARF_CB_OK) break;
        }
    }

  // undo the focus_on_cu
  this->cu = NULL;
  this->function_name.clear();
  this->function = NULL;

  return rc;
}


/* This basically only goes one level down from the compile unit so it
 * only picks up top level stuff (i.e. nothing in a lower scope) */
template<> int
dwflpp::iterate_over_globals<void>(Dwarf_Die *cu_die,
                                   int (*callback)(Dwarf_Die*,
                                                   bool,
                                                   const string&,
                                                   void*),
                                   void *data)
{
  assert (cu_die);
  assert (dwarf_tag(cu_die) == DW_TAG_compile_unit
	  || dwarf_tag(cu_die) == DW_TAG_type_unit
	  || dwarf_tag(cu_die) == DW_TAG_partial_unit);

  // Ignore partial_unit, if they get imported by a real unit, then
  // iterate_over_types will traverse them.
  if (dwarf_tag(cu_die) == DW_TAG_partial_unit)
    return DWARF_CB_OK;

  // If this is C++, recurse for any inner types
  bool has_inner_types = dwarf_srclang(cu_die) == DW_LANG_C_plus_plus;

  return iterate_over_types(cu_die, has_inner_types, "", callback, data);
}

template<> int
dwflpp::iterate_over_types<void>(Dwarf_Die *top_die,
                                 bool has_inner_types,
                                 const string& prefix,
                                 int (* callback)(Dwarf_Die*,
                                                  bool,
                                                  const string&,
                                                  void*),
                                 void *data)
{
  int rc = DWARF_CB_OK;
  Dwarf_Die die, import;

  assert (top_die);

  if (dwarf_child(top_die, &die) != 0)
    return rc;

  do
    /* We're only currently looking for named types,
     * although other types of declarations exist */
    switch (dwarf_tag(&die))
      {
      case DW_TAG_base_type:
      case DW_TAG_enumeration_type:
      case DW_TAG_structure_type:
      case DW_TAG_class_type:
      case DW_TAG_typedef:
      case DW_TAG_union_type:
      case DW_TAG_namespace:
        rc = (*callback)(&die, has_inner_types, prefix, data);
        break;

      case DW_TAG_imported_unit:
	// Follow the imported_unit and iterate over its contents
	// (either a partial_unit or a full compile_unit), all its
	// children should be treated as if they appear in this place.
	if (dwarf_attr_die(&die, DW_AT_import, &import))
	  rc = iterate_over_types(&import, has_inner_types, prefix,
				  callback, data);
	break;
      }
  while (rc == DWARF_CB_OK && dwarf_siblingof(&die, &die) == 0);

  return rc;
}


/* For each notes section in the current module call 'callback', use
 * 'data' for the notes buffer and pass 'object' back in case
 * 'callback' is a method */

template<> int
dwflpp::iterate_over_notes<void>(void *object, void (*callback)(void*,
                                                                const string&,
                                                                const string&,
                                                                int,
                                                                const char*,
                                                                size_t))
{
  Dwarf_Addr bias;
  // Note we really want the actual elf file, not the dwarf .debug file.
  // Older binutils had a bug where they mangled the SHT_NOTE type during
  // --keep-debug.
  Elf* elf = dwfl_module_getelf (module, &bias);
  size_t shstrndx;
  if (elf_getshdrstrndx (elf, &shstrndx))
    return elf_errno();

  Elf_Scn *scn = NULL;

  vector<Dwarf_Die> notes;

  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr;
      if (gelf_getshdr (scn, &shdr) == NULL)
	  continue;
      switch (shdr.sh_type)
	{
	case SHT_NOTE:
	  if (!(shdr.sh_flags & SHF_ALLOC))
	    {
	      string scn_name = elf_strptr(elf, shstrndx, shdr.sh_name);
	      Elf_Data *data = elf_getdata (scn, NULL);
	      size_t next;
	      GElf_Nhdr nhdr;
	      size_t name_off;
	      size_t desc_off;
	      for (size_t offset = 0;
		   (next = gelf_getnote (data, offset, &nhdr, &name_off, &desc_off)) > 0;
		   offset = next)
		{
		  const char *note_name_addr = (const char *)data->d_buf + name_off;
		  const char *note_desc_addr = (const char *)data->d_buf + desc_off;
		  string note_name = nhdr.n_namesz > 1 // n_namesz includes NULL
		                     ? string(note_name_addr, nhdr.n_namesz-1) : "";
		  (*callback) (object, scn_name, note_name, nhdr.n_type,
		               note_desc_addr, nhdr.n_descsz);
		}
	    }
	  break;
	}
    }
  return 0;
}


/* For each entry in the .dynamic section in the current module call 'callback'
 * returning 'object' in case 'callback' is a method */

template<> void
dwflpp::iterate_over_libraries<void>(void (*callback)(void*, const char*),
                                     void *data)
{
  std::set<std::string> added;
  string interpreter;

  assert (this->module_name.length() != 0);

  Dwarf_Addr bias;
//  We cannot use this: dwarf_getelf (dwfl_module_getdwarf (module, &bias))
  Elf *elf = dwfl_module_getelf (module, &bias);
//  elf_getphdrnum (elf, &phnum) is not available in all versions of elfutils
//  needs libelf from elfutils 0.144+
  for (int i = 0; ; i++)
    {
      GElf_Phdr mem;
      GElf_Phdr *phdr;
      phdr = gelf_getphdr (elf, i, &mem);
      if (phdr == NULL)
        break;
      if (phdr->p_type == PT_INTERP)
        {
          size_t maxsize;
          char *filedata = elf_rawfile (elf, &maxsize);

          if (filedata != NULL && phdr->p_offset < maxsize)
            interpreter = (char*) (filedata + phdr->p_offset);
          break;
        }
    }

  if (interpreter.length() == 0)
    return;
  // If it gets cumbersome to maintain this whitelist, we could just check for
  // startswith("/lib/ld") || startswith("/lib64/ld"), and trust that no admin
  // would install untrustworthy loaders in those paths.
  // See also http://sourceware.org/git/?p=glibc.git;a=blob;f=shlib-versions;hb=HEAD
  if (interpreter != "/lib/ld.so.1"                     // s390, ppc
      && interpreter != "/lib/ld64.so.1"                // s390x, ppc64
      && interpreter != "/lib64/ld64.so.1"
      && interpreter != "/lib/ld-linux-ia64.so.2"       // ia64
      && interpreter != "/emul/ia32-linux/lib/ld-linux.so.2"
      && interpreter != "/lib64/ld-linux-x86-64.so.2"   // x8664
      && interpreter != "/lib/ld-linux.so.2"            // x86
      && interpreter != "/lib/ld-linux.so.3"            // arm
      && interpreter != "/lib/ld-linux-armhf.so.3"      // arm
      && interpreter != "/lib/ld-linux-aarch64.so.1"    // arm64
      && interpreter != "/lib64/ld64.so.2"              // ppc64le
      )
    {
      sess.print_warning (_F("module %s --ldd skipped: unsupported interpreter: %s",
                               module_name.c_str(), interpreter.c_str()));
      return;
    }

  vector<string> ldd_command;
  ldd_command.push_back("/usr/bin/env");
  ldd_command.push_back("LD_TRACE_LOADED_OBJECTS=1");
  ldd_command.push_back("LD_WARN=yes");
  ldd_command.push_back("LD_BIND_NOW=yes");
  ldd_command.push_back(interpreter);
  ldd_command.push_back(module_name);

  FILE *fp;
  int child_fd;
  pid_t child = stap_spawn_piped(sess.verbose, ldd_command, NULL, &child_fd);
  if (child <= 0 || !(fp = fdopen(child_fd, "r")))
    clog << _F("library iteration on %s failed: %s",
               module_name.c_str(), strerror(errno)) << endl;
  else
    {
      while (1) // this parsing loop borrowed from add_unwindsym_ldd
        {
          char linebuf[256];
          char *soname = 0;
          char *shlib = 0;
          unsigned long int addr = 0;

          char *line = fgets (linebuf, 256, fp);
          if (line == 0) break; // EOF or error

#if __GLIBC__ >2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 7)
#define MS_FMT "%ms"
#else
#define MS_FMT "%as"
#endif
          // Try soname => shlib (0xaddr)
          int nf = sscanf (line, MS_FMT " => " MS_FMT " (0x%lx)",
              &soname, &shlib, &addr);
          if (nf != 3 || shlib[0] != '/')
            {
              // Try shlib (0xaddr)
              nf = sscanf (line, " " MS_FMT " (0x%lx)", &shlib, &addr);
              if (nf != 2 || shlib[0] != '/')
                continue; // fewer than expected fields, or bad shlib.
            }

          if (added.find (shlib) == added.end())
            {
              if (sess.verbose > 2)
                {
                  clog << _F("Added -d '%s", shlib);
                  if (nf == 3)
                    clog << _F("' due to '%s'", soname);
                  else
                    clog << "'";
                  clog << endl;
                }
              added.insert (shlib);
            }

          free (soname);
          free (shlib);
        }
      if ((fclose(fp) || stap_waitpid(sess.verbose, child)))
         sess.print_warning("failed to read libraries from " + module_name + ": " + strerror(errno));
    }

  for (std::set<std::string>::iterator it = added.begin();
      it != added.end();
      it++)
    {
      string modname = *it;
      (callback) (data, modname.c_str());
    }
}


/* For each plt section in the current module call 'callback', pass the plt entry
 * 'address' and 'name' back, and pass 'object' back in case 'callback' is a method */

template<> int
dwflpp::iterate_over_plt<void>(void *object, void (*callback)(void*,
                                                              const char*,
                                                              size_t))
{
  Dwarf_Addr load_addr;
  // Note we really want the actual elf file, not the dwarf .debug file.
  Elf* elf = dwfl_module_getelf (module, &load_addr);
  size_t shstrndx;
  assert (elf_getshdrstrndx (elf, &shstrndx) >= 0);

  // Get the load address
  for (int i = 0; ; i++)
    {
      GElf_Phdr mem;
      GElf_Phdr *phdr;
      phdr = gelf_getphdr (elf, i, &mem);
      if (phdr == NULL)
	break;
      if (phdr->p_type == PT_LOAD)
	{
	  load_addr = phdr->p_vaddr;
	  break;
	}
    }

  // Get the plt section header
  Elf_Scn *scn = NULL;
  GElf_Shdr *plt_shdr = NULL;
  GElf_Shdr plt_shdr_mem;
  while ((scn = elf_nextscn (elf, scn)))
    {
      GElf_Shdr *shdr = gelf_getshdr (scn, &plt_shdr_mem);
      assert (shdr != NULL);
      if (strcmp (elf_strptr (elf, shstrndx, shdr->sh_name), ".plt") == 0)
	{
	  plt_shdr = shdr;
	  break;
	}
    }
  if (plt_shdr == NULL)
    return 0;

  // Layout of the plt section
  int plt0_entry_size;
  int plt_entry_size;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr* em = gelf_getehdr (elf, &ehdr_mem);
  switch (em->e_machine)
  {
  case EM_386:    plt0_entry_size = 16; plt_entry_size = 16; break;
  case EM_X86_64: plt0_entry_size = 16; plt_entry_size = 16; break;
  case EM_ARM:    plt0_entry_size = 20; plt_entry_size = 12; break;
  case EM_AARCH64:plt0_entry_size = 32; plt_entry_size = 16; break;
  case EM_PPC64:
  case EM_S390:
  case EM_PPC:
  default:
    throw SEMANTIC_ERROR(".plt is not supported on this architecture");
  }

  scn = NULL;
  while ((scn = elf_nextscn (elf, scn)))
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      bool have_rela = false;
      bool have_rel = false;

      if (shdr == NULL)
        continue;
      assert (shdr != NULL);

      if ((have_rela = (strcmp (elf_strptr (elf, shstrndx, shdr->sh_name), ".rela.plt") == 0))
	  || (have_rel = (strcmp (elf_strptr (elf, shstrndx, shdr->sh_name), ".rel.plt") == 0)))
	{
	  /* Get the data of the section.  */
	  Elf_Data *data = elf_getdata (scn, NULL);
	  assert (data != NULL);
	  /* Get the symbol table information.  */
	  Elf_Scn *symscn = elf_getscn (elf, shdr->sh_link);
	  GElf_Shdr symshdr_mem;
	  GElf_Shdr *symshdr = gelf_getshdr (symscn, &symshdr_mem);
	  assert (symshdr != NULL);
	  Elf_Data *symdata = elf_getdata (symscn, NULL);
	  assert (symdata != NULL);

	  unsigned int nsyms = shdr->sh_size / shdr->sh_entsize;
	  
	  for (unsigned int cnt = 0; cnt < nsyms; ++cnt)
	    {
	      GElf_Ehdr ehdr_mem;
	      GElf_Ehdr* em = gelf_getehdr (elf, &ehdr_mem);
	      if (em == 0) { DWFL_ASSERT ("dwfl_getehdr", dwfl_errno()); }

	      GElf_Rela relamem;
	      GElf_Rela *rela = NULL;
	      GElf_Rel relmem;
	      GElf_Rel *rel = NULL;
	      if (have_rela)
		{
		  rela = gelf_getrela (data, cnt, &relamem);
		  assert (rela != NULL);
		}
	      else if (have_rel)
		{
		  rel = gelf_getrel (data, cnt, &relmem);
		  assert (rel != NULL);
		}
	      GElf_Sym symmem;
	      Elf32_Word xndx;
	      Elf_Data *xndxdata = NULL;
	      GElf_Sym *sym =
		gelf_getsymshndx (symdata, xndxdata,
				  GELF_R_SYM (have_rela ? rela->r_info : rel->r_info),
				  &symmem, &xndx);
	      assert (sym != NULL);
	      Dwarf_Addr addr = plt_shdr->sh_offset + plt0_entry_size + cnt * plt_entry_size;

	      if (elf_strptr (elf, symshdr->sh_link, sym->st_name))
	        (*callback) (object, elf_strptr (elf, symshdr->sh_link, sym->st_name), addr + load_addr);
	    }
	  break; // while scn
	}
    }
  return 0;
}


// Comparator function for sorting
static bool
compare_lines(Dwarf_Line* a, Dwarf_Line* b)
{
  if (a == b)
    return false;

  int lineno_a = DWARF_LINENO(a);
  int lineno_b = DWARF_LINENO(b);
  if (lineno_a == lineno_b)
    return DWARF_LINEADDR(a) < DWARF_LINEADDR(b);
  return lineno_a < lineno_b;
}

// Comparator object for searching Dwarf_Lines with a specific lineno when we
// don't have a Dwarf_Line of our own to search for (hence why a or b is always
// NULL).
struct lineno_comparator {
  int lineno;
  lineno_comparator(int lineno): lineno(lineno) {}
  bool operator() (Dwarf_Line* a, Dwarf_Line* b)
    {
      assert(a || b);
      if (a)
        return DWARF_LINENO(a) < lineno;
      else
        return lineno < DWARF_LINENO(b);
    }
};

// Returns a range of lines in between begin and end with wanted lineno. If
// none exist, points to where it would have been.
static lines_range_t
lineno_equal_range(lines_t* v, int lineno)
{
  lineno_comparator lc(lineno);
  return equal_range(v->begin(), v->end(), (Dwarf_Line*)NULL, lc);
}

// Interface to CU lines cache sorted by lineno
lines_t*
dwflpp::get_cu_lines_sorted_by_lineno(const char *srcfile)
{
  assert(cu);

  srcfile_lines_cache_t *srcfile_lines = cu_lines_cache[cu];
  if (!srcfile_lines)
    {
      srcfile_lines = new srcfile_lines_cache_t();
      cu_lines_cache[cu] = srcfile_lines;
    }

  lines_t *lines = (*srcfile_lines)[srcfile];
  if (!lines)
    {
      size_t nlines_cu = 0;
      Dwarf_Lines *lines_cu = NULL;
      DWARF_ASSERT("dwarf_getsrclines",
                   dwarf_getsrclines(cu, &lines_cu, &nlines_cu));

      lines = new lines_t();
      (*srcfile_lines)[srcfile] = lines;

      for (size_t i = 0; i < nlines_cu; i++)
        {
          Dwarf_Line *line = dwarf_onesrcline(lines_cu, i);
          const char *linesrc = DWARF_LINESRC(line);
          if (strcmp(srcfile, linesrc))
            continue;
          lines->push_back(line);
        }

      if (lines->size() > 1)
        sort(lines->begin(), lines->end(), compare_lines);

      if (sess.verbose > 3)
        {
          clog << _F("found the following lines for %s:", srcfile) << endl;
          lines_t::iterator i;
          for (i  = lines->begin(); i != lines->end(); ++i)
            cout << DWARF_LINENO(*i) << " = " << hex
                 << DWARF_LINEADDR(*i) << dec << endl;
        }
    }
  return lines;
}

static Dwarf_Line*
get_func_first_line(Dwarf_Die *cu, base_func_info& func)
{
  // dwarf_getsrc_die() uses binary search to find the Dwarf_Line, but will
  // return the wrong line if not found.
  Dwarf_Line *line = dwarf_getsrc_die(cu, func.entrypc);
  if (line && DWARF_LINEADDR(line) == func.entrypc)
    return line;

  // Line not found (or line at wrong addr). We have to resort to a slower
  // linear method. We won't find an exact match (probably this is an inlined
  // instance), so just settle on the first Dwarf_Line with lowest addr which
  // falls in the die.
  size_t nlines = 0;
  Dwarf_Lines *lines = NULL;
  DWARF_ASSERT("dwarf_getsrclines",
               dwarf_getsrclines(cu, &lines, &nlines));

  for (size_t i = 0; i < nlines; i++)
    {
      line = dwarf_onesrcline(lines, i);
      if (dwarf_haspc(&func.die, DWARF_LINEADDR(line)))
        return line;
    }
  return NULL;
}

static lines_t
collect_lines_in_die(lines_range_t range, Dwarf_Die *die)
{
  lines_t lines_in_die;
  for (lines_t::iterator line  = range.first;
                         line != range.second; ++line)
    if (dwarf_haspc(die, DWARF_LINEADDR(*line)))
      lines_in_die.push_back(*line);
  return lines_in_die;
}

static void
add_matching_lines_in_func(Dwarf_Die *cu,
                           lines_t *cu_lines,
                           base_func_info& func,
                           lines_t& matching_lines)
{
  Dwarf_Line *start_line = get_func_first_line(cu, func);
  if (!start_line)
    return;

  for (int lineno = DWARF_LINENO(start_line);;)
    {
      lines_range_t range = lineno_equal_range(cu_lines, lineno);

      // We consider the lineno still part of the die if at least one of them
      // falls in the die.
      lines_t lines_in_die = collect_lines_in_die(range, &func.die);
      if (lines_in_die.empty())
        break;

      // Just pick the first LR even if there are more than one. Since the lines
      // are sorted by lineno and then addr, the first one is the one with the
      // lowest addr.
      matching_lines.push_back(lines_in_die[0]);

      // break out if there are no more lines, otherwise, go to the next lineno
      if (range.second == cu_lines->end())
        break;
      lineno = DWARF_LINENO(*range.second);
    }
}

void
dwflpp::collect_all_lines(char const * srcfile,
                          base_func_info_map_t& funcs,
                          lines_t& matching_lines)
{
  // This is where we handle WILDCARD lineno types.
  lines_t *cu_lines = get_cu_lines_sorted_by_lineno(srcfile);
  for (base_func_info_map_t::iterator func  = funcs.begin();
                                      func != funcs.end(); ++func)
    add_matching_lines_in_func(cu, cu_lines, *func, matching_lines);
}


static void
add_matching_line_in_die(lines_t *cu_lines,
                         lines_t& matching_lines,
                         Dwarf_Die *die, int lineno)
{
  lines_range_t lineno_range = lineno_equal_range(cu_lines, lineno);
  lines_t lines_in_die = collect_lines_in_die(lineno_range, die);
  if (lines_in_die.empty())
    return;

  // Even if there are more than 1 LRs, just pick the first one. Since the lines
  // are sorted by lineno and then addr, the first one is the one with the
  // lowest addr. This is similar to what GDB does.
  matching_lines.push_back(lines_in_die[0]);
}

void
dwflpp::collect_lines_for_single_lineno(char const * srcfile,
                                        int lineno,
                                        bool is_relative,
                                        base_func_info_map_t& funcs,
                                        lines_t& matching_lines)
{
  /* Here, we handle ABSOLUTE and RELATIVE lineno types. Relative line numbers
   * are a bit special. The issue is that functions (esp. inlined ones) may not
   * even have a LR corresponding to the first valid line of code. So, applying
   * an offset to the 'first' LR found in the DIE can be quite imprecise.
   *     Instead, we use decl_line, which although does not necessarily have a
   * LR associated with it (it can sometimes still happen esp. if the code is
   * written in OTB-style), it serves as an anchor on which we can apply the
   * offset to yield a lineno that will not change with compiler optimization.
   *     It also has the added benefit of being consistent with the lineno
   * printed by e.g. stap -l kernel.function("vfs_read"), so users can be
   * confident from what lineno we adjust.
   */
  lines_t *cu_lines = get_cu_lines_sorted_by_lineno(srcfile);
  for (base_func_info_map_t::iterator func  = funcs.begin();
                                      func != funcs.end(); ++func)
    add_matching_line_in_die(cu_lines, matching_lines, &func->die,
                             is_relative ? lineno + func->decl_line
                                         : lineno);
}

static bool
functions_have_lineno(base_func_info_map_t& funcs,
                      lines_t *lines, int lineno)
{
  lines_range_t lineno_range = lineno_equal_range(lines, lineno);
  if (lineno_range.first == lineno_range.second)
    return false; // no LRs at this lineno

  for (base_func_info_map_t::iterator func  = funcs.begin();
                                      func != funcs.end(); ++func)
    if (!collect_lines_in_die(lineno_range, &func->die).empty())
      return true;

  return false;
}

// returns pair of valid linenos surrounding target lineno
pair<int,int>
dwflpp::get_nearest_linenos(char const * srcfile,
                            int lineno,
                            base_func_info_map_t& funcs)
{
  assert(cu);
  lines_t *cu_lines = get_cu_lines_sorted_by_lineno(srcfile);

  // Look around lineno for linenos with LRs.
  pair<int,int> nearest_linenos = make_pair(-1, -1);
  for (size_t i = 1; i < 6; ++i)
    {
      if (nearest_linenos.first == -1 && functions_have_lineno(funcs, cu_lines, lineno-i))
        nearest_linenos.first = lineno - i;
      if (nearest_linenos.second == -1 && functions_have_lineno(funcs, cu_lines, lineno+i))
        nearest_linenos.second = lineno + i;
    }

  return nearest_linenos;
}

// returns nearest valid lineno to target lineno
int
dwflpp::get_nearest_lineno(char const * srcfile,
                           int lineno,
                           base_func_info_map_t& funcs)
{
  assert(cu);
  pair<int,int> nearest_linenos = get_nearest_linenos(srcfile, lineno, funcs);

  if (nearest_linenos.first > 0
      && nearest_linenos.second > 0)
    {
      // pick the nearest line number (break tie to upper)
      if (lineno - nearest_linenos.first < nearest_linenos.second - lineno)
        return nearest_linenos.first;
      else
        return nearest_linenos.second;
    }
  else if (nearest_linenos.first > 0)
    return nearest_linenos.first;
  else if (nearest_linenos.second > 0)
    return nearest_linenos.second;
  else
    return -1;
}

void
dwflpp::suggest_alternative_linenos(char const * srcfile,
                                    int lineno,
                                    base_func_info_map_t& funcs)
{
  assert(cu);
  pair<int,int> nearest_linenos = get_nearest_linenos(srcfile, lineno, funcs);

  stringstream advice;
  advice << _F("no line records for %s:%d [man error::dwarf]", srcfile, lineno);

  if (nearest_linenos.first > 0 || nearest_linenos.second > 0)
    {
      //TRANSLATORS: Here we are trying to advise what source file
      //TRANSLATORS: to attempt.
      advice << _(" (try ");
      if (nearest_linenos.first > 0)
        advice << ":" << nearest_linenos.first;
      if (nearest_linenos.first > 0 && nearest_linenos.second > 0)
        advice << _(" or ");
      if (nearest_linenos.second > 0)
        advice << ":" << nearest_linenos.second;
      advice << ")";
    }
  throw SEMANTIC_ERROR (advice.str());
}

static base_func_info_map_t
get_funcs_in_srcfile(base_func_info_map_t& funcs,
                     const char * srcfile)
{
  base_func_info_map_t matching_funcs;
  for (base_func_info_map_t::iterator func  = funcs.begin();
                                      func != funcs.end(); ++func)
    if (func->decl_file == string(srcfile))
      matching_funcs.push_back(*func);
  return matching_funcs;
}

template<> void
dwflpp::iterate_over_srcfile_lines<void>(char const * srcfile,
                                         const vector<int>& linenos,
                                         enum lineno_t lineno_type,
                                         base_func_info_map_t& funcs,
                                         void (* callback) (Dwarf_Addr,
                                                            int, void*),
                                         bool has_nearest,
                                         void *data)
{
  /* Matching line records (LRs) to user-provided linenos is trickier than it
   * seems. The fate of all functions is one of three possibilities:
   *  1. it's a normal function, with a subprogram DIE and a bona fide lowpc
   *     and highpc attribute.
   *  2. it's an inlined function (one/multiple inlined_subroutine DIE, with one
   *     abstract_origin DIE)
   *  3. it's both a normal function and an inlined function. For example, if
   *     the funtion has been inlined only in some places, we'll have a DIE for
   *     the normal subprogram DIE as well as inlined_subroutine DIEs.
   *
   * Multiple LRs for the same lineno but different addresses can simply happen
   * due to the function appearing in multiple forms. E.g. a function inlined
   * in two spots can yield two sets of LRs for its linenos at the different
   * addresses where it is inlined.
   *     This is why the collect_* functions used here try to match up LRs back
   * to their originating DIEs. For example, in the function
   * collect_lines_for_single_lineno(), we filter first by DIE so that a lineno
   * corresponding to multiple addrs in multiple inlined_subroutine DIEs yields
   * a probe for each of them.
   */
  assert(cu);

  // only work on the functions found in the current srcfile
  base_func_info_map_t current_funcs = get_funcs_in_srcfile(funcs, srcfile);
  if (current_funcs.empty())
    return;

  // collect lines
  lines_t matching_lines;
  if (lineno_type == ABSOLUTE)
    collect_lines_for_single_lineno(srcfile, linenos[0], false, /* is_relative */
                                    current_funcs, matching_lines);
  else if (lineno_type == RELATIVE)
    collect_lines_for_single_lineno(srcfile, linenos[0], true, /* is_relative */
                                    current_funcs, matching_lines);
  else if (lineno_type == WILDCARD)
    collect_all_lines(srcfile, current_funcs, matching_lines);
  else if (lineno_type == ENUMERATED)
    {
      set<int> collected_linenos;
      for (vector<int>::const_iterator it  = linenos.begin();
                                       it != linenos.end(); it++)
        {
          // have we already collected this lineno?
          if (collected_linenos.find(*it) != collected_linenos.end())
            continue;

          // remember end iterator so we can tell if things were found later
          lines_t::const_iterator itend = matching_lines.end();

          collect_lines_for_single_lineno(srcfile, *it, false, /* is_relative */
                                          current_funcs, matching_lines);
          // add to set if we found LRs
          if (itend != matching_lines.end())
            collected_linenos.insert(*it);

          // if we didn't find anything and .nearest is given, then try nearest
          if (itend == matching_lines.end() && has_nearest)
            {
              int nearest_lineno = get_nearest_lineno(srcfile, *it,
                                                      current_funcs);
              if (nearest_lineno <= 0) // no valid nearest linenos
                continue;

              bool new_lineno = collected_linenos.insert(nearest_lineno).second;
              if (new_lineno)
                collect_lines_for_single_lineno(srcfile, nearest_lineno,
                                                false, /* is_relative */
                                                current_funcs, matching_lines);
            }
        }
    }

  // should we try to collect the nearest lines if we didn't collect everything
  // on first try? (ABSOLUTE and RELATIVE only: ENUMERATED handles it already
  // and WILDCARD doesn't need it)
  if (matching_lines.empty() && has_nearest && (lineno_type == ABSOLUTE ||
                                                lineno_type == RELATIVE))
    {
      int lineno = linenos[0];
      if (lineno_type == RELATIVE)
        // just pick the first function and make it relative to that
        lineno += current_funcs[0].decl_line;

      int nearest_lineno = get_nearest_lineno(srcfile, lineno, current_funcs);
      if (nearest_lineno > 0)
        collect_lines_for_single_lineno(srcfile, nearest_lineno,
                                        false, /* is_relative */
                                        current_funcs, matching_lines);
    }

  // call back with matching lines
  if (!matching_lines.empty())
    {
      set<Dwarf_Addr> probed_addrs;
      for (lines_t::iterator line  = matching_lines.begin();
                             line != matching_lines.end(); ++line)
        {
          int lineno = DWARF_LINENO(*line);
          Dwarf_Addr addr = DWARF_LINEADDR(*line);
          bool is_new_addr = probed_addrs.insert(addr).second;
          if (is_new_addr)
            callback(addr, lineno, data);
        }
    }
  // No LRs found at the wanted lineno. So let's suggest other ones if user was
  // targeting a specific lineno (ABSOLUTE or RELATIVE).
  else if (lineno_type == ABSOLUTE ||
           lineno_type == RELATIVE)
    {
      int lineno = linenos[0];
      if (lineno_type == RELATIVE)
        // just pick the first function and make it relative to that
        lineno += current_funcs[0].decl_line;

      suggest_alternative_linenos(srcfile, lineno, current_funcs);
    }
}


template<> void
dwflpp::iterate_over_labels<void>(Dwarf_Die *begin_die,
                                  const string& sym,
                                  const base_func_info& function,
                                  const vector<int>& linenos,
                                  enum lineno_t lineno_type,
                                  void *data,
                                  void (* callback)(const base_func_info&,
                                                    const char*,
                                                    const char*,
                                                    int,
                                                    Dwarf_Die*,
                                                    Dwarf_Addr,
                                                    void*))
{
  get_module_dwarf();

  Dwarf_Die die, import;
  const char *name;
  int res = dwarf_child (begin_die, &die);
  if (res != 0)
    return;  // die without children, bail out.

  do
    {
      switch (dwarf_tag(&die))
        {
        case DW_TAG_label:
          name = dwarf_diename (&die);
          if (name &&
              (name == sym
               || (name_has_wildcard(sym)
                   && function_name_matches_pattern (name, sym))))
            {
              // Don't try to be smart. Just drop no addr labels.
              Dwarf_Addr stmt_addr;
              if (dwarf_lowpc (&die, &stmt_addr) == 0)
                {
                  // Get the file/line number for this label
                  int dline;
                  const char *file = dwarf_decl_file (&die) ?: "<unknown source>";
                  dwarf_decl_line (&die, &dline);

                  vector<Dwarf_Die> scopes = getscopes_die(&die);
                  if (scopes.size() > 1)
                    {
                      Dwarf_Die scope;
                      if (!inner_die_containing_pc(scopes[1], stmt_addr, scope))
                        {
                          sess.print_warning(_F("label '%s' at address %s (dieoffset: %s) is not "
                                                "contained by its scope '%s' (dieoffset: %s) -- bad"
                                                " debuginfo? [man error::dwarf]", name, lex_cast_hex(stmt_addr).c_str(),
                                                lex_cast_hex(dwarf_dieoffset(&die)).c_str(),
                                                (dwarf_diename(&scope) ?: "<unknown>"),
                                                lex_cast_hex(dwarf_dieoffset(&scope)).c_str()));
                        }

                      bool matches_lineno;
                      if (lineno_type == ABSOLUTE)
                        matches_lineno = dline == linenos[0];
                      else if (lineno_type == RELATIVE)
                        matches_lineno = dline == linenos[0] + function.decl_line;
                      else if (lineno_type == ENUMERATED)
                        matches_lineno = (binary_search(linenos.begin(), linenos.end(), dline));
                      else // WILDCARD
                        matches_lineno = true;

                      if (matches_lineno)
                        callback(function, name, file, dline,
                                 &scope, stmt_addr, data);
                    }
                }
            }
          break;

        case DW_TAG_subprogram:
        case DW_TAG_inlined_subroutine:
          // Stay within our filtered function
          break;

	case DW_TAG_imported_unit:
	  // Iterate over the children of the imported unit as if they
	  // were inserted in place.
	  if (dwarf_attr_die(&die, DW_AT_import, &import))
	    iterate_over_labels (&import, sym, function, linenos,
	                         lineno_type, data, callback);
	  break;

        default:
          if (dwarf_haschildren (&die))
            iterate_over_labels (&die, sym, function, linenos,
                                 lineno_type, data, callback);
          break;
        }
    }
  while (dwarf_siblingof (&die, &die) == 0);
}

// Mini 'query-like' struct to help us navigate callbacks during
// external function resolution
struct external_function_query {
  dwflpp* dw;
  const string name;
  Dwarf_Die die;
  Dwarf_Addr addr;
  bool resolved;
  external_function_query(dwflpp* dw, const string& name):
    dw(dw), name(name), die(), addr(0), resolved(false) {}
};

int
dwflpp::external_function_cu_callback (Dwarf_Die* cu, external_function_query *efq)
{
  efq->dw->focus_on_cu(cu);
  return efq->dw->iterate_over_functions(external_function_func_callback,
                                         efq, efq->name);
}

int
dwflpp::external_function_func_callback (Dwarf_Die* func, external_function_query *efq)
{
  Dwarf_Attribute external;
  Dwarf_Addr func_addr;
  if (dwarf_attr_integrate(func, DW_AT_external, &external) != NULL &&
      dwarf_lowpc(func, &func_addr) == 0)
    {
      efq->die = *func;
      efq->addr = func_addr;
      efq->resolved = true;
      return DWARF_CB_ABORT; // we found it so stop here
    }
  return DWARF_CB_OK;
}

template<> void
dwflpp::iterate_over_callees<void>(Dwarf_Die *begin_die,
                                   const string& sym,
                                   int64_t recursion_depth,
                                   void *data,
                                   void (* callback)(base_func_info&,
                                                     base_func_info&,
                                                     stack<Dwarf_Addr>*,
                                                     void*),
                                   base_func_info& caller,
                                   stack<Dwarf_Addr> *callers)
{
  get_module_dwarf();

  Dwarf_Die die, import;

  // DIE of abstract_origin found in die
  Dwarf_Die origin;

  // callee's entry pc (= where we'll probe)
  Dwarf_Addr func_addr;

  // caller's unwind pc during call (to match against bt for filtering)
  Dwarf_Addr caller_uw_addr;

  Dwarf_Attribute attr;

  base_func_info callee;
  if (dwarf_child(begin_die, &die) != 0)
    return;  // die without children, bail out.

  bool free_callers = false;
  if (callers == NULL) /* first call */
    {
      callers = new stack<Dwarf_Addr>();
      free_callers = true;
    }

  do
    {
      bool inlined = false;
      switch (dwarf_tag(&die))
        {
        case DW_TAG_inlined_subroutine:
          inlined = true;
          /* FALLTHROUGH */ /* thanks mjw */
        case DW_TAG_GNU_call_site:
          callee.name = dwarf_diename(&die) ?: "";
          if (callee.name.empty())
            continue;
          if (callee.name != sym)
            {
              if (!name_has_wildcard(sym))
                continue;
              if (!function_name_matches_pattern(callee.name, sym))
                continue;
            }

          /* In both cases (call sites and inlines), we want the
           * abstract_origin. The difference is that in inlines, the addr is
           * in the die itself, whereas for call sites, the addr is in the
           * abstract_origin's die.
           *     Note that in the case of inlines, we're only calling back
           * for that inline instance, not all. This is what we want, since
           * it will only be triggered when 'called' from the target func,
           * which is something we have to emulate for non-inlined funcs
           * (which is the purpose of the caller_uw_addr below) */
          if (dwarf_attr_die(&die, DW_AT_abstract_origin, &origin) == NULL)
            continue;

          // the low_pc of the die in either cases is the pc that would
          // show up in a backtrace (inlines are a special case in which
          // the die's low_pc is also the abstract_origin's low_pc = the
          // 'start' of the inline instance)
          if (dwarf_lowpc(&die, &caller_uw_addr) != 0)
            continue;

          if (inlined)
            func_addr = caller_uw_addr;
          else if (dwarf_lowpc(&origin, &func_addr) != 0)
            {
              // function doesn't have a low_pc, is it external?
              if (dwarf_attr_integrate(&origin, DW_AT_external,
                                       &attr) != NULL)
                {
                  // let's iterate over the CUs and find it. NB: it's
                  // possible we could have also done this by creating a
                  // probe point with .exported tacked on and rerunning it
                  // through derive_probe(). But since we're already on the
                  // dwflpp side of things, and we already have access to
                  // everything we need, let's try to be self-sufficient.

                  // remember old focus
                  Dwarf_Die *old_cu = cu;

                  external_function_query efq(this, dwarf_linkage_name(&origin) ?: callee.name);
                  iterate_over_cus(external_function_cu_callback, &efq, false);

                  // restore focus
                  cu = old_cu;

                  if (!efq.resolved) // did we resolve it?
                    continue;

                  func_addr = efq.addr;
                  origin = efq.die;
                }
              // non-external function without low_pc, jump ship
              else continue;
            }

          // We now have the addr to probe in func_addr, and the DIE
          // from which to obtain file/line info in origin

          // Get the file/line number for this callee
          callee.decl_file = dwarf_decl_file (&origin) ?: "<unknown source>";
          dwarf_decl_line (&origin, &callee.decl_line);

          // add as a caller to match against
          if (!inlined)
            callers->push(caller_uw_addr);

          callee.die = inlined ? die : origin;
          callee.entrypc = func_addr;
          callback(callee, caller, callers, data);

          // If it's a tail call, print a warning that it may not be caught
          if (!inlined
              && dwarf_attr_integrate(&die, DW_AT_GNU_tail_call, &attr) != NULL)
            sess.print_warning (_F("Callee \"%s\" in function \"%s\" is a tail call: "
                                   ".callee probe may not fire. Try placing the probe "
                                   "directly on the callee function instead.",
                                   callee.name.to_string().c_str(),
                                   caller.name.to_string().c_str()));
          
          // For .callees(N) probes, we recurse on this callee. Note that we
          // pass the callee we just found as the caller arg for this recursion,
          // since it (the callee we just found) will be the caller of whatever
          // callees found inside this recursion.
          if (recursion_depth > 1)
            iterate_over_callees(inlined ? &die : &origin,
                                 sym, recursion_depth-1, data,
                                 callback, callee, callers);

          if (!inlined)
            callers->pop();
          break;

        case DW_TAG_subprogram:
          break; // don't leave our filtered func

        case DW_TAG_imported_unit:
          // Iterate over the children of the imported unit as if they
          // were inserted in place.
          if (dwarf_attr_die(&die, DW_AT_import, &import))
            // NB: we pass the same caller arg into it
            iterate_over_callees (&import, sym, recursion_depth, data,
                                  callback, caller, callers);
          break;

        default:
          if (dwarf_haschildren (&die))
            // NB: we pass the same caller arg into it
            iterate_over_callees (&die, sym, recursion_depth, data,
                                  callback, caller, callers);
          break;
        }
    }
  while (dwarf_siblingof (&die, &die) == 0);

  if (free_callers && callers != NULL)
    delete callers;
}


void
dwflpp::collect_srcfiles_matching (string const & pattern,
                                   set<string> & filtered_srcfiles)
{
  assert (module);
  assert (cu);

  size_t nfiles;
  Dwarf_Files *srcfiles;

  // PR 5049: implicit * in front of given path pattern.
  // NB: fnmatch() is used without FNM_PATHNAME.
  string prefixed_pattern = string("*/") + pattern;

  DWARF_ASSERT ("dwarf_getsrcfiles",
                dwarf_getsrcfiles (cu, &srcfiles, &nfiles));
  {
  for (size_t i = 0; i < nfiles; ++i)
    {
      char const * fname = dwarf_filesrc (srcfiles, i, NULL, NULL);
      if (fnmatch (pattern.c_str(), fname, 0) == 0 ||
          fnmatch (prefixed_pattern.c_str(), fname, 0) == 0)
        {
          filtered_srcfiles.insert (fname);
          if (sess.verbose>2)
            clog << _F("selected source file '%s'\n", fname);
        }
    }
  }
}


void
dwflpp::resolve_prologue_endings (func_info_map_t & funcs)
{
  // When a program is compiled with no optimization, GCC does no variable
  // tracking, which means that location info is actually only really valid
  // after the prologue, even though GCC reports it as valid during. So we need
  // to find the prologue ends to get accurate info. This may or may not be the
  // first address that has a source line distinct from the function
  // declaration's.

  assert(module);
  assert(cu);

  size_t nlines = 0;
  Dwarf_Lines *lines = NULL;

  /* trouble cases:
     malloc do_symlink  in init/initramfs.c    tail-recursive/tiny then no-prologue
     sys_get?id         in kernel/timer.c      no-prologue
     sys_exit_group                            tail-recursive
     {do_,}sys_open                            extra-long-prologue (gcc 3.4)
     cpu_to_logical_apicid                     NULL-decl_file
   */

  // Fetch all srcline records, sorted by address. No need to free lines, it's a
  // direct pointer to the CU's cached lines.
  if (dwarf_getsrclines(cu, &lines, &nlines) != 0
      || lines == NULL || nlines == 0)
    {
      if (sess.verbose > 2)
        clog << _F("aborting prologue search: no source lines found for cu '%s'\n",
                   cu_name().c_str());
      return;
    }

  // Dump them into our own array for easier searching. They should already be
  // sorted by addr, but we doublecheck that here. We want to keep the indices
  // between lines and addrs the same.
  vector<Dwarf_Addr> addrs;
  for (size_t i = 0; i < nlines; i++)
    {
      Dwarf_Line* line = dwarf_onesrcline(lines, i);
      Dwarf_Addr addr = DWARF_LINEADDR(line);
      if (!addrs.empty() && addr < addrs.back())
        throw SEMANTIC_ERROR(_("lines from dwarf_getsrclines() not sorted"));
      addrs.push_back(addr);
    }
  // We normally ignore a function's decl_line, since it is associated with the
  // line at which the identifier appears in the declaration, and has no
  // meaningful relation to the lineno associated with the entrypc (which is
  // normally the lineno of '{', which could occur at the same line as the
  // declaration, or lower down).
  //     However, if the CU was compiled using GCC < 4.4, then the decl_line
  // actually represents the lineno of '{' as well, in which case if the lineno
  // associated with the entrypc is != to the decl_line, it means the compiler
  // scraped/optimized off some of the beginning of the function and the safest
  // thing we can do is consider it naked.
  bool consider_decl_line = false;
  {
    string prod, vers;
    if (is_gcc_producer(cu, prod, vers)
     && strverscmp(vers.c_str(), "4.4.0") < 0)
      consider_decl_line = true;
  }

  for(func_info_map_t::iterator it = funcs.begin(); it != funcs.end(); it++)
    {
#if 0 /* someday */
      Dwarf_Addr* bkpts = 0;
      int n = dwarf_entry_breakpoints (& it->die, & bkpts);
      // ...
      free (bkpts);
#endif

      Dwarf_Addr entrypc = it->entrypc;
      Dwarf_Addr highpc; // NB: highpc is exclusive: [entrypc,highpc)
      DWFL_ASSERT ("dwarf_highpc", dwarf_highpc (& it->die,
                                                 & highpc));

      unsigned entrypc_srcline_idx = 0;
      Dwarf_Line *entrypc_srcline = NULL;
      {
        vector<Dwarf_Addr>::const_iterator it_addr =
          lower_bound(addrs.begin(), addrs.end(), entrypc);
        if (it_addr != addrs.end() && *it_addr == entrypc)
          {
            entrypc_srcline_idx = it_addr - addrs.begin();
            entrypc_srcline = dwarf_onesrcline(lines, entrypc_srcline_idx);
          }
      }

      if (!entrypc_srcline)
        {
          if (sess.verbose > 2)
            clog << _F("missing entrypc dwarf line record for function '%s'\n",
                       it->name.to_string().c_str());
          // This is probably an inlined function.  We'll end up using
          // its lowpc as a probe address.
          continue;
        }

      if (entrypc == 0)
        {
          if (sess.verbose > 2)
            clog << _F("null entrypc dwarf line record for function '%s'\n",
                       it->name.to_string().c_str());
          // This is probably an inlined function.  We'll skip this instance;
          // it is messed up. 
          continue;
        }

      if (sess.verbose>2)
        clog << _F("searching for prologue of function '%s' %#" PRIx64 "-%#" PRIx64 
                   "@%s:%d\n", it->name.to_string().c_str(), entrypc, highpc,
                   it->decl_file.to_string().c_str(), it->decl_line);

      // For each function, we look for the prologue-end marker (e.g. clang
      // outputs one). If there is no explicit marker (e.g. GCC does not), we
      // accept a bigger or equal lineno as a prologue end (this catches GCC's
      // 0-line advances).

      // We may have to skip a few because some old compilers plop
      // in dummy line records for longer prologues.  If we go too
      // far (addr >= highpc), we take the previous one.  Or, it may
      // be the first one, if the function had no prologue, and thus
      // the entrypc maps to a statement in the body rather than the
      // declaration.

      int entrypc_srcline_lineno = DWARF_LINENO(entrypc_srcline);
      unsigned postprologue_srcline_idx = entrypc_srcline_idx;
      Dwarf_Line *postprologue_srcline = entrypc_srcline;

      while (postprologue_srcline_idx < nlines)
        {
          postprologue_srcline = dwarf_onesrcline(lines,
                                                  postprologue_srcline_idx);
          Dwarf_Addr lineaddr   = DWARF_LINEADDR(postprologue_srcline);
          const char* linesrc   = DWARF_LINESRC(postprologue_srcline);
          int lineno            = DWARF_LINENO(postprologue_srcline);
          bool lineprologue_end = DWARF_LINEPROLOGUEEND(postprologue_srcline);

          if (sess.verbose>2)
            clog << _F("checking line record %#" PRIx64 "@%s:%d%s\n", lineaddr,
                       linesrc, lineno, lineprologue_end ? " (marked)" : "");

          // have we passed the function?
          if (lineaddr >= highpc)
            break;
          // is there an explicit prologue_end marker?
          if (lineprologue_end)
            break;
          // is it a different file?
          if (it->decl_file != string(linesrc))
            break;
          // OK, it's the same file, but is it a different line?
          if (lineno != entrypc_srcline_lineno)
            break;
          // Same file and line, is this a second line record (e.g. 0-line advance)?
          if (postprologue_srcline_idx != entrypc_srcline_idx)
            break;
          // This is the first iteration. Is decl_line meaningful and is the
          // lineno past the decl_line?
          if (consider_decl_line && lineno != it->decl_line)
            break;

          // Let's try the next srcline.
          postprologue_srcline_idx ++;

        } // loop over srclines


      Dwarf_Addr postprologue_addr = DWARF_LINEADDR(postprologue_srcline);
      if (postprologue_addr >= highpc)
        {
          // pick addr of previous line record
          Dwarf_Line *lr = dwarf_onesrcline(lines, postprologue_srcline_idx-1);
          postprologue_addr = DWARF_LINEADDR(lr);
        }

      it->prologue_end = postprologue_addr;

      if (sess.verbose>2)
        {
          clog << _F("prologue found function '%s'", it->name.to_string().c_str());
          // Add a little classification datum
          //TRANSLATORS: Here we're adding some classification datum (ie Prologue Free)
          if (postprologue_addr == entrypc)
            clog << _(" (naked)");
          //TRANSLATORS: Here we're adding some classification datum (ie we went over)
          if (DWARF_LINEADDR(postprologue_srcline) >= highpc)
            clog << _(" (tail-call?)");
          //TRANSLATORS: Here we're adding some classification datum (ie it was marked)
          if (DWARF_LINEPROLOGUEEND(postprologue_srcline))
            clog << _(" (marked)");

          clog << " = 0x" << hex << postprologue_addr << dec << "\n";
        }

    } // loop over functions
}


bool
dwflpp::function_entrypc (Dwarf_Addr * addr)
{
  assert (function);
  // PR10574: reject 0, which tends to be eliminated COMDAT
  return (dwarf_entrypc (function, addr) == 0 && *addr != 0);
}


bool
dwflpp::die_entrypc (Dwarf_Die * die, Dwarf_Addr * addr)
{
  int rc = 0;
  string lookup_method;

  * addr = 0;

  lookup_method = "dwarf_entrypc";
  rc = dwarf_entrypc (die, addr);

  if (rc)
    {
      lookup_method = "dwarf_ranges";

      Dwarf_Addr base;
      Dwarf_Addr begin;
      Dwarf_Addr end;
      ptrdiff_t offset = dwarf_ranges (die, 0, &base, &begin, &end);
      if (offset < 0) rc = -1;
      else if (offset > 0)
        {
          * addr = begin;
          rc = 0;

          // Now we need to check that there are no more ranges
          // associated with this function, which could conceivably
          // happen if a function is inlined, then pieces of it are
          // split amongst different conditional branches.  It's not
          // obvious which of them to favour.  As a heuristic, we
          // pick the beginning of the first range, and ignore the
          // others (but with a warning).

          unsigned extra = 0;
          while ((offset = dwarf_ranges (die, offset, &base, &begin, &end)) > 0)
            extra ++;
          if (extra)
            lookup_method += _F(", ignored %s more", lex_cast(extra).c_str());
        }
    }

  // PR10574: reject subprograms where the entrypc address turns out
  // to be 0, since they tend to correspond to duplicate-eliminated
  // COMDAT copies of C++ functions.
  if (rc == 0 && *addr == 0)
    {
      lookup_method += _(" (skip comdat)");
      rc = 1;
    }

  if (sess.verbose > 2)
    clog << _F("entry-pc lookup (%s dieoffset: %s) = %#" PRIx64 " (rc %d)", lookup_method.c_str(), 
               lex_cast_hex(dwarf_dieoffset(die)).c_str(), *addr, rc) << endl;

  return (rc == 0);
}


void
dwflpp::function_die (Dwarf_Die *d)
{
  assert (function);
  *d = *function;
}


void
dwflpp::function_file (char const ** c)
{
  assert (function);
  assert (c);
  *c = dwarf_decl_file (function);
  if (*c == NULL)
    {
      // The line table might know.
      Dwarf_Addr pc;
      if (dwarf_lowpc(function, &pc) == 0)
	*c = pc_line (pc, NULL, NULL);

      if (*c == NULL)
	*c = "<unknown source>";
    }
}


void
dwflpp::function_line (int *linep)
{
  assert (function);
  if (dwarf_decl_line (function, linep) != 0)
    {
      // The line table might know.
      Dwarf_Addr pc;
      if (dwarf_lowpc(function, &pc) == 0)
	pc_line (pc, linep, NULL);
    }
}


bool
dwflpp::die_has_pc (Dwarf_Die & die, Dwarf_Addr pc)
{
  int res = dwarf_haspc (&die, pc);
  // dwarf_ranges will return -1 if a function die has no DW_AT_ranges
  // if (res == -1)
  //    DWARF_ASSERT ("dwarf_haspc", res);
  return res == 1;
}


bool
dwflpp::inner_die_containing_pc(Dwarf_Die& scope, Dwarf_Addr addr,
                                Dwarf_Die& result)
{
  result = scope;

  // Sometimes we're in a bad scope to begin with -- just let it be.  This can
  // happen for example if the compiler outputs a label PC that's just outside
  // the lexical scope.  We can't really do anything about that, but variables
  // will probably not be accessible in this case.
  if (!die_has_pc(scope, addr))
    return false;

  Dwarf_Die child, import;
  int rc = dwarf_child(&result, &child);
  while (rc == 0)
    {
      switch (dwarf_tag (&child))
        {
	case DW_TAG_imported_unit:
	  // The children of the imported unit need to be treated as if
	  // they are inserted here. So look inside and set result if
	  // found.
	  if (dwarf_attr_die(&child, DW_AT_import, &import))
	    {
	      Dwarf_Die import_result;
	      if (inner_die_containing_pc(import, addr, import_result))
		{
		  result = import_result;
		  return true;
		}
	    }
	  break;

        // lexical tags to recurse within the same starting scope
        // NB: this intentionally doesn't cross into inlines!
        case DW_TAG_lexical_block:
        case DW_TAG_with_stmt:
        case DW_TAG_catch_block:
        case DW_TAG_try_block:
        case DW_TAG_entry_point:
          if (die_has_pc(child, addr))
            {
              result = child;
              rc = dwarf_child(&result, &child);
              continue;
            }
        }
      rc = dwarf_siblingof(&child, &child);
    }
  return true;
}


void
dwflpp::loc2c_error (void *arg, const char *fmt, ...)
{
  const char *msg = "?";
  char *tmp = NULL;
  int rc;
  va_list ap;
  va_start (ap, fmt);
  rc = vasprintf (& tmp, fmt, ap);
  if (rc < 0)
    msg = "?";
  else
    msg = tmp;
  va_end (ap);

  dwflpp *pp = (dwflpp *) arg;
  semantic_error err(ERR_SRC, msg);
  string what = pp->die_location_as_string(pp->l2c_ctx.die);
  string where = pp->pc_location_as_function_string (pp->l2c_ctx.pc);
  err.details.push_back(what);
  err.details.push_back(where);
  throw err;
}


// This function generates code used for addressing computations of
// target variables.
void
dwflpp::emit_address (struct obstack *pool, Dwarf_Addr address)
{
  int n = dwfl_module_relocations (module);
  DWFL_ASSERT ("dwfl_module_relocations", n >= 0);
  Dwarf_Addr reloc_address = address;
  const char *secname = "";
  if (n > 1)
    {
      int i = dwfl_module_relocate_address (module, &reloc_address);
      DWFL_ASSERT ("dwfl_module_relocate_address", i >= 0);
      secname = dwfl_module_relocation_info (module, i, NULL);
    }

  if (sess.verbose > 2)
    {
      clog << _F("emit dwarf addr %#" PRIx64 " => module %s section %s relocaddr %#" PRIx64,
                 address, module_name.c_str (), (secname ?: "null"),
                 reloc_address) << endl;
    }

  if (n > 0 && !(n == 1 && secname == NULL))
   {
      DWFL_ASSERT ("dwfl_module_relocation_info", secname);
      if (n > 1 || secname[0] != '\0')
        {
          // This gives us the module name, and section name within the
          // module, for a kernel module (or other ET_REL module object).
          obstack_printf (pool, "({ unsigned long addr = 0; ");
          obstack_printf (pool, "addr = _stp_kmodule_relocate (\"%s\",\"%s\",%#" PRIx64 "); ",
                          module_name.c_str(), secname, reloc_address);
          obstack_printf (pool, "addr; })");
        }
      else if (n == 1 && module_name == TOK_KERNEL && secname[0] == '\0')
        {
          // elfutils' way of telling us that this is a relocatable kernel address, which we
          // need to treat the same way here as dwarf_query::add_probe_point does: _stext.
          address -= sess.sym_stext;
          secname = "_stext";
          // Note we "cache" the result here through a static because the
          // kernel will never move after being loaded (unlike modules and
          // user-space dynamic share libraries).
          obstack_printf (pool, "({ static unsigned long addr = 0; ");
          obstack_printf (pool, "if (addr==0) addr = _stp_kmodule_relocate (\"%s\",\"%s\",%#" PRIx64 "); ",
                          module_name.c_str(), secname, address); // PR10000 NB: not reloc_address
          obstack_printf (pool, "addr; })");
        }
      else
        {
          obstack_printf (pool, "/* pragma:vma */");
          obstack_printf (pool, "({ unsigned long addr = 0; ");
          obstack_printf (pool, "addr = _stp_umodule_relocate (\"%s\",%#" PRIx64 ", current); ",
                          resolve_path(module_name.c_str()).c_str(), address);
          obstack_printf (pool, "addr; })");
        }
    }
  else
    obstack_printf (pool, "%#" PRIx64 "UL", address); // assume as constant
}


void
dwflpp::loc2c_emit_address (void *arg, struct obstack *pool,
                            Dwarf_Addr address)
{
  static_cast<dwflpp *>(arg)->emit_address (pool, address);
}


void
dwflpp::get_locals(vector<Dwarf_Die>& scopes, set<string>& locals)
{
  // XXX Shouldn't this be walking up to outer scopes too?

  get_locals_die(scopes[0], locals);
}

void
dwflpp::get_locals_die(Dwarf_Die& die, set<string>& locals)
{
  // Try to get the first child of die.
  Dwarf_Die child, import;
  if (dwarf_child (&die, &child) == 0)
    {
      do
        {
          const char *name;
          // Output each sibling's name (that is a variable or
          // parameter) to 'o'.
          switch (dwarf_tag (&child))
            {
            case DW_TAG_variable:
            case DW_TAG_formal_parameter:
              name = dwarf_diename (&child);
              if (name)
                locals.insert(string("$") + name);
              break;
	    case DW_TAG_imported_unit:
	      // Treat the imported unit children as if they are
	      // children of the given DIE.
	      if (dwarf_attr_die(&child, DW_AT_import, &import))
		get_locals_die (import, locals);
	      break;
            default:
              break;
            }
        }
      while (dwarf_siblingof (&child, &child) == 0);
    }
}


Dwarf_Attribute *
dwflpp::find_variable_and_frame_base (vector<Dwarf_Die>& scopes,
                                      Dwarf_Addr pc,
                                      string const & local,
                                      const target_symbol *e,
                                      Dwarf_Die *vardie,
                                      Dwarf_Attribute *fb_attr_mem)
{
  Dwarf_Die *scope_die = &scopes[0];
  Dwarf_Attribute *fb_attr = NULL;

  assert (cu);

  int declaring_scope = dwarf_getscopevar (&scopes[0], scopes.size(),
                                           local.c_str(),
                                           0, NULL, 0, 0,
                                           vardie);
  if (declaring_scope < 0)
    {
      set<string> locals;
      get_locals(scopes, locals);
      string sugs = levenshtein_suggest(local, locals); // probably not that many, so no limit
      if (pc)
        throw SEMANTIC_ERROR (_F("unable to find local '%s', [man error::dwarf] dieoffset %s in %s, near pc %s %s %s %s (%s)",
                                 local.c_str(),
                                 lex_cast_hex(dwarf_dieoffset(scope_die)).c_str(),
                                 module_name.c_str(),
                                 lex_cast_hex(pc).c_str(),
                                 (scope_die == NULL) ? "" : _("in"),
                                 (dwarf_diename(scope_die) ?: "<unknown>"),
                                 (dwarf_diename(cu) ?: "<unknown>"),
                                 (sugs.empty()
                                  ? (_("<no alternatives>"))
				  : (_("alternatives: ") + sugs + ")")).c_str()),
                              e->tok);
      else
        throw SEMANTIC_ERROR (_F("unable to find global '%s', [man error::dwarf] dieoffset %s in %s, %s %s %s (%s)",
                                 local.c_str(),
                                 lex_cast_hex(dwarf_dieoffset(scope_die)).c_str(),
                                 module_name.c_str(),
                                 (scope_die == NULL) ? "" : _("in"),
                                 (dwarf_diename(scope_die) ?: "<unknown>"),
                                 cu_name().c_str(),
                                 (sugs.empty()
                                  ? (_("<no alternatives>"))
				  : (_("alternatives: ") + sugs + ")")).c_str()),
                              e->tok);
    }

  /* Some GCC versions would output duplicate external variables, one
     without a location attribute. If so, try to find the other if it
     exists in the same scope. See GCC PR51410.  */
  Dwarf_Attribute attr_mem;
  if (dwarf_attr_integrate (vardie, DW_AT_const_value, &attr_mem) == NULL
      && dwarf_attr_integrate (vardie, DW_AT_location, &attr_mem) == NULL
      && dwarf_attr_integrate (vardie, DW_AT_external, &attr_mem) != NULL
      && dwarf_tag(&scopes[declaring_scope]) == DW_TAG_compile_unit)
    {
      Dwarf_Die orig_vardie = *vardie;
      bool alt_found = false;
      if (dwarf_child(&scopes[declaring_scope], vardie) == 0)
	do
	  {
	    // Note, not handling DW_TAG_imported_unit, assuming GCC
	    // version is recent enough to not need this workaround if
	    // we would see an imported unit.
	    if (dwarf_tag (vardie) == DW_TAG_variable
		&& strcmp (dwarf_diename (vardie), local.c_str ()) == 0
		&& (dwarf_attr_integrate (vardie, DW_AT_external, &attr_mem)
		    != NULL)
		&& ((dwarf_attr_integrate (vardie, DW_AT_const_value, &attr_mem)
		     != NULL)
		    || (dwarf_attr_integrate (vardie, DW_AT_location, &attr_mem)
			!= NULL)))
	      alt_found = true;
	  }
	while (!alt_found && dwarf_siblingof(vardie, vardie) == 0);

      if (! alt_found)
	*vardie = orig_vardie;
    }

  // Global vars don't need (cannot use) frame base in location descriptor.
  if (pc == 0)
    return NULL;

  /* We start out walking the "lexical scopes" as returned by
   * as returned by dwarf_getscopes for the address, starting with the
   * declaring_scope that the variable was found in.
   */
  vector<Dwarf_Die> physcopes, *fbscopes = &scopes;
  for (size_t inner = declaring_scope;
       inner < fbscopes->size() && fb_attr == NULL;
       ++inner)
    {
      Dwarf_Die& scope = (*fbscopes)[inner];
      switch (dwarf_tag (&scope))
        {
        default:
          continue;
        case DW_TAG_subprogram:
        case DW_TAG_entry_point:
          fb_attr = dwarf_attr_integrate (&scope,
                                          DW_AT_frame_base,
                                          fb_attr_mem);
          break;
        case DW_TAG_inlined_subroutine:
          /* Unless we already are going through the "pyshical die tree",
           * we now need to start walking the die tree where this
           * subroutine is inlined to find the appropriate frame base. */
           if (declaring_scope != -1)
             {
               physcopes = getscopes_die(&scope);
               if (physcopes.empty())
                 throw SEMANTIC_ERROR (_F("unable to get die scopes for '%s' in an inlined subroutine",
                                          local.c_str()), e->tok);
               fbscopes = &physcopes;
               inner = 0; // zero is current scope, for look will increase.
               declaring_scope = -1;
             }
          break;
        }
    }

  return fb_attr;
}

/* Returns a human readable string with suggested locations where a
   DIE attribute is valid.  */
static string
suggested_locations_string(Dwarf_Attribute *attr)
{
  string locsstr;
  if (attr == NULL)
    locsstr = "<no alternatives for NULL attribute>";
  else
    {
#if _ELFUTILS_PREREQ (0, 158)
      Dwarf_Op *expr;
      size_t exprlen;
      Dwarf_Addr base, start, end;
      ptrdiff_t off = 0;

      off = dwarf_getlocations (attr, off, &base,
				&start, &end,
				&expr, &exprlen);
      if (off > 0)
	{
	  locsstr = _("alternative locations: ");

	  while (off > 0)
            {
	      locsstr += "[";
	      locsstr += lex_cast_hex(start);
	      locsstr += ",";
	      locsstr += lex_cast_hex(end);
	      locsstr += "]";

	      off = dwarf_getlocations (attr, off, &base,
					&start, &end,
					&expr, &exprlen);
	      if (off > 0)
		locsstr += ", ";
	    }
	}
      else if (off == 0)
	locsstr = _("<no alternative locations>");
      else
	locsstr = _F("<error getting alternative locations: %s>",
		     dwarf_errmsg(-1));
#else
      locsstr = "<cannot suggest any alternative locations, elfutils too old>";
#endif /* _ELFUTILS_PREREQ (0, 158) */
    }

  return locsstr;
}

/* Produce a human readable name for a DIE. */
static string
die_name_string (Dwarf_Die *die)
{
  string res;
  const char *name = dwarf_linkage_name(die);
  if (name == NULL)
    name = dwarf_diename (die);

  size_t demangle_buffer_len = 0;
  char *demangle_buffer = NULL;
  if (name != NULL && name[0] == '_' && name[1] == 'Z')
    {
      int status = -1;
      char *dsymname = abi::__cxa_demangle (name, demangle_buffer,
					    &demangle_buffer_len, &status);
      if (status == 0)
	name = demangle_buffer = dsymname;
    }
  if (name != NULL)
    res = name;
  else
    res = _("<unknown");
  free (demangle_buffer);

  return res;
}

/* Returns a source file name, line and column information based on the
   pc and the current cu.  */
const char *
dwflpp::pc_line (Dwarf_Addr pc, int *lineno, int *colno)
{
  if (pc != 0)
    {
      Dwarf_Line *line = dwarf_getsrc_die (cu, pc);
      if (line != NULL)
	{
	  if (lineno != NULL)
	    dwarf_lineno (line, lineno);
	  if (colno != NULL)
	    dwarf_linecol (line, colno);
	  return dwarf_linesrc (line, NULL, NULL);
	}
    }

  return NULL;
}

/* Returns a source line and column string based on the inlined DIE
   or based on the pc if DIE is NULL. */
string
dwflpp::pc_die_line_string (Dwarf_Addr pc, Dwarf_Die *die)
{
  string linestr;

  int lineno, col;
  const char *src = NULL;
  lineno = col = -1;

  if (die == NULL)
    src = pc_line (pc, &lineno, &col);
  else
    {
      Dwarf_Files *files;
      if (dwarf_getsrcfiles (cu, &files, NULL) == 0)
	{
	  Dwarf_Attribute attr;
	  Dwarf_Word val;
	  if (dwarf_formudata (dwarf_attr (die, DW_AT_call_file, &attr),
			       &val) == 0)
	    {
	      src = dwarf_filesrc (files, val, NULL, NULL);
	      if (dwarf_formudata (dwarf_attr (die, DW_AT_call_line,
					       &attr), &val) == 0)
		{
		  lineno = val;
		  if (dwarf_formudata (dwarf_attr (die, DW_AT_call_column,
						   &attr), &val) == 0)
		    col = val;
		}
	    }
	}
      else
	src = pc_line (pc, &lineno, &col);
    }

  if (src != NULL)
    {
      linestr += src;
      if (lineno > 0)
	{
	  linestr += ":" + lex_cast(lineno);
	  if (col > 0)
	    linestr += ":" + lex_cast(col);
	}
    }
  else
    linestr += _("unknown source");

  return linestr;
}

/* Returns a human readable DIE offset for use in error messages.
   Includes DIE offset and DWARF file used. */
string
dwflpp::die_location_as_string(Dwarf_Die *die)
{
  string locstr;

  /* DIE offset */
  locstr += _("dieoffset: ");
  locstr += lex_cast_hex(dwarf_dieoffset(die));

  /* DWARF file */
  const char *mainfile, *debugfile;
  locstr += _(" from ");
  if (dwfl_module_info (module, NULL, NULL, NULL, NULL, NULL, &mainfile,
			&debugfile) == NULL
      || (mainfile == NULL && debugfile == NULL))
    {
      locstr += _("unknown debug file for ");
      locstr += module_name;
    }
  else
    {
      if (debugfile != NULL)
	locstr += debugfile;
      else
	locstr += mainfile;
    }

  return locstr;
}

/* Returns a human readable (inlined) function and source file/line location
   for a pc location.  */
string
dwflpp::pc_location_as_function_string(Dwarf_Addr pc)
{
  string locstr;
  locstr = _("function: ");

  /* Find the first function-like DIE with a name in scope.  */
  Dwarf_Die funcdie_mem;
  Dwarf_Die *funcdie = NULL;
  string funcname = "";
  Dwarf_Die *scopes = NULL;
  int nscopes = dwarf_getscopes (cu, pc, &scopes);
  for (int i = 0; funcname == "" && i < nscopes; i++)
    {
      Dwarf_Die *scope = &scopes[i];
      int tag = dwarf_tag (scope);
      if (tag == DW_TAG_subprogram
	  || tag == DW_TAG_inlined_subroutine
	  || tag == DW_TAG_entry_point)
	funcname = die_name_string (scope);
      if (funcname != "")
	{
	  funcdie_mem = *scope;
	  funcdie = &funcdie_mem;
	}
    }
  free (scopes);

  /* source location */
  if (funcname == "")
    locstr += _("<unknown> at ") + pc_die_line_string (pc, NULL);
  else
    {
      int nscopes = dwarf_getscopes_die (funcdie, &scopes);
      if (nscopes > 0)
	{
	  /* scopes[0] == funcdie, the lowest level, for which we already have
	     the name.  This is the actual source location where it
	     happened.  */
	  locstr += funcname;
	  locstr +=  _(" at ");
	  locstr += pc_die_line_string (pc, NULL);

	  /* last_scope is the source location where the next inlined frame/function
	     call was done. */
	  Dwarf_Die *last_scope = &scopes[0];
	  for (int i = 1; i < nscopes; i++)
	    {
	      Dwarf_Die *scope = &scopes[i];
	      int tag = dwarf_tag (scope);
	      if (tag != DW_TAG_inlined_subroutine
		  && tag != DW_TAG_entry_point
		  && tag != DW_TAG_subprogram)
		continue;

	      locstr += _(" inlined by ");
	      locstr += die_name_string (scope);
	      locstr += _(" at ");
	      locstr += pc_die_line_string (pc, last_scope);

	      /* Found the "top-level" in which everything was inlined.  */
	      if (tag == DW_TAG_subprogram)
		break;

	      last_scope = scope;
	    }
	}
      else
	{
	  locstr += funcname;
	  locstr += _(" at ");
	  locstr += pc_die_line_string (pc, NULL);
	}
      free (scopes);
    }

  return locstr;
}

struct location *
dwflpp::translate_location(struct obstack *pool,
                           Dwarf_Attribute *attr, Dwarf_Die *die,
			   Dwarf_Addr pc,
                           Dwarf_Attribute *fb_attr,
                           struct location **tail,
                           const target_symbol *e)
{

  /* DW_AT_data_member_location, can be either constant offsets
     (struct member fields), or full blown location expressions.  */

  /* There is no location expression, but a constant value instead.  */
  if (dwarf_whatattr (attr) == DW_AT_const_value)
    {
      l2c_ctx.pc = pc;
      l2c_ctx.die = die;
      *tail = c_translate_constant (pool, &loc2c_error, this,
				    &loc2c_emit_address, 0, pc, attr);
      return *tail;
    }

  Dwarf_Op *expr;
  size_t len;

  /* PR9768: formerly, we added pc+module_bias here.  However, that bias value
     is not present in the pc value by the time we get it, so adding it would
     result in false negatives of variable reachibility.  In other instances
     further below, the c_translate_FOO functions, the module_bias value used
     to be passed in, but instead should now be zero for the same reason. */

 retry:
  switch (dwarf_getlocation_addr (attr, pc /*+ module_bias*/, &expr, &len, 1))
    {
    case 1:			/* Should always happen.  */
      if (len > 0)
        break;
      /* Fall through.  */

    case 0:			/* Shouldn't happen.... but can, e.g. due to PR15123. */
      {
        Dwarf_Addr pc2 = pr15123_retry_addr (pc, die);
        if (pc2 != 0) {
          pc = pc2;
          goto retry;
        }
      }

      /* FALLTHROUGH */
      {
	string msg = _F("not accessible at this address (pc: %s) [man error::dwarf]", lex_cast_hex(pc).c_str());
	semantic_error err(ERR_SRC, msg, e->tok);
	err.details.push_back(die_location_as_string(die));
	err.details.push_back(pc_location_as_function_string(pc));
	err.details.push_back(suggested_locations_string(attr));
	throw err;
      }

    default:			/* Shouldn't happen.  */
    case -1:
      {
	string msg = _F("dwarf_getlocation_addr failed at this address (pc: %s) [man error::dwarf]", lex_cast_hex(pc).c_str());
	semantic_error err(ERR_SRC, msg, e->tok);
	string dwarf_err = _F("dwarf_error: %s", dwarf_errmsg(-1));
	err.details.push_back(dwarf_err);
	err.details.push_back(die_location_as_string(die));
	err.details.push_back(pc_location_as_function_string(pc));
	err.details.push_back(suggested_locations_string(attr));
	throw err;
      }
    }

  Dwarf_Op *cfa_ops;
  // pc is in the dw address space of the current module, which is what
  // c_translate_location expects. get_cfa_ops wants the global dwfl address.
  // cfa_ops only make sense for locals.
  if (pc)
    {
      Dwarf_Addr addr = pc + module_bias;
      cfa_ops = get_cfa_ops (addr);
    }
  else
    cfa_ops = NULL;

  l2c_ctx.pc = pc;
  l2c_ctx.die = die;
  return c_translate_location (pool, &loc2c_error, this,
                               &loc2c_emit_address,
                               1, 0 /* PR9768 */,
                               pc, attr, expr, len, tail, fb_attr, cfa_ops);
}


void
dwflpp::get_members(Dwarf_Die *typedie, set<string>& members, set<string> &dupes)
{
  const int typetag = dwarf_tag (typedie);

  /* compile and partial unit included for recursion through
     imported_unit below. */
  if (typetag != DW_TAG_structure_type &&
      typetag != DW_TAG_class_type &&
      typetag != DW_TAG_union_type &&
      typetag != DW_TAG_compile_unit &&
      typetag != DW_TAG_partial_unit)
    {
      throw SEMANTIC_ERROR(_F("Type %s isn't a struct/class/union",
                              dwarf_type_name(typedie).c_str()));
    }

  // Try to get the first child of vardie.
  Dwarf_Die die_mem, import;
  Dwarf_Die *die = &die_mem;
  switch (dwarf_child (typedie, die))
    {
    case 1:				// No children.
      throw SEMANTIC_ERROR(_F("Type %s is empty", dwarf_type_name(typedie).c_str()));

    case -1:				// Error.
    default:				// Shouldn't happen.
      throw SEMANTIC_ERROR(_F("Type %s: %s", dwarf_type_name(typedie).c_str(),
                                             dwarf_errmsg(-1)));

    case 0:				// Success.
      break;
    }

  // Add each sibling's name to members set
  do
    {
      int tag = dwarf_tag(die);

      /* The children of an imported_unit should be treated as members too. */
      if (tag == DW_TAG_imported_unit
          && dwarf_attr_die(die, DW_AT_import, &import))
        get_members(&import, members, dupes);

      if (tag != DW_TAG_member && tag != DW_TAG_inheritance)
        continue;

      const char *member = dwarf_diename (die) ;

      if ( tag == DW_TAG_member && member != NULL )
        {
          // Only output if this is new, to avoid inheritance dupes.
          if (dupes.insert(member).second)
            members.insert(member);
        }
      else
        {
          Dwarf_Die temp_die;
          if (!dwarf_attr_die (die, DW_AT_type, &temp_die))
            {
              string source = dwarf_decl_file(die) ?: "<unknown source>";
              int line = -1;
              dwarf_decl_line(die, &line);
              throw SEMANTIC_ERROR(_F("Couldn't obtain type attribute for anonymous "
                                      "member at %s:%d", source.c_str(), line));
            }

          get_members(&temp_die, members, dupes);
        }

    }
  while (dwarf_siblingof (die, die) == 0);
}


bool
dwflpp::find_struct_member(const target_symbol::component& c,
                           Dwarf_Die *parentdie,
                           Dwarf_Die *memberdie,
                           vector<Dwarf_Die>& dies,
                           vector<Dwarf_Attribute>& locs)
{
  Dwarf_Attribute attr;
  Dwarf_Die die;

  /* With inheritance, a subclass may mask member names of parent classes, so
   * our search among the inheritance tree must be breadth-first rather than
   * depth-first (recursive).  The parentdie is still our starting point. */
  deque<Dwarf_Die> inheritees(1, *parentdie);
  for (; !inheritees.empty(); inheritees.pop_front())
    {
      switch (dwarf_child (&inheritees.front(), &die))
        {
        case 0:		/* First child found.  */
          break;
        case 1:		/* No children.  */
          continue;
        case -1:	/* Error.  */
        default:	/* Shouldn't happen */
          throw SEMANTIC_ERROR (dwarf_type_name(&inheritees.front()) + ": "
                                + string (dwarf_errmsg (-1)),
                                c.tok);
        }

      do
        {
          int tag = dwarf_tag(&die);
          /* recurse into imported units as if they are anonymoust structs */
          Dwarf_Die import;
          if (tag == DW_TAG_imported_unit
              && dwarf_attr_die(&die, DW_AT_import, &import)
              && find_struct_member(c, &import, memberdie, dies, locs))
            goto success;

          if (tag != DW_TAG_member && tag != DW_TAG_inheritance)
            continue;

          const char *name = dwarf_diename(&die);
          if (tag == DW_TAG_inheritance)
            {
              /* Remember inheritee for breadth-first search. */
              Dwarf_Die inheritee;
              if (dwarf_attr_die (&die, DW_AT_type, &inheritee))
                inheritees.push_back(inheritee);
            }
          else if (name == NULL)
            {
              /* Need to recurse for anonymous structs/unions. */
              Dwarf_Die subdie;
              if (dwarf_attr_die (&die, DW_AT_type, &subdie) &&
                  find_struct_member(c, &subdie, memberdie, dies, locs))
                goto success;
            }
          else if (name == c.member)
            {
              *memberdie = die;
              goto success;
            }
        }
      while (dwarf_siblingof (&die, &die) == 0);
    }

  return false;

success:
  /* As we unwind the recursion, we need to build the chain of
   * locations that got to the final answer. */
  if (dwarf_attr_integrate (&die, DW_AT_data_member_location, &attr))
    {
      dies.insert(dies.begin(), die);
      locs.insert(locs.begin(), attr);
    }

  /* Union members don't usually have a location,
   * but just use the containing union's location.  */
  else if (dwarf_tag(parentdie) != DW_TAG_union_type)
    throw SEMANTIC_ERROR (_F("no location for field '%s':%s",
                             c.member.c_str(), dwarf_errmsg(-1)), c.tok);

  return true;
}


static inline void
dwarf_die_type (Dwarf_Die *die, Dwarf_Die *typedie_mem, const token *tok=NULL)
{
  if (!dwarf_attr_die (die, DW_AT_type, typedie_mem))
    throw SEMANTIC_ERROR (_F("cannot get type of field: %s", dwarf_errmsg(-1)), tok);
}


void
dwflpp::translate_components(struct obstack *pool,
                             struct location **tail,
                             Dwarf_Addr pc,
                             const target_symbol *e,
                             Dwarf_Die *vardie,
                             Dwarf_Die *typedie,
                             unsigned first)
{
  unsigned i = first;
  while (i < e->components.size())
    {
      const target_symbol::component& c = e->components[i];

      /* XXX: This would be desirable, but we don't get the target_symbol token,
         and printing that gives us the file:line number too early anyway. */
#if 0
      // Emit a marker to note which field is being access-attempted, to give
      // better error messages if deref() fails.
      string piece = string(...target_symbol token...) + string ("#") + lex_cast(components[i].second);
      obstack_printf (pool, "c->last_stmt = %s;", lex_cast_qstring(piece).c_str());
#endif

      switch (dwarf_tag (typedie))
        {
        case DW_TAG_typedef:
        case DW_TAG_const_type:
        case DW_TAG_volatile_type:
        case DW_TAG_restrict_type:
          /* Just iterate on the referent type.  */
          dwarf_die_type (typedie, typedie, c.tok);
          break;

        case DW_TAG_reference_type:
        case DW_TAG_rvalue_reference_type:
          if (pool)
	    {
	      l2c_ctx.die = typedie;
	      c_translate_pointer (pool, 1, 0 /* PR9768*/, typedie, tail);
	    }
          dwarf_die_type (typedie, typedie, c.tok);
          break;

        case DW_TAG_pointer_type:
          /* A pointer with no type is a void* -- can't dereference it. */
          if (!dwarf_hasattr_integrate (typedie, DW_AT_type))
            throw SEMANTIC_ERROR (_F("invalid access '%s' vs '%s'", lex_cast(c).c_str(),
                                     dwarf_type_name(typedie).c_str()), c.tok);

          if (pool)
	    {
	      l2c_ctx.die = typedie;
	      c_translate_pointer (pool, 1, 0 /* PR9768*/, typedie, tail);
	    }
          if (c.type != target_symbol::comp_literal_array_index &&
              c.type != target_symbol::comp_expression_array_index)
            {
              dwarf_die_type (typedie, typedie, c.tok);
              break;
            }
          /* else fall through as an array access */

        case DW_TAG_array_type:
          if (c.type == target_symbol::comp_literal_array_index)
            {
              if (pool)
		{
		  l2c_ctx.die = typedie;
                  c_translate_array (pool, 1, 0 /* PR9768 */, typedie, tail,
				     NULL, c.num_index);
		}
            }
          else if (c.type == target_symbol::comp_expression_array_index)
            {
              string index = "STAP_ARG_index" + lex_cast(i);
              if (pool)
		{
		  l2c_ctx.die = typedie;
                  c_translate_array (pool, 1, 0 /* PR9768 */, typedie, tail,
                                     index.c_str(), 0);
		}
            }
          else
            throw SEMANTIC_ERROR (_F("invalid access '%s' for array type",
                                     lex_cast(c).c_str()), c.tok);

          dwarf_die_type (typedie, typedie, c.tok);
          *vardie = *typedie;
          ++i;
          break;

        case DW_TAG_structure_type:
        case DW_TAG_union_type:
        case DW_TAG_class_type:
          if (c.type != target_symbol::comp_struct_member)
            throw SEMANTIC_ERROR (_F("invalid access '%s' for %s",
                                     lex_cast(c).c_str(), dwarf_type_name(typedie).c_str()));

          if (dwarf_hasattr(typedie, DW_AT_declaration))
            {
              Dwarf_Die *tmpdie = declaration_resolve(typedie);
              if (tmpdie == NULL)
                throw SEMANTIC_ERROR (_F("unresolved %s", dwarf_type_name(typedie).c_str()), c.tok);
              *typedie = *tmpdie;
            }

            {
              vector<Dwarf_Die> dies;
              vector<Dwarf_Attribute> locs;
              if (!find_struct_member(c, typedie, vardie, dies, locs))
                {
                  /* Add a file:line hint for anonymous types */
                  string source;
                  if (!dwarf_hasattr_integrate(typedie, DW_AT_name))
                    {
                      int line;
                      const char *file = dwarf_decl_file(typedie);
                      if (file && dwarf_decl_line(typedie, &line) == 0)
                        source = " (" + string(file) + ":"
                                 + lex_cast(line) + ")";
                    }

                  set<string> members, member_dupes;
                  get_members(typedie, members, member_dupes);
                  string sugs = levenshtein_suggest(c.member, members);
                  if (!sugs.empty())
                    sugs = " (alternatives: " + sugs + ")";
                  throw SEMANTIC_ERROR(_F("unable to find member '%s' for %s%s%s", c.member.c_str(),
                                          dwarf_type_name(typedie).c_str(), source.c_str(),
                                          sugs.c_str()), c.tok);
                }

              for (unsigned j = 0; j < locs.size(); ++j)
                if (pool)
                  translate_location (pool, &locs[j], &dies[j],
                                      pc, NULL, tail, e);
            }

          dwarf_die_type (vardie, typedie, c.tok);
          ++i;
          break;

        case DW_TAG_enumeration_type:
        case DW_TAG_base_type:
          throw SEMANTIC_ERROR (_F("invalid access '%s' vs. %s", lex_cast(c).c_str(),
                                   dwarf_type_name(typedie).c_str()), c.tok);
          break;

        case -1:
          throw SEMANTIC_ERROR (_F("cannot find type: %s", dwarf_errmsg(-1)), c.tok);
          break;

        default:
          throw SEMANTIC_ERROR (_F("%s: unexpected type tag %s", dwarf_type_name(typedie).c_str(),
                                   lex_cast(dwarf_tag(typedie)).c_str()), c.tok);
          break;
        }
    }
}


void
dwflpp::resolve_unqualified_inner_typedie (Dwarf_Die *typedie,
                                           Dwarf_Die *innerdie,
                                           const target_symbol *e)
{
  int typetag = dwarf_tag (typedie);
  *innerdie = *typedie;
  while (typetag == DW_TAG_typedef ||
         typetag == DW_TAG_const_type ||
         typetag == DW_TAG_volatile_type ||
         typetag == DW_TAG_restrict_type)
    {
      if (!dwarf_attr_die (innerdie, DW_AT_type, innerdie))
        throw SEMANTIC_ERROR (_F("cannot get type of pointee: %s", dwarf_errmsg(-1)), e->tok);
      typetag = dwarf_tag (innerdie);
    }
}


void
dwflpp::translate_final_fetch_or_store (struct obstack *pool,
                                        struct location **tail,
                                        Dwarf_Addr /*module_bias*/,
                                        Dwarf_Die *vardie,
                                        Dwarf_Die *start_typedie,
                                        bool lvalue,
                                        const target_symbol *e,
                                        string &,
                                        string & postlude,
                                        Dwarf_Die *typedie)
{
  /* First boil away any qualifiers associated with the type DIE of
     the final location to be accessed.  */

  resolve_unqualified_inner_typedie (start_typedie, typedie, e);

  /* If we're looking for an address, then we can just provide what
     we computed to this point, without using a fetch/store. */
  if (e->addressof)
    {
      if (lvalue)
        throw SEMANTIC_ERROR (_("cannot write to member address"), e->tok);

      if (dwarf_hasattr_integrate (vardie, DW_AT_bit_offset))
        throw SEMANTIC_ERROR (_("cannot take address of bit-field"), e->tok);

      l2c_ctx.die = vardie;
      c_translate_addressof (pool, 1, 0, vardie, typedie, tail, "STAP_RETVALUE");
      return;
    }

  /* Then switch behavior depending on the type of fetch/store we
     want, and the type and pointer-ness of the final location. */

  int typetag = dwarf_tag (typedie);
  switch (typetag)
    {
    default:
      throw SEMANTIC_ERROR (_F("unsupported type tag %s for %s", lex_cast(typetag).c_str(),
                               dwarf_type_name(typedie).c_str()), e->tok);
      break;

    case DW_TAG_structure_type:
    case DW_TAG_class_type:
    case DW_TAG_union_type:
      throw SEMANTIC_ERROR (_F("'%s' is being accessed instead of a member",
                               dwarf_type_name(typedie).c_str()), e->tok);
      break;

    case DW_TAG_base_type:

      // Reject types we can't handle in systemtap
      {
        Dwarf_Attribute encoding_attr;
        Dwarf_Word encoding = (Dwarf_Word) -1;
        dwarf_formudata (dwarf_attr_integrate (typedie, DW_AT_encoding, &encoding_attr),
                         & encoding);
        if (encoding == (Dwarf_Word) -1)
          {
            // clog << "bad type1 " << encoding << " diestr" << endl;
            throw SEMANTIC_ERROR (_F("unsupported type (mystery encoding %s for %s", lex_cast(encoding).c_str(),
                                     dwarf_type_name(typedie).c_str()), e->tok);
          }

        if (encoding == DW_ATE_float
            || encoding == DW_ATE_complex_float
            /* XXX || many others? */)
          {
            // clog << "bad type " << encoding << " diestr" << endl;
            throw SEMANTIC_ERROR (_F("unsupported type (encoding %s) for %s", lex_cast(encoding).c_str(),
                                     dwarf_type_name(typedie).c_str()), e->tok);
          }
      }
      // Fallthrough. enumeration_types are always scalar.
    case DW_TAG_enumeration_type:

      l2c_ctx.die = vardie;
      if (lvalue)
        c_translate_store (pool, 1, 0 /* PR9768 */, vardie, typedie, tail,
                           "STAP_ARG_value");
      else
        c_translate_fetch (pool, 1, 0 /* PR9768 */, vardie, typedie, tail,
                           "STAP_RETVALUE");
      break;

    case DW_TAG_array_type:
    case DW_TAG_pointer_type:
    case DW_TAG_reference_type:
    case DW_TAG_rvalue_reference_type:

        if (lvalue)
          {
            if (typetag == DW_TAG_array_type)
              throw SEMANTIC_ERROR (_("cannot write to array address"), e->tok);
            if (typetag == DW_TAG_reference_type ||
                typetag == DW_TAG_rvalue_reference_type)
              throw SEMANTIC_ERROR (_("cannot write to reference"), e->tok);
            assert (typetag == DW_TAG_pointer_type);
	    l2c_ctx.die = typedie;
            c_translate_pointer_store (pool, 1, 0 /* PR9768 */, typedie, tail,
                                       "STAP_ARG_value");
          }
        else
          {
            // We have the pointer: cast it to an integral type via &(*(...))

            // NB: per bug #1187, at one point char*-like types were
            // automagically converted here to systemtap string values.
            // For several reasons, this was taken back out, leaving
            // pointer-to-string "conversion" (copying) to tapset functions.

	    l2c_ctx.die = typedie;
            if (typetag == DW_TAG_array_type)
              c_translate_array (pool, 1, 0 /* PR9768 */, typedie, tail, NULL, 0);
            else
              c_translate_pointer (pool, 1, 0 /* PR9768 */, typedie, tail);
            c_translate_addressof (pool, 1, 0 /* PR9768 */, NULL, NULL, tail,
                                   "STAP_RETVALUE");
          }
      break;
    }

  if (lvalue)
    postlude += "  STAP_RETVALUE = STAP_ARG_value;\n";
}


string
dwflpp::express_as_string (string prelude,
                           string postlude,
                           struct location *head)
{
  size_t bufsz = 0;
  char *buf = 0; // NB: it would leak to pre-allocate a buffer here
  FILE *memstream = open_memstream (&buf, &bufsz);
  assert(memstream);

  fprintf(memstream, "{\n");
  fprintf(memstream, "%s", prelude.c_str());

  unsigned int stack_depth;
  bool deref = c_emit_location (memstream, head, 1, &stack_depth);

  // Ensure that DWARF keeps loc2c to a "reasonable" stack size
  // 32 intptr_t leads to max 256 bytes on the stack
  if (stack_depth > 32)
    throw SEMANTIC_ERROR("oversized DWARF stack");

  fprintf(memstream, "%s", postlude.c_str());
  fprintf(memstream, "  goto out;\n");

  // dummy use of deref_fault label, to disable warning if deref() not used
  fprintf(memstream, "if (0) goto deref_fault;\n");

  // XXX: deref flag not reliable; emit fault label unconditionally
  (void) deref;
  fprintf(memstream,
          "deref_fault:\n"
          "  goto out;\n");
  fprintf(memstream, "}\n");

  fclose (memstream);
  string result(buf);
  free (buf);
  return result;
}

Dwarf_Addr
dwflpp::vardie_from_symtable (Dwarf_Die *vardie, Dwarf_Addr *addr)
{
  const char *name = dwarf_linkage_name (vardie) ?: dwarf_diename (vardie);

  if (sess.verbose > 2)
    clog << _F("finding symtable address for %s\n", name);

  *addr = 0;
  int syms = dwfl_module_getsymtab (module);
  DWFL_ASSERT (_("Getting symbols"), syms >= 0);

  for (int i = 0; *addr == 0 && i < syms; i++)
    {
      GElf_Sym sym;
      GElf_Word shndxp;
      const char *symname = dwfl_module_getsym(module, i, &sym, &shndxp);
      if (symname
	  && ! strcmp (name, symname)
	  && sym.st_shndx != SHN_UNDEF
	  && (GELF_ST_TYPE (sym.st_info) == STT_NOTYPE // PR13284
	      || GELF_ST_TYPE (sym.st_info) == STT_OBJECT))
	*addr = sym.st_value;
    }

  // Don't relocate for the kernel, or kernel modules we handle those
  // specially in emit_address.
  if (dwfl_module_relocations (module) == 1 && module_name != TOK_KERNEL)
    dwfl_module_relocate_address (module, addr);

  if (sess.verbose > 2)
    clog << _F("found %s @%#" PRIx64 "\n", name, *addr);

  return *addr;
}

string
dwflpp::literal_stmt_for_local (vector<Dwarf_Die>& scopes,
                                Dwarf_Addr pc,
                                string const & local,
                                const target_symbol *e,
                                bool lvalue,
                                Dwarf_Die *die_mem)
{
  Dwarf_Die vardie;
  Dwarf_Attribute fb_attr_mem, *fb_attr = NULL;

  // NB: when addr_loc is used for a synthesized DW_OP_addr below, then it
  // needs to remain valid until express_as_string() has finished with it.
  Dwarf_Op addr_loc;

  fb_attr = find_variable_and_frame_base (scopes, pc, local, e,
                                          &vardie, &fb_attr_mem);

  if (sess.verbose>2)
    {
      if (pc)
        clog << _F("finding location for local '%s' near address %#" PRIx64
                   ", module bias %#" PRIx64 "\n", local.c_str(), pc,
	           module_bias);
      else
        clog << _F("finding location for global '%s' in CU '%s'\n",
		   local.c_str(), cu_name().c_str());
    }

  struct obstack pool;
  obstack_tracker p (&pool);

  struct location *tail = NULL;

  /* Given $foo->bar->baz[NN], translate the location of foo. */

  struct location *head;

  Dwarf_Attribute attr_mem;
  if (dwarf_attr_integrate (&vardie, DW_AT_const_value, &attr_mem) == NULL
      && dwarf_attr_integrate (&vardie, DW_AT_location, &attr_mem) == NULL)
    {
      memset(&addr_loc, 0, sizeof(Dwarf_Op));
      addr_loc.atom = DW_OP_addr;
      // If it is an external variable try the symbol table. PR10622.
      if (dwarf_attr_integrate (&vardie, DW_AT_external, &attr_mem) != NULL
	  && vardie_from_symtable (&vardie, &addr_loc.number) != 0)
	{
	  l2c_ctx.pc = pc;
	  l2c_ctx.die = &vardie;
	  head = c_translate_location (&pool, &loc2c_error, this,
				       &loc2c_emit_address,
				       1, 0, pc,
				       NULL, &addr_loc, 1, &tail, NULL, NULL);
	}
      else
	{
	  string msg = _F("failed to retrieve location attribute for '%s' [man error::dwarf]", local.c_str());
	  semantic_error err(ERR_SRC, msg, e->tok);
	  err.details.push_back(die_location_as_string(&vardie));
	  err.details.push_back(pc_location_as_function_string(pc));
	  throw err;
	}
    }
  else
    head = translate_location (&pool, &attr_mem, &vardie, pc, fb_attr, &tail, e);

  /* Translate the ->bar->baz[NN] parts. */

  Dwarf_Die typedie;
  if (dwarf_attr_die (&vardie, DW_AT_type, &typedie) == NULL)
    {
      string msg = _F("failed to retrieve type attribute for '%s' [man error::dwarf]", local.c_str());
      semantic_error err(ERR_SRC, msg, e->tok);
      err.details.push_back(die_location_as_string(&vardie));
      err.details.push_back(pc_location_as_function_string(pc));
      throw err;
    }

  translate_components (&pool, &tail, pc, e, &vardie, &typedie);

  /* Translate the assignment part, either
     x = $foo->bar->baz[NN]
     or
     $foo->bar->baz[NN] = x
  */

  string prelude, postlude;
  translate_final_fetch_or_store (&pool, &tail, module_bias,
                                  &vardie, &typedie, lvalue, e,
                                  prelude, postlude, die_mem);

  /* Write the translation to a string. */
  string result = express_as_string(prelude, postlude, head);
  return result;
}

Dwarf_Die*
dwflpp::type_die_for_local (vector<Dwarf_Die>& scopes,
                            Dwarf_Addr pc,
                            string const & local,
                            const target_symbol *e,
                            Dwarf_Die *typedie)
{
  Dwarf_Die vardie;
  Dwarf_Attribute attr_mem;

  find_variable_and_frame_base (scopes, pc, local, e, &vardie, &attr_mem);

  if (dwarf_attr_die (&vardie, DW_AT_type, typedie) == NULL)
    throw SEMANTIC_ERROR(_F("failed to retrieve type attribute for '%s' [man error::dwarf]", local.c_str()), e->tok);

  translate_components (NULL, NULL, pc, e, &vardie, typedie);
  return typedie;
}


string
dwflpp::literal_stmt_for_return (Dwarf_Die *scope_die,
                                 Dwarf_Addr pc,
                                 const target_symbol *e,
                                 bool lvalue,
                                 Dwarf_Die *die_mem)
{
  if (sess.verbose>2)
      clog << _F("literal_stmt_for_return: finding return value for %s (%s)\n",
                (dwarf_diename(scope_die) ?: "<unknown>"), (dwarf_diename(cu) ?: "<unknown>"));

  struct obstack pool;
  obstack_tracker p (&pool);
  struct location *tail = NULL;

  /* Given $return->bar->baz[NN], translate the location of return. */
  const Dwarf_Op *locops;
  int nlocops = dwfl_module_return_value_location (module, scope_die,
                                                   &locops);
  if (nlocops < 0)
    throw SEMANTIC_ERROR(_F("failed to retrieve return value location for %s [man error::dwarf] (%s)",
                            (dwarf_diename(scope_die) ?: "<unknown>"),
                            (dwarf_diename(cu) ?: "<unknown>")), e->tok);
  // the function has no return value (e.g. "void" in C)
  else if (nlocops == 0)
    throw SEMANTIC_ERROR(_F("function %s (%s) has no return value",
                            (dwarf_diename(scope_die) ?: "<unknown>"),
                            (dwarf_diename(cu) ?: "<unknown>")), e->tok);

  l2c_ctx.pc = pc;
  l2c_ctx.die = scope_die;
  struct location  *head = c_translate_location (&pool, &loc2c_error, this,
                                                 &loc2c_emit_address,
                                                 1, 0 /* PR9768 */,
                                                 pc, NULL, locops, nlocops,
                                                 &tail, NULL, NULL);

  /* Translate the ->bar->baz[NN] parts. */

  Dwarf_Die vardie = *scope_die, typedie;
  if (dwarf_attr_die (&vardie, DW_AT_type, &typedie) == NULL)
    throw SEMANTIC_ERROR(_F("failed to retrieve return value type attribute for %s [man error::dwarf] (%s)",
                            (dwarf_diename(&vardie) ?: "<unknown>"),
                            (dwarf_diename(cu) ?: "<unknown>")), e->tok);
  
  translate_components (&pool, &tail, pc, e, &vardie, &typedie);

  /* Translate the assignment part, either
     x = $return->bar->baz[NN]
     or
     $return->bar->baz[NN] = x
  */

  string prelude, postlude;
  translate_final_fetch_or_store (&pool, &tail, module_bias,
                                  &vardie, &typedie, lvalue, e,
                                  prelude, postlude, die_mem);

  /* Write the translation to a string. */
  string result = express_as_string(prelude, postlude, head);
  return result;
}

Dwarf_Die*
dwflpp::type_die_for_return (Dwarf_Die *scope_die,
                             Dwarf_Addr pc,
                             const target_symbol *e,
                             Dwarf_Die *typedie)
{
  Dwarf_Die vardie = *scope_die;
  if (dwarf_attr_die (&vardie, DW_AT_type, typedie) == NULL)
    throw SEMANTIC_ERROR(_F("failed to retrieve return value type attribute for %s [man error::dwarf] (%s)",
                           (dwarf_diename(&vardie) ?: "<unknown>"),
                           (dwarf_diename(cu) ?: "<unknown>")), e->tok);

  translate_components (NULL, NULL, pc, e, &vardie, typedie);
  return typedie;
}


string
dwflpp::literal_stmt_for_pointer (Dwarf_Die *start_typedie,
                                  const target_symbol *e,
                                  bool lvalue,
                                  Dwarf_Die *die_mem)
{
  if (sess.verbose>2)
      clog << _F("literal_stmt_for_pointer: finding value for %s (%s)\n",
                  dwarf_type_name(start_typedie).c_str(), (dwarf_diename(cu) ?: "<unknown>"));

  struct obstack pool;
  obstack_tracker p (&pool);
  l2c_ctx.pc = 1;
  l2c_ctx.die = start_typedie;
  struct location *head = c_translate_argument (&pool, &loc2c_error, this,
                                                &loc2c_emit_address,
                                                1, "STAP_ARG_pointer");
  struct location *tail = head;

  /* Translate the ->bar->baz[NN] parts. */

  unsigned first = 0;
  Dwarf_Die typedie = *start_typedie, vardie = typedie;

  /* As a special case when typedie is not an array or pointer, we can
   * allow array indexing on STAP_ARG_pointer instead (since we do
   * know the pointee type and can determine its size).  PR11556. */
  const target_symbol::component* c =
    e->components.empty() ? NULL : &e->components[0];
  if (c && (c->type == target_symbol::comp_literal_array_index ||
            c->type == target_symbol::comp_expression_array_index))
    {
      resolve_unqualified_inner_typedie (&typedie, &typedie, e);
      int typetag = dwarf_tag (&typedie);
      if (typetag != DW_TAG_pointer_type &&
          typetag != DW_TAG_array_type)
        {
	  l2c_ctx.die = &typedie;
          if (c->type == target_symbol::comp_literal_array_index)
            c_translate_array_pointer (&pool, 1, &typedie, &tail, NULL, c->num_index);
          else
            c_translate_array_pointer (&pool, 1, &typedie, &tail, "STAP_ARG_index0", 0);
          ++first;
        }
    }

  /* Now translate the rest normally. */

  translate_components (&pool, &tail, 0, e, &vardie, &typedie, first);

  /* Translate the assignment part, either
     x = (STAP_ARG_pointer)->bar->baz[NN]
     or
     (STAP_ARG_pointer)->bar->baz[NN] = x
  */

  string prelude, postlude;
  translate_final_fetch_or_store (&pool, &tail, module_bias,
                                  &vardie, &typedie, lvalue, e,
                                  prelude, postlude, die_mem);

  /* Write the translation to a string. */
  string result = express_as_string(prelude, postlude, head);
  return result;
}

Dwarf_Die*
dwflpp::type_die_for_pointer (Dwarf_Die *start_typedie,
                              const target_symbol *e,
                              Dwarf_Die *typedie)
{
  unsigned first = 0;
  *typedie = *start_typedie;
  Dwarf_Die vardie = *typedie;

  /* Handle the same PR11556 case as above. */
  const target_symbol::component* c =
    e->components.empty() ? NULL : &e->components[0];
  if (c && (c->type == target_symbol::comp_literal_array_index ||
            c->type == target_symbol::comp_expression_array_index))
    {
      resolve_unqualified_inner_typedie (typedie, typedie, e);
      int typetag = dwarf_tag (typedie);
      if (typetag != DW_TAG_pointer_type &&
          typetag != DW_TAG_array_type)
        ++first;
    }

  translate_components (NULL, NULL, 0, e, &vardie, typedie, first);
  return typedie;
}


static bool
in_kprobes_function(systemtap_session& sess, Dwarf_Addr addr)
{
  if (sess.sym_kprobes_text_start != 0 && sess.sym_kprobes_text_end != 0)
    {
      // If the probe point address is anywhere in the __kprobes
      // address range, we can't use this probe point.
      if (addr >= sess.sym_kprobes_text_start && addr < sess.sym_kprobes_text_end)
        return true;
    }
  return false;
}


enum dwflpp::blacklisted_type
dwflpp::blacklisted_p(interned_string funcname,
                      interned_string filename,
                      int,
                      interned_string module,
                      Dwarf_Addr addr,
                      bool has_return)
{
  if (!blacklist_enabled)
    return dwflpp::blacklisted_none;

  enum dwflpp::blacklisted_type blacklisted = dwflpp::blacklisted_none;

  // check against section blacklist
  string section = get_blacklist_section(addr);

  // PR6503: modules don't need special init/exit treatment
  if (module == TOK_KERNEL && !regexec (&blacklist_section, section.c_str(), 0, NULL, 0))
    blacklisted = dwflpp::blacklisted_section;

  // Check for function marked '__kprobes'.
  else if (module == TOK_KERNEL && in_kprobes_function(sess, addr))
    blacklisted = dwflpp::blacklisted_kprobes;

  // Check probe point against function blacklist
  else if (!regexec(&blacklist_func, funcname.to_string().c_str(), 0, NULL, 0))
    blacklisted = dwflpp::blacklisted_function;

  // Check probe point against function return blacklist
  else if (has_return && !regexec(&blacklist_func_ret, funcname.to_string().c_str(), 0, NULL, 0))
    blacklisted = dwflpp::blacklisted_function_return;

  // Check probe point against file blacklist
  else if (!regexec(&blacklist_file, filename.to_string().c_str(), 0, NULL, 0))
    blacklisted = dwflpp::blacklisted_file;

  if (blacklisted)
    {
      if (sess.verbose>1)
        clog << _(" - blacklisted");
      if (sess.guru_mode)
        {
          blacklisted = dwflpp::blacklisted_none;
          if (sess.verbose>1)
            clog << _(" but not skipped (guru mode enabled)");
        }
    }

  // This probe point is not blacklisted.
  return blacklisted;
}


void
dwflpp::build_kernel_blacklist()
{
  // We build up the regexps in these strings

  // Add ^ anchors at the front; $ will be added just before regcomp.

  string blfn = "^(";
  string blfn_ret = "^(";
  string blfile = "^(";
  string blsection = "^(";

  blsection += "\\.init\\."; // first alternative, no "|"
  blsection += "|\\.exit\\.";
  blsection += "|\\.devinit\\.";
  blsection += "|\\.devexit\\.";
  blsection += "|\\.cpuinit\\.";
  blsection += "|\\.cpuexit\\.";
  blsection += "|\\.meminit\\.";
  blsection += "|\\.memexit\\.";

  /* NOTE all include/asm .h blfile patterns might need "full path"
     so prefix those with '.*' - see PR13108 and PR13112. */
  blfile += "kernel/kprobes\\.c"; // first alternative, no "|"
  blfile += "|arch/.*/kernel/kprobes\\.c";
  blfile += "|.*/include/asm/io\\.h";
  blfile += "|.*/include/asm/io-defs\\.h";
  blfile += "|.*/include/asm/io_64\\.h";
  blfile += "|.*/include/asm/bitops\\.h";
  blfile += "|drivers/ide/ide-iops\\.c";
  // paravirt ops
  blfile += "|arch/.*/kernel/paravirt\\.c";
  blfile += "|.*/include/asm/paravirt\\.h";

  // XXX: it would be nice if these blacklisted functions were pulled
  // in dynamically, instead of being statically defined here.
  // Perhaps it could be populated from script files.  A "noprobe
  // kernel.function("...")"  construct might do the trick.

  // Most of these are marked __kprobes in newer kernels.  We list
  // them here (anyway) so the translator can block them on older
  // kernels that don't have the __kprobes function decorator.  This
  // also allows detection of problems at translate- rather than
  // run-time.

  blfn += "atomic_notifier_call_chain"; // first blfn; no "|"
  blfn += "|default_do_nmi";
  blfn += "|__die";
  blfn += "|die_nmi";
  blfn += "|do_debug";
  blfn += "|do_general_protection";
  blfn += "|do_int3";
  blfn += "|do_IRQ";
  blfn += "|do_page_fault";
  blfn += "|do_sparc64_fault";
  blfn += "|do_trap";
  blfn += "|dummy_nmi_callback";
  blfn += "|flush_icache_range";
  blfn += "|ia64_bad_break";
  blfn += "|ia64_do_page_fault";
  blfn += "|ia64_fault";
  blfn += "|io_check_error";
  blfn += "|mem_parity_error";
  blfn += "|nmi_watchdog_tick";
  blfn += "|notifier_call_chain";
  blfn += "|oops_begin";
  blfn += "|oops_end";
  blfn += "|program_check_exception";
  blfn += "|single_step_exception";
  blfn += "|sync_regs";
  blfn += "|unhandled_fault";
  blfn += "|unknown_nmi_error";
  blfn += "|xen_[gs]et_debugreg";
  blfn += "|xen_irq_.*";
  blfn += "|xen_.*_fl_direct.*";
  blfn += "|check_events";
  blfn += "|xen_adjust_exception_frame";
  blfn += "|xen_iret.*";
  blfn += "|xen_sysret64.*";
  blfn += "|test_ti_thread_flag.*";
  blfn += "|inat_get_opcode_attribute";
  blfn += "|system_call_after_swapgs";
  blfn += "|HYPERVISOR_[gs]et_debugreg";
  blfn += "|HYPERVISOR_event_channel_op";
  blfn += "|hash_64";
  blfn += "|hash_ptr";
  blfn += "|native_set_pte";

  // Lots of locks
  blfn += "|.*raw_.*_lock.*";
  blfn += "|.*raw_.*_unlock.*";
  blfn += "|.*raw_.*_trylock.*";
  blfn += "|.*read_lock.*";
  blfn += "|.*read_unlock.*";
  blfn += "|.*read_trylock.*";
  blfn += "|.*write_lock.*";
  blfn += "|.*write_unlock.*";
  blfn += "|.*write_trylock.*";
  blfn += "|.*write_seqlock.*";
  blfn += "|.*write_sequnlock.*";
  blfn += "|.*spin_lock.*";
  blfn += "|.*spin_unlock.*";
  blfn += "|.*spin_trylock.*";
  blfn += "|.*spin_is_locked.*";
  blfn += "|rwsem_.*lock.*";
  blfn += "|.*mutex_.*lock.*";
  blfn += "|raw_.*";

  // atomic functions
  blfn += "|atomic_.*";
  blfn += "|atomic64_.*";

  // few other problematic cases
  blfn += "|get_bh";
  blfn += "|put_bh";

  // Experimental
  blfn += "|.*apic.*|.*APIC.*";
  blfn += "|.*softirq.*";
  blfn += "|.*IRQ.*";
  blfn += "|.*_intr.*";
  blfn += "|__delay";
  blfn += "|.*kernel_text.*";
  blfn += "|get_current";
  blfn += "|current_.*";
  blfn += "|.*exception_tables.*";
  blfn += "|.*setup_rt_frame.*";

  // PR 5759, CONFIG_PREEMPT kernels
  blfn += "|.*preempt_count.*";
  blfn += "|preempt_schedule";

  // These functions don't return, so return probes would never be recovered
  blfn_ret += "do_exit"; // no "|"
  blfn_ret += "|sys_exit";
  blfn_ret += "|sys_exit_group";

  // __switch_to changes "current" on x86_64 and i686, so return probes
  // would cause kernel panic, and it is marked as "__kprobes" on x86_64
  if (sess.architecture == "x86_64")
    blfn += "|__switch_to";
  if (sess.architecture == "i686")
    blfn_ret += "|__switch_to";

  // RHEL6 pre-beta 2.6.32-19.el6
  blfn += "|special_mapping_.*";
  blfn += "|.*_pte_.*"; // or "|smaps_pte_range";
  blfile += "|fs/seq_file\\.c";

  blfn += ")$";
  blfn_ret += ")$";
  blfile += ")$";
  blsection += ")"; // NB: no $, sections match just the beginning

  if (sess.verbose > 2)
    {
      clog << _("blacklist regexps:") << endl;
      clog << "blfn: " << blfn << endl;
      clog << "blfn_ret: " << blfn_ret << endl;
      clog << "blfile: " << blfile << endl;
      clog << "blsection: " << blsection << endl;
    }

  int rc = regcomp (& blacklist_func, blfn.c_str(), REG_NOSUB|REG_EXTENDED);
  if (rc) throw SEMANTIC_ERROR (_("blacklist_func regcomp failed"));
  rc = regcomp (& blacklist_func_ret, blfn_ret.c_str(), REG_NOSUB|REG_EXTENDED);
  if (rc) throw SEMANTIC_ERROR (_("blacklist_func_ret regcomp failed"));
  rc = regcomp (& blacklist_file, blfile.c_str(), REG_NOSUB|REG_EXTENDED);
  if (rc) throw SEMANTIC_ERROR (_("blacklist_file regcomp failed"));
  rc = regcomp (& blacklist_section, blsection.c_str(), REG_NOSUB|REG_EXTENDED);
  if (rc) throw SEMANTIC_ERROR (_("blacklist_section regcomp failed"));

  blacklist_enabled = true;
}


void
dwflpp::build_user_blacklist()
{
  // We build up the regexps in these strings

  // Add ^ anchors at the front; $ will be added just before regcomp.

  string blfn = "^(";
  string blfn_ret = "^(";
  string blfile = "^(";
  string blsection = "^(";

  // Non-matching placeholders until we have real things to match
  blfn += ".^";
  blfile += ".^";
  blsection += ".^";

  // These functions don't use the normal function-entry ABI, so can't be .return probed safely
  blfn_ret += "_start";

  blfn += ")$";
  blfn_ret += ")$";
  blfile += ")$";
  blsection += ")"; // NB: no $, sections match just the beginning

  if (sess.verbose > 2)
    {
      clog << _("blacklist regexps:") << endl;
      clog << "blfn: " << blfn << endl;
      clog << "blfn_ret: " << blfn_ret << endl;
      clog << "blfile: " << blfile << endl;
      clog << "blsection: " << blsection << endl;
    }

  int rc = regcomp (& blacklist_func, blfn.c_str(), REG_NOSUB|REG_EXTENDED);
  if (rc) throw SEMANTIC_ERROR (_("blacklist_func regcomp failed"));
  rc = regcomp (& blacklist_func_ret, blfn_ret.c_str(), REG_NOSUB|REG_EXTENDED);
  if (rc) throw SEMANTIC_ERROR (_("blacklist_func_ret regcomp failed"));
  rc = regcomp (& blacklist_file, blfile.c_str(), REG_NOSUB|REG_EXTENDED);
  if (rc) throw SEMANTIC_ERROR (_("blacklist_file regcomp failed"));
  rc = regcomp (& blacklist_section, blsection.c_str(), REG_NOSUB|REG_EXTENDED);
  if (rc) throw SEMANTIC_ERROR (_("blacklist_section regcomp failed"));

  blacklist_enabled = true;
}


string
dwflpp::get_blacklist_section(Dwarf_Addr addr)
{
  string blacklist_section;
  Dwarf_Addr bias;
  // We prefer dwfl_module_getdwarf to dwfl_module_getelf here,
  // because dwfl_module_getelf can force costly section relocations
  // we don't really need, while either will do for this purpose.
  Elf* elf = (dwarf_getelf (dwfl_module_getdwarf (module, &bias))
              ?: dwfl_module_getelf (module, &bias));

  Dwarf_Addr offset = addr - bias;
  if (elf)
    {
      Elf_Scn* scn = 0;
      size_t shstrndx;
      DWFL_ASSERT ("getshdrstrndx", elf_getshdrstrndx (elf, &shstrndx));
      while ((scn = elf_nextscn (elf, scn)) != NULL)
        {
          GElf_Shdr shdr_mem;
          GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
          if (! shdr)
            continue; // XXX error?

          if (!(shdr->sh_flags & SHF_ALLOC))
            continue;

          GElf_Addr start = shdr->sh_addr;
          GElf_Addr end = start + shdr->sh_size;
          if (! (offset >= start && offset < end))
            continue;

          blacklist_section = elf_strptr (elf, shstrndx, shdr->sh_name);
          break;
        }
    }
  return blacklist_section;
}


/* Find the section named 'section_name'  in the current module
 * returning the section header using 'shdr_mem' */

GElf_Shdr *
dwflpp::get_section(string section_name, GElf_Shdr *shdr_mem, Elf **elf_ret)
{
  GElf_Shdr *shdr = NULL;
  Elf* elf;
  Dwarf_Addr bias;
  size_t shstrndx;

  // Explicitly look in the main elf file first.
  elf = dwfl_module_getelf (module, &bias);
  Elf_Scn *probe_scn = NULL;

  DWFL_ASSERT ("getshdrstrndx", elf_getshdrstrndx (elf, &shstrndx));

  bool have_section = false;

  while ((probe_scn = elf_nextscn (elf, probe_scn)))
    {
      shdr = gelf_getshdr (probe_scn, shdr_mem);
      assert (shdr != NULL);

      if (elf_strptr (elf, shstrndx, shdr->sh_name) == section_name)
	{
	  have_section = true;
	  break;
	}
    }

  // Older versions may put the section in the debuginfo dwarf file,
  // so check if it actually exists, if not take a look in the debuginfo file
  if (! have_section || (have_section && shdr->sh_type == SHT_NOBITS))
    {
      elf = dwarf_getelf (dwfl_module_getdwarf (module, &bias));
      if (! elf)
	return NULL;
      DWFL_ASSERT ("getshdrstrndx", elf_getshdrstrndx (elf, &shstrndx));
      probe_scn = NULL;
      while ((probe_scn = elf_nextscn (elf, probe_scn)))
	{
	  shdr = gelf_getshdr (probe_scn, shdr_mem);
	  if (elf_strptr (elf, shstrndx, shdr->sh_name) == section_name)
	    {
	      have_section = true;
	      break;
	    }
	}
    }

  if (!have_section)
    return NULL;

  if (elf_ret)
    *elf_ret = elf;
  return shdr;
}


Dwarf_Addr
dwflpp::relocate_address(Dwarf_Addr dw_addr, interned_string& reloc_section)
{
  // PR10273
  // libdw address, so adjust for bias gotten from dwfl_module_getdwarf
  Dwarf_Addr reloc_addr = dw_addr + module_bias;
  if (!module)
    {
      assert(module_name == TOK_KERNEL);
      reloc_section = "";
    }
  else if (dwfl_module_relocations (module) > 0)
    {
      // This is a relocatable module; libdwfl already knows its
      // sections, so we can relativize addr.
      int idx = dwfl_module_relocate_address (module, &reloc_addr);
      const char* r_s = dwfl_module_relocation_info (module, idx, NULL);
      if (r_s)
        reloc_section = r_s;

      if (reloc_section == "" && dwfl_module_relocations (module) == 1)
          reloc_section = ".dynamic";
    }
  else
    reloc_section = ".absolute";
  return reloc_addr;
}

/* Returns the call frame address operations for the given program counter
 * in the libdw address space.
 */
Dwarf_Op *
dwflpp::get_cfa_ops (Dwarf_Addr pc)
{
  Dwarf_Op *cfa_ops = NULL;

  if (sess.verbose > 2)
    clog << "get_cfa_ops @0x" << hex << pc << dec
	 << ", module_start @0x" << hex << module_start << dec << endl;

  // Try debug_frame first, then fall back on eh_frame.
  size_t cfa_nops = 0;
  Dwarf_Addr bias = 0;
  Dwarf_Frame *frame = NULL;
  Dwarf_CFI *cfi = dwfl_module_dwarf_cfi (module, &bias);
  if (cfi != NULL)
    {
      if (sess.verbose > 3)
	clog << "got dwarf cfi bias: 0x" << hex << bias << dec << endl;
      if (dwarf_cfi_addrframe (cfi, pc - bias, &frame) == 0)
	dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops);
      else if (sess.verbose > 3)
	clog << "dwarf_cfi_addrframe failed: " << dwarf_errmsg(-1) << endl;
    }
  else if (sess.verbose > 3)
    clog << "dwfl_module_dwarf_cfi failed: " << dwfl_errmsg(-1) << endl;

  if (cfa_ops == NULL)
    {
      cfi = dwfl_module_eh_cfi (module, &bias);
      if (cfi != NULL)
	{
	  if (sess.verbose > 3)
	    clog << "got eh cfi bias: 0x" << hex << bias << dec << endl;
	  Dwarf_Frame *frame = NULL;
	  if (dwarf_cfi_addrframe (cfi, pc - bias, &frame) == 0)
	    dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops);
	  else if (sess.verbose > 3)
	    clog << "dwarf_cfi_addrframe failed: " << dwarf_errmsg(-1) << endl;
	}
      else if (sess.verbose > 3)
	clog << "dwfl_module_eh_cfi failed: " << dwfl_errmsg(-1) << endl;

    }

  if (sess.verbose > 2)
    {
      if (cfa_ops == NULL)
	clog << _("not found cfa") << endl;
      else
	{
	  Dwarf_Addr frame_start, frame_end;
	  bool frame_signalp;
	  int info = dwarf_frame_info (frame, &frame_start, &frame_end,
				       &frame_signalp);
          clog << _F("found cfa, info: %d [start: %#" PRIx64 ", end: %#" PRIx64 
                     ", nops: %zu", info, frame_start, frame_end, cfa_nops) << endl;
	}
    }

  return cfa_ops;
}

int
dwflpp::add_module_build_id_to_hash (Dwfl_Module *m,
                 void **userdata __attribute__ ((unused)),
                 const char *name,
                 Dwarf_Addr,
                 void *arg)
{
   string modname = name;
   systemtap_session * s = (systemtap_session *)arg;
  if (pending_interrupts)
    return DWARF_CB_ABORT;

  // Extract the build ID
  const unsigned char *bits;
  GElf_Addr vaddr;
  int bits_length = dwfl_module_build_id(m, &bits, &vaddr);
  if(bits_length > 0)
    {
      // Convert the binary bits to a hex string
      string hex = hex_dump(bits, bits_length);

      // Store the build ID in the session
      s->build_ids.push_back(hex);
    }

  return DWARF_CB_OK;
}



// Perform PR15123 heuristic for given variable at given address.
// Return alternate pc address to do location-list lookup at, or 0 if
// inapplicable.
//
Dwarf_Addr
dwflpp::pr15123_retry_addr (Dwarf_Addr pc, Dwarf_Die* die)
{
  // For PR15123, we'd like to detect the situation where the
  // incoming PC may point to a couple-of-byte instruction
  // sequence that gcc emits for CFLAGS=-mfentry, and where
  // context variables are in fact available throughout, *but* due
  // to the bug, the dwarf debuginfo location-list only starts a
  // few instructions later.  Prologue searching does not resolve
  // this as a line-record is in place at the -mfentry prologue.
  //
  // Detecting this is complicated because ...
  // - we only want to do this if -mfentry was actually used
  // - if <pc> points to the a function entry point
  // - if the architecture is familiar enough that we can have a
  // hard-coded constant to skip over the prologue.
  //
  // Otherwise, we could give a false-positive - return corrupted
  // data.
  //
  // Use of -mfentry is detectable only if CFLAGS=-grecord-gcc-switches
  // was used.  Without it, set the PR15123_ASSUME_MFENTRY environment
  // variable to override the -mfentry test.

  if (getenv ("PR15123_DISABLE"))
    return 0;

  if (!getenv ("PR15123_ASSUME_MFENTRY")) {
    Dwarf_Die cudie;
    string producer, version;
    dwarf_diecu (die, &cudie, NULL, NULL);
    if (!is_gcc_producer(&cudie, producer, version))
      return 0;
    if (producer.find("-mfentry") == string::npos)
      return 0;
  }

  // Determine if this pc maps to the beginning of a
  // real function (not some inlined doppelganger.  This
  // is made tricker by this->function may not be
  // pointing at the right DIE (say e.g. stap encountered
  // the inlined copy first, so was focus_on_function'd).
  vector<Dwarf_Die> scopes = getscopes(pc);
  if (scopes.size() == 0)
    return 0;

  Dwarf_Die outer_function_die = scopes[0];
  Dwarf_Addr entrypc;
  if (!die_entrypc(& outer_function_die, &entrypc) || entrypc != pc)
    return 0; // (will fail on retry, so we won't loop more than once)

  if (sess.architecture == "i386" ||
      sess.architecture == "x86_64") {
    /* pull the trigger */
    if (sess.verbose > 2)
      clog << _("retrying variable location-list lookup at address pc+5\n");
    return pc + 5;
  }

  return 0;
}

bool
dwflpp::has_gnu_debugdata ()
{
  Dwarf_Addr load_addr;
  // Note we really want the actual elf file, not the dwarf .debug file.
  Elf* elf = dwfl_module_getelf (module, &load_addr);
  size_t shstrndx;
  assert (elf_getshdrstrndx (elf, &shstrndx) >= 0);

  // Get the gnu_debugdata section header
  Elf_Scn *scn = NULL;
  GElf_Shdr *gnu_debugdata_shdr = NULL;
  GElf_Shdr gnu_debugdata_shdr_mem;
  while ((scn = elf_nextscn (elf, scn)))
    {
      gnu_debugdata_shdr = gelf_getshdr (scn, &gnu_debugdata_shdr_mem);
      assert (gnu_debugdata_shdr != NULL);
      if (strcmp (elf_strptr (elf, shstrndx, gnu_debugdata_shdr->sh_name), ".gnu_debugdata") == 0)
	return true;
    }
  return false;
}

// If not GCC, return false. Otherwise, return true and set vers.
bool
dwflpp::is_gcc_producer(Dwarf_Die *cudie, string& producer, string& version)
{
  Dwarf_Attribute producer_attr;
  if (!dwarf_attr_integrate(cudie, DW_AT_producer, &producer_attr))
    return false;

  // GNU {C|C++|...} x.x.x YYYYMMDD ...
  const char *cproducer = dwarf_formstring(&producer_attr);
  if (!cproducer)
    return false;
  producer = cproducer;

  vector<string> tokens;
  tokenize(producer, tokens);

  if (tokens.size() < 3
      || tokens[0] != "GNU"
      || tokens[2].find_first_not_of(".0123456789") != string::npos)
    return false;

  version = tokens[2];
  return true;
}

static bool
die_has_loclist(Dwarf_Die *begin_die)
{
  Dwarf_Die die;
  Dwarf_Attribute loc;

  if (dwarf_child(begin_die, &die) != 0)
    return false;

  do
    {
      switch (dwarf_tag(&die))
        {
        case DW_TAG_formal_parameter:
        case DW_TAG_variable:
          if (dwarf_attr_integrate(&die, DW_AT_location, &loc)
           && dwarf_whatform(&loc) == DW_FORM_sec_offset)
            return true;
          break;
        default:
          if (dwarf_haschildren (&die))
            if (die_has_loclist(&die))
              return true;
          break;
        }
    }
  while (dwarf_siblingof (&die, &die) == 0);

  return false;
}

bool
dwflpp::has_valid_locs ()
{
  assert(cu);

  // The current CU has valid location info (implying we do not need to skip the
  // prologue) if
  //   - it was compiled with -O2 -g (in which case, GCC outputs proper location
  //     info for the prologue), and
  //   - it was compiled by GCC >= 4.5 (previous versions could have had invalid
  //     debug info in the prologue, see GDB's PR13777)
  // Note that clang behaves similarly to GCC here: non-optimized code does not
  // have location lists, while optimized code does. In the check below, even if
  // the producer is not GCC, we assume that it is valid to do the loclist check
  // afterwards (which it is for clang).

  string prod, vers;
  if (is_gcc_producer(cu, prod, vers)
   && strverscmp(vers.c_str(), "4.5") < 0)
    return false;

  // We determine if the current CU has been optimized with -O2 -g by looking
  // for any data objects whose DW_AT_location is a location list. This is also
  // how GDB determines whether to skip the prologue or not. See GDB's PR12573
  // and also RHBZ612253#c6.
  if (!die_has_loclist(cu))
    return false;

  if (sess.verbose > 2)
    clog << _F("CU '%s' in module '%s' has valid locs",
               cu_name().c_str(), module_name.c_str()) << endl;

  return true;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
