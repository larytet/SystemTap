// -*- C++ -*-
// Copyright (C) 2008-2014 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef DWARF_WRAPPERS_H
#define DWARF_WRAPPERS_H 1

#include "config.h"

extern "C" {
#include <elfutils/libdw.h>
#include <elfutils/version.h>
#include <dwarf.h>
}

#include <string>

#if ! _ELFUTILS_PREREQ(0, 148)
#define DW_AT_linkage_name 0x6e
#endif

#if ! _ELFUTILS_PREREQ(0, 153)
#define DW_TAG_GNU_call_site 0x4109
#define DW_AT_GNU_tail_call 0x2115
#endif

#if ! _ELFUTILS_PREREQ(0, 155)
#define DW_ATE_UTF 0x10
#endif

#define DWFL_ASSERT(desc, arg) \
  dwfl_assert(desc, arg, __FILE__, __LINE__)

// NB: "rc == 0" means OK in this case
void dwfl_assert(const std::string& desc, int rc,
                 const std::string& file, int line);

// Throw error if pointer is NULL
inline void
dwfl_assert(const std::string& desc, const void* ptr,
            const std::string& file, int line)
{
  if (!ptr)
    dwfl_assert(desc, -1, file, line);
}

// Throw error if condition is false
inline void
dwfl_assert(const std::string& desc, bool condition,
            const std::string& file, int line)
{
  if (!condition)
    dwfl_assert(desc, -1, file, line);
}

#define DWARF_ASSERT(desc, arg) \
  dwarf_assert(desc, arg, __FILE__, __LINE__)

// NB: "rc == 0" means OK in this case
void dwarf_assert(const std::string& desc, int rc,
                  const std::string& file, int line);

// Throw error if pointer is NULL
inline void
dwarf_assert(const std::string& desc, const void* ptr,
             const std::string& file, int line)
{
  if (!ptr)
    dwarf_assert(desc, -1, file, line);
}

#define DWARF_LINENO(line) \
  safe_dwarf_lineno(line, __FILE__, __LINE__)

inline int
safe_dwarf_lineno(const Dwarf_Line* line,
                  const std::string& errfile, int errline)
{
  int lineno;
  dwarf_assert("dwarf_lineno",
               dwarf_lineno(const_cast<Dwarf_Line*>(line), &lineno),
               errfile, errline);
  return lineno;
}

#define DWARF_LINEADDR(line) \
  safe_dwarf_lineaddr(line, __FILE__, __LINE__)

inline Dwarf_Addr
safe_dwarf_lineaddr(const Dwarf_Line* line,
                    const std::string& errfile, int errline)
{
  Dwarf_Addr addr;
  dwarf_assert("dwarf_lineaddr",
               dwarf_lineaddr(const_cast<Dwarf_Line*>(line), &addr),
               errfile, errline);
  return addr;
}

#define DWARF_LINESRC(line) \
  safe_dwarf_linesrc(line, NULL, NULL, __FILE__, __LINE__)
#define DWARF_LINESRC2(line, mtime) \
  safe_dwarf_linesrc(line, mtime, NULL, __FILE__, __LINE__)
#define DWARF_LINESRC3(line, mtime, length) \
  safe_dwarf_linesrc(line, mtime, length, __FILE__, __LINE__)

inline const char*
safe_dwarf_linesrc(const Dwarf_Line* line,
                   Dwarf_Word* mtime,
                   Dwarf_Word* length,
                   const std::string& errfile, int errline)
{
  const char* linesrc =
    dwarf_linesrc(const_cast<Dwarf_Line*>(line), mtime, length);
  dwarf_assert("dwarf_linesrc", linesrc, errfile, errline);
  return linesrc;
}

#define DWARF_LINEPROLOGUEEND(line) \
  safe_dwarf_lineprologueend(line, __FILE__, __LINE__)

inline bool
safe_dwarf_lineprologueend(const Dwarf_Line* line,
                           const std::string& errfile, int errline)
{
  bool flag;
  dwarf_assert("is_prologue_end",
               dwarf_lineprologueend(const_cast<Dwarf_Line*>(line), &flag),
               errfile, errline);
  return flag;
}


// Look up the DIE for a reference-form attribute name
inline Dwarf_Die *
dwarf_attr_die (Dwarf_Die *die, unsigned int attr, Dwarf_Die *result)
{
  Dwarf_Attribute attr_mem;
  if (dwarf_formref_die (dwarf_attr_integrate (die, attr, &attr_mem),
			 result) != NULL)
    {
      /* If we want a type make sure we get the actual DIE describing
	 the real type. */
      if (attr == DW_AT_type)
	{
	  Dwarf_Attribute sigm;
	  Dwarf_Attribute *sig = dwarf_attr (result, DW_AT_signature, &sigm);
	  if (sig != NULL)
	    result = dwarf_formref_die (sig, result);

	  /* A DW_AT_signature might point to a type_unit, then
	     the actual type DIE we want is the first child.  */
	  if (result != NULL && dwarf_tag (result) == DW_TAG_type_unit)
	    DWFL_ASSERT("type_unit child", dwarf_child (result, result));
	}
      return result;
    }
  return NULL;
}


// Retrieve the linkage name of a die, either by the MIPS vendor extension or
// DWARF4's standardized attribute.
inline const char *
dwarf_linkage_name (Dwarf_Die *die)
{
  Dwarf_Attribute attr_mem;
  return dwarf_formstring
    (dwarf_attr_integrate (die, DW_AT_MIPS_linkage_name, &attr_mem)
     ?: dwarf_attr_integrate (die, DW_AT_linkage_name, &attr_mem));
}


#if !_ELFUTILS_PREREQ(0, 143)
// Elfutils prior to 0.143 didn't use attr_integrate when looking up the
// decl_file or decl_line, so the attributes would sometimes be missed.  For
// those old versions, we define custom implementations to do the integration.

const char *dwarf_decl_file_integrate (Dwarf_Die *die);
#define dwarf_decl_file dwarf_decl_file_integrate

int dwarf_decl_line_integrate (Dwarf_Die *die, int *linep)
  __nonnull_attribute__ (2);
#define dwarf_decl_line dwarf_decl_line_integrate

#endif // !_ELFUTILS_PREREQ(0, 143)


// Resolve a C declaration for dwarf types
bool dwarf_type_decl(Dwarf_Die *type_die, const std::string& var_name, std::string& decl);

// Resolve a full name for dwarf types
bool dwarf_type_name(Dwarf_Die *type_die, std::string& type_name);
std::string dwarf_type_name(Dwarf_Die *type_die);


#endif

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
