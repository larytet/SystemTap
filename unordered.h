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

#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;

#include <unordered_set>
using std::unordered_set;
using std::unordered_multiset;

namespace std {
  template<> struct hash<interned_string> {
    size_t operator() (interned_string s) const
    {
      // NB: we'd love to be able to hook up to a blob hashing
      // function in std::hash, but there isn't one.  We don't want
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

#endif // UNORDERED_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
