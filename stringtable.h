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

boost::string_ref intern(const std::string& value);

#if 0
// some helpers for common usage
inline boost::string_ref intern(char value) {
  return intern(std::string(1, value));
}
inline boost::string_ref intern(boost::string_ref value1, char value2) {
  return intern(value1.to_string() + std::string(1, value2));
}
inline boost::string_ref intern(boost::string_ref value1, char value2, char value3) {
  return intern(value1.to_string() + std::string(1, value2) + std::string(1, value3));
}
#endif

#endif // STRINGTABLE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
