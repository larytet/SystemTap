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
#include <cstring>

#if defined(HAVE_BOOST_UTILITY_STRING_REF_HPP)
#include <boost/utility/string_ref.hpp> //header with string_ref

// XXX: experimental tunables
#define INTERNED_STRING_FIND_MEMMEM 1 /* perf stat indicates a very slight benefit */
#define INTERNED_STRING_CUSTOM_HASH 1 /* maybe an abbreviated hash function for long strings? */
#define INTERNED_STRING_INSTRUMENT 0 /* write out hash logs ... super super slow */

struct interned_string: public boost::string_ref
{
  // all these construction operations intern the incoming string
  interned_string(): boost::string_ref() {}
  interned_string(const char* value);
  interned_string(const std::string& value);
  interned_string(const boost::string_ref& value): boost::string_ref(value) {}
  interned_string(const interned_string& value): boost::string_ref(value) {}
  interned_string& operator = (const std::string& value);
  interned_string& operator = (const char* value);
  
  // easy out-conversion operators
  operator std::string () const { return this->to_string(); }

  // NB: this is OK because we always use a std::string as a backing
  // store, and as of c++0x, those always store a \0 in their data().
  const char* c_str() const { return this->size() ? (const char*)this->data() : ""; }
  // NB: the above property holds only if we don't expose the efficient
  // boost::string_ref substrings.  We can either have efficient
  // substrings -xor- implicit \0 terminators - not both.
  interned_string substr(size_t pos = 0, size_t len = npos) const
  {
    return interned_string::intern (boost::string_ref::substr(pos, len).to_string());
  }

  // boost oversights
  template <typename F>
  size_t find (const F& f, size_t start_pos)
  {
    size_t x = this->boost::string_ref::substr(start_pos).find(f); // don't intern substring unnecessarily
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
  static interned_string intern(const std::string& value);
};
#else /* !defined(HAVE_BOOST_UTILITY_STRING_REF_HPP) */

struct interned_string : public std::string {
  interned_string(): std::string() {}
  interned_string(const char* value): std::string (value ? :"") {}
  interned_string(const std::string& value): std::string(value) {}
  std::string to_string() const {return (std::string) *this; }
  void remove_prefix (size_t n) {*this = this->substr(n);}
  interned_string substr(size_t pos = 0, size_t len = npos) const
  {
    return (interned_string)std::string::substr(pos, len);
  }
  bool starts_with(const char* value) const
  {
    return (this->compare(0, std::strlen(value), value) == 0);
  }

  bool starts_with(const std::string& value) const
  {
    return (this->compare(0, value.length(), value) == 0);
  }
};

#endif /* defined(HAVE_BOOST_UTILITY_STRING_REF_HPP) */


#endif // STRINGTABLE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
