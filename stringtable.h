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


struct interned_string: public boost::string_ref
{
  // all these construction operations intern the incoming string
  interned_string();
  interned_string(const char* value);
  interned_string(const std::string& value);
  interned_string(const boost::string_ref& value);
  interned_string(const interned_string& value);
  interned_string& operator = (const std::string& value);
  interned_string& operator = (const char* value);
  
  ~interned_string();
  
  // easy out-conversion operators
  operator std::string () const;

  const char* c_str() const;
private:
  mutable char *_c_str; // last value copied out
};


// interned_string intern(const std::string& value);


#endif // STRINGTABLE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
