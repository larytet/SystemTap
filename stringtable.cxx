// interned string table
// Copyright (C) 2015 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include "stringtable.h"

#include <string>

#if __cplusplus >= 201103L /* -std=c++11 */
#include <unordered_set>
typedef std::unordered_set<std::string> stringtable_t;
typedef std::unordered_set<boost::string_ref> stringtable_cache_t;
#else
#include <set>
typedef std::set<std::string> stringtable_t;
typedef std::set<boost::string_ref> stringtable_cache_t;
#endif

using namespace std;
using namespace boost;

stringtable_t stringtable;


// Generate a long-lived string_ref for the given input string.  In
// the absence of proper refcounting, memory is kept for the whole
// duration of the systemtap run.  Try to reuse the same string
// object for multiple invocations.  Old string_refs remain valid 
// because std::set<> guarantees iterator validity across inserts,
// which means that our value strings stay put.

string_ref intern(const string& value)
{
  // check the string table for exact match
  stringtable_t::iterator it = stringtable.find(value);
  if (it != stringtable.end())
    return string_ref (it->data(), it->length());

  // alas ... no joy ... insert into set
  it = (stringtable.insert(value)).first; // persistent iterator!
  return string_ref (it->data(), it->length());

  // XXX: for future consideration, consider searching the stringtable
  // for instances where 'value' is a substring.  We could string_ref
  // to substrings just fine.  The trouble is that searching the
  // stringtable naively is very timetaking; it saves memory but costs
  // mucho CPU.
}


/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
