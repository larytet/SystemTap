// -*- C++ -*-
// Copyright (C) 2015 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.


#ifndef STRINGTABLE_H
#define STRINGTABLE_H

#include <functional>
#include <string>
#include <cstring>

// TODO use C++17's std::string_view when possible.  It even hashes natively.
// (some compilers already have std::experimental::string_view)

#if defined(HAVE_BOOST_UTILITY_STRING_REF_HPP)
#include <boost/version.hpp>
#include <boost/utility/string_ref.hpp> //header with string_ref

// XXX: experimental tunables
#define INTERNED_STRING_FIND_MEMMEM 1 /* perf stat indicates a very slight benefit */
#define INTERNED_STRING_CUSTOM_HASH 1 /* maybe an abbreviated hash function for long strings? */
#define INTERNED_STRING_INSTRUMENT 0 /* write out hash logs ... super super slow */

struct interned_string: public boost::string_ref
{
  // all these construction operations intern the incoming string
  interned_string(): boost::string_ref() {}
  interned_string(const char* value):
    boost::string_ref(intern(value)) {}
  interned_string(const std::string& value):
    boost::string_ref(intern(value)) {}
  interned_string& operator = (const std::string& value)
    { return *this = intern(value); }
  interned_string& operator = (const char* value)
    { return *this = intern(value); }

#if BOOST_VERSION < 105400
  std::string to_string () const { return std::string(this->data(), this->size()); }

  // some comparison operators that aren't available in boost 1.53
  bool operator == (const char* y) { return compare(boost::string_ref(y)) == 0; }
  bool operator == (const std::string& y) { return compare(boost::string_ref(y)) == 0; }
  friend bool operator == (interned_string x, interned_string y) { return x.compare(y) == 0; }
  friend  bool operator == (const char * x, interned_string y)
  {
    return y.compare(boost::string_ref(x)) == 0;
  }
  friend bool operator == (const std::string& x, interned_string y)
  {
    return y.compare(boost::string_ref(x)) == 0;
  }

  bool operator != (const char* y) { return compare(boost::string_ref(y)) != 0; }
  bool operator != (const std::string& y) { return compare(boost::string_ref(y)) != 0; }
  friend bool operator != (interned_string x, interned_string y) { return x.compare(y) != 0; }
  friend  bool operator != (const char * x, interned_string y)
  {
    return y.compare(boost::string_ref(x)) != 0;
  }
  friend bool operator != (const std::string& x, interned_string y)
  {
    return y.compare(boost::string_ref(x)) != 0;
  }
#endif

  // easy out-conversion operators
  operator std::string () const { return this->to_string(); }

  // return an efficient substring reference
  interned_string substr(size_t pos = 0, size_t len = npos) const
  {
    return boost::string_ref::substr(pos, len);
  }

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

  size_t find (const interned_string& f) const
  {
    return find (static_cast<const boost::string_ref&> (f));
  }
  
private:
  static interned_string intern(const std::string& value);
  static interned_string intern(const char* value);
  static interned_string intern(char value);

  // This is private so we can be sure of ownership, from our interned string table.
  interned_string(const boost::string_ref& value): boost::string_ref(value) {}
};

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

private:
  // c_str is not allowed on boost::string_ref, so add a private unimplemented
  // declaration here to prevent the use of string::c_str accidentally.
  const char* c_str() const;
};

namespace std {
  template<> struct hash<interned_string> {
    size_t operator() (interned_string const& s) const
    {
      // strings are directly hashable
      return hash<string>()(s);
    }
  };
}

#endif /* defined(HAVE_BOOST_UTILITY_STRING_REF_HPP) */

#endif // STRINGTABLE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
