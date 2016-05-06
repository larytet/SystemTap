// -*- C++ -*-
// Copyright (C) 2008-2014 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "dwarf_wrappers.h"
#include "staptree.h"
#include "util.h"

#include <cstring>
#include <sstream>
#include <string>
#include <elfutils/libdwfl.h>
#include <dwarf.h>

using namespace std;

void dwfl_assert(const string& desc, int rc,
                 const string& file, int line)
{
  if (rc == 0)
    return;
  string msg = _F("libdwfl failure (%s): ", desc.c_str());
  if (rc < 0)
    msg += (dwfl_errmsg (rc) ?: "?");
  else
    msg += std::strerror (rc);
  throw semantic_error (file+":"+lex_cast(line), msg);
}

void dwarf_assert(const string& desc, int rc,
                  const string& file, int line)
{
  if (rc == 0)
    return;
  string msg = _F("libdw failure (%s): ", desc.c_str());
  if (rc < 0)
    msg += dwarf_errmsg (rc);
  else
    msg += std::strerror (rc);
  throw semantic_error (file+":"+lex_cast(line), msg);
}


#if !_ELFUTILS_PREREQ(0, 143)
// Elfutils prior to 0.143 didn't use attr_integrate when looking up the
// decl_file or decl_line, so the attributes would sometimes be missed.  For
// those old versions, we define custom implementations to do the integration.

const char *
dwarf_decl_file_integrate (Dwarf_Die *die)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Sword idx = 0;
  if (dwarf_formsdata (dwarf_attr_integrate (die, DW_AT_decl_file, &attr_mem),
                       &idx) != 0
      || idx == 0)
    return NULL;

  Dwarf_Die cudie;
  Dwarf_Files *files = NULL;
  if (dwarf_getsrcfiles (dwarf_diecu (die, &cudie, NULL, NULL),
                         &files, NULL) != 0)
    return NULL;

  return dwarf_filesrc(files, idx, NULL, NULL);
}

int
dwarf_decl_line_integrate (Dwarf_Die *die, int *linep)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Sword line;

  int res = dwarf_formsdata (dwarf_attr_integrate
                             (die, DW_AT_decl_line, &attr_mem),
                             &line);
  if (res == 0)
    *linep = line;

  return res;
}

#endif // !_ELFUTILS_PREREQ(0, 143)


static bool
dwarf_type_name(Dwarf_Die *type_die, ostream& o, Dwarf_Die& subroutine)
{
  // if we've gotten down to a basic type, then we're done
  bool done = true;
  switch (dwarf_tag(type_die))
    {
    case DW_TAG_enumeration_type:
      o << "enum ";
      break;
    case DW_TAG_structure_type:
      o << "struct ";
      break;
    case DW_TAG_union_type:
      o << "union ";
      break;
    case DW_TAG_class_type:
      o << "class ";
      break;
    case DW_TAG_typedef:
    case DW_TAG_base_type:
      break;

    // modifier types that require recursion first
    case DW_TAG_reference_type:
    case DW_TAG_rvalue_reference_type:
    case DW_TAG_pointer_type:
    case DW_TAG_array_type:
    case DW_TAG_const_type:
    case DW_TAG_volatile_type:
    case DW_TAG_restrict_type:
      done = false;
      break;

    case DW_TAG_subroutine_type:
      // Subroutine types (function pointers) are a weird case.  The modifiers
      // we've recursed so far need to go in the middle, with the return type
      // on the left and parameter types on the right.  We'll back out now to
      // get those modifiers, getting the return and parameters separately.
      subroutine = *type_die;
      return true;

    // unknown tag
    default:
      return false;
    }
  if (done)
    {
      // this follows gdb precedent that anonymous structs/unions
      // are displayed as "struct {...}" and "union {...}".
      o << (dwarf_diename(type_die) ?: "{...}");
      return true;
    }

  // otherwise, this die is a type modifier.

  // recurse into the referent type
  Dwarf_Die subtype_die_mem, *subtype_die;
  subtype_die = dwarf_attr_die(type_die, DW_AT_type, &subtype_die_mem);

  // NB: va_list is a builtin type that shows up in the debuginfo as a
  // "struct __va_list_tag*", but it has to be called only va_list.
  if (subtype_die != NULL &&
      dwarf_tag(type_die) == DW_TAG_pointer_type &&
      dwarf_tag(subtype_die) == DW_TAG_structure_type &&
      strcmp(dwarf_diename(subtype_die) ?: "", "__va_list_tag") == 0)
    {
      o << "va_list";
      return true;
    }

  // if it can't be named, just call it "void"
  if (subtype_die == NULL ||
      !dwarf_type_name(subtype_die, o, subroutine))
    o << "void";

  switch (dwarf_tag(type_die))
    {
    case DW_TAG_reference_type:
      o << "&";
      break;
    case DW_TAG_rvalue_reference_type:
      o << "&&";
      break;
    case DW_TAG_pointer_type:
      o << "*";
      break;
    case DW_TAG_array_type:
      o << "[]";
      break;
    case DW_TAG_const_type:
      // NB: the debuginfo may sometimes have an extra const tag
      // on reference types, which is redundant to us.
      if (subtype_die == NULL ||
          (dwarf_tag(subtype_die) != DW_TAG_reference_type &&
           dwarf_tag(subtype_die) != DW_TAG_rvalue_reference_type))
        o << " const";
      break;
    case DW_TAG_volatile_type:
      o << " volatile";
      break;
    case DW_TAG_restrict_type:
      o << " restrict";
      break;
    default:
      return false;
    }

  return true;
}


static bool
dwarf_subroutine_name(Dwarf_Die *subroutine, ostream& o, const string& modifier)
{
  // First add the return value.
  Dwarf_Die ret_type;
  string ret_string;
  if (dwarf_attr_die (subroutine, DW_AT_type, &ret_type) == NULL)
    o << "void";
  else if (dwarf_type_name (&ret_type, ret_string))
    o << ret_string;
  else
    return false;

  // Now the subroutine modifiers.
  o << " (" << modifier << ")";

  // Then write each parameter.
  o << " (";
  bool first = true;
  Dwarf_Die child;
  if (dwarf_child (subroutine, &child) == 0)
    do
      {
        auto tag = dwarf_tag (&child);
        if (tag == DW_TAG_unspecified_parameters
            || tag == DW_TAG_formal_parameter)
          {
            if (first)
              first = false;
            else
              o << ", ";

            if (tag == DW_TAG_unspecified_parameters)
              o << "...";
            else if (tag == DW_TAG_formal_parameter)
              {
                Dwarf_Die param_type;
                string param_string;
                if (dwarf_attr_die (&child, DW_AT_type, &param_type) == NULL)
                  o << "void";
                else if (dwarf_type_name (&param_type, param_string))
                  o << param_string;
                else
                  return false;
              }
          }
      }
    while (dwarf_siblingof (&child, &child) == 0);
  if (first)
    o << "void";
  o << ")";

  return true;
}


bool
dwarf_type_decl(Dwarf_Die *type_die, const string& var_name, string& decl)
{
  ostringstream o;
  Dwarf_Die subroutine = { 0, 0, 0, 0 };
  if (!dwarf_type_name (type_die, o, subroutine))
    return false;

  if (!var_name.empty())
    o << " " << var_name;

  if (subroutine.addr != 0)
    {
      ostringstream subo;
      if (!dwarf_subroutine_name (&subroutine, subo, o.str()))
        return false;
      decl = subo.str();
    }
  else
    decl = o.str();

  return true;
}


bool
dwarf_type_name(Dwarf_Die *type_die, string& type_name)
{
  return dwarf_type_decl(type_die, "", type_name);
}


string
dwarf_type_name(Dwarf_Die *type_die)
{
  string o;
  return dwarf_type_name (type_die, o) ? o : "<unknown>";
}


/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
