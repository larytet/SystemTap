// interned string table
// Copyright (C) 2015 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include "stringtable.h"

#include "stap-probe.h"

#include <string>
#include <cstring>
#include <fstream>
#include <unordered_set>


using namespace std;
#if defined(HAVE_BOOST_UTILITY_STRING_REF_HPP)
using namespace boost;


#if INTERNED_STRING_INSTRUMENT
static bool whitespace_p (char c)
{
  return isspace(c);
}
#endif


#if INTERNED_STRING_CUSTOM_HASH
// A custom hash 
struct stringtable_hash
{
  size_t operator()(const string& c) const {
    const char* b = c.data();
    size_t real_length = c.size();
    const size_t blocksize = 32; // a cache line or two

    // hash the length
    size_t hash = real_length;

    // hash the beginning
    size_t length = real_length;
    if (length > blocksize)
      length = blocksize;
    while (length-- > 0)
      hash = (hash * 131) + *b++;

    // hash the middle
    if (real_length > blocksize * 3)
      {
        length = blocksize; // more likely not to span a cache line
        b = (const char*)c.data() + (real_length/2);
        while (length-- > 0)
          hash = (hash * 131) + *b++;
      }

    // the ends, especially of generated bits, are likely to be } } }
    // \n kinds of similar things

#if INTERNED_STRING_INSTRUMENT
    ofstream f ("/tmp/hash.log", ios::app);
    string s = c.substr(0,32);
    s.erase (remove_if(s.begin(), s.end(), whitespace_p), s.end());
    f << hash << " " << c.length() << " " << s << endl;
    f.close();
#endif
      
    return hash;
  }
};
typedef unordered_set<std::string, stringtable_hash> stringtable_t;
#else
typedef unordered_set<std::string> stringtable_t;
#endif


static char chartable[256];
static stringtable_t stringtable;
// XXX: set a larger initial size?  For reference, a
//
//    probe kernel.function("*") {}
//
// can intern some 450,000 entries.


// Generate a long-lived string_ref for the given input string.  In
// the absence of proper refcounting, memory is kept for the whole
// duration of the systemtap run.  Try to reuse the same string
// object for multiple invocations.  Old string_refs remain valid 
// because std::set<> guarantees iterator validity across inserts,
// which means that our value strings stay put.

// static
interned_string interned_string::intern(const string& value)
{
  if (value.empty())
    return interned_string ();

  if (value.size() == 1)
    return intern(value[0]);

  pair<stringtable_t::iterator,bool> result = stringtable.insert(value);
  PROBE2(stap, intern_string, value.c_str(), result.second);
  stringtable_t::iterator it = result.first; // persistent iterator!
  return string_ref (it->data(), it->length()); // hope for RVO/elision

  // XXX: for future consideration, consider searching the stringtable
  // for instances where 'value' is a substring.  We could string_ref
  // to substrings just fine.  The trouble is that searching the
  // stringtable naively is very timetaking; it saves memory but costs
  // mucho CPU.
}

// static
interned_string interned_string::intern(const char* value)
{
  if (!value || !value[0])
    return interned_string ();

  if (!value[1])
    return intern(value[0]);

  return intern(string(value));
}

// static
interned_string interned_string::intern(char value)
{
  if (!value)
    return interned_string ();

  size_t i = (unsigned char) value;
  if (!chartable[i]) // lazy init
    chartable[i] = value;

  return string_ref (&chartable[i], 1);
}

#if INTERNED_STRING_FIND_MEMMEM
size_t interned_string::find (const boost::string_ref& f) const
{
  const char *ptr = (const char*) memmem (this->data(), this->size(),
                                          f.data(), f.size());
  if (ptr)
    return (ptr - this->data());
  else
    return boost::string_ref::npos;
}

size_t interned_string::find (const std::string& f) const
{
  const char *ptr = (const char*) memmem (this->data(), this->size(),
                                          f.data(), f.size());
  if (ptr)
    return (ptr - this->data());
  else
    return boost::string_ref::npos;
}

size_t interned_string::find (const char *f) const
{
  const char *ptr = (const char*) memmem (this->data(), this->size(),
                                          f, strlen(f));
  if (ptr)
    return (ptr - this->data());
  else
    return boost::string_ref::npos;
}
#endif
#endif /* defined(HAVE_BOOST_UTILITY_STRING_REF_HPP) */

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
