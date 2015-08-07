// backward-compatible unordered containers
// Copyright (C) 2009-2015 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef UNORDERED_H
#define UNORDERED_H

#include "config.h"
#include "stringtable.h"


#if 0 // uncomment to force the old mode
#undef HAVE_TR1_UNORDERED_MAP
#define _BACKWARD_BACKWARD_WARNING_H 1 // defeat deprecation warning
#endif

#ifdef HAVE_TR1_UNORDERED_MAP

#include <tr1/unordered_map>
using std::tr1::unordered_map;
using std::tr1::unordered_multimap;

#include <tr1/unordered_set>
using std::tr1::unordered_set;
using std::tr1::unordered_multiset;

namespace std {
  namespace tr1 {
    template<> struct hash<interned_string> {
      size_t operator() (interned_string s) const
      {
        // NB: we'd love to be able to hook up to a blob hashing
        // function in std::tr1, but there isn't one.  We don't want
        // to copy the interned_string into a temporary string just to
        // hash the thing.
        //
        // This code is based on the g++ _Fnv_hash_base ptr/length case.
        size_t hash = 0;
        const char* x = s.data();
        for (size_t i=s.length(); i>0; i--)
          hash = (hash * 131) + *x++;
        return hash;
      }
    };
  }
}

#else

#include <ext/hash_map>
#define unordered_map __gnu_cxx::hash_map
#define unordered_multimap __gnu_cxx::hash_multimap

#include <ext/hash_set>
#define unordered_set __gnu_cxx::hash_set
#define unordered_multiset __gnu_cxx::hash_multiset

// Hack in common hash functions for strings and raw pointers
namespace __gnu_cxx
{
  template<class T> struct hash<T*> {
    size_t operator() (T* p) const
    { hash<long> h; return h(reinterpret_cast<long>(p)); }
  };
  template<> struct hash<std::string> {
    size_t operator() (std::string const& s) const
    { hash<const char*> h; return h(s.c_str()); }
  };
  template<> struct hash<interned_string> {
    size_t operator() (interned_string s) const
    {
      // same as above for std::tr1:: case
      size_t hash = 0;
      const char* x = s.data();
      for (size_t i=s.length(); i>0; i--)
        hash = (hash * 131) + *x++;
      return hash;
    }
  };
}

#endif

#endif // UNORDERED_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
