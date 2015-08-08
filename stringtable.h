// -*- C++ -*-
// Copyright (C) 2015 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.


#ifndef STRINGTABLE_H
#define STRINGTABLE_H

#include <string>
#include <boost/utility/string_ref.hpp> //header with string_ref

// XXX: experimental tunables
#define INTERNED_STRING_FIND_MEMMEM 1 /* perf stat indicates a very slight benefit */
#define INTERNED_STRING_CUSTOM_HASH 1 /* maybe an abbreviated hash function for long strings? */
#define INTERNED_STRING_INSTRUMENT 0 /* write out hash logs ... super super slow */

struct interned_string: public boost::string_ref
{
  // all these construction operations intern the incoming string
  interned_string(): boost::string_ref(), _c_str(0) {}
  interned_string(const char* value);
  interned_string(const std::string& value);
  interned_string(const boost::string_ref& value): boost::string_ref(value), _c_str(0) {}
  interned_string(const interned_string& value): boost::string_ref(value), _c_str(0) {}
  interned_string& operator = (const interned_string& value) {
    if (&value==this) return *this;
    boost::string_ref::operator = (value);
    // NB: don't propagate _c_str!
    return *this;
  }
  interned_string& operator = (const std::string& value);
  interned_string& operator = (const char* value);
  
  ~interned_string() { if (_c_str) free (_c_str); }
  
  // easy out-conversion operators
  operator std::string () const { return this->to_string(); }
  const char* c_str() const;

  // boost oversights
  template <typename F>
  size_t find (const F& f, size_t start_pos)
  {
    size_t x = this->substr(start_pos).find(f);
    if (x == boost::string_ref::npos)
      return x;
    else
      return x + start_pos;
  }
  
  template <typename F>
  size_t find (const F& f) const
  {
    return boost::string_ref::find(f);
  }
  
#if INTERNED_STRING_FIND_MEMMEM
  size_t find (const boost::string_ref& f) const;
  size_t find (const std::string& f) const;
  size_t find (const char *f) const;
#endif
  
private:
  mutable char *_c_str; // last value copied out
  interned_string intern(const std::string& value);
};




#endif // STRINGTABLE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
