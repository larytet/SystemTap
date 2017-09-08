// -*- C++ -*-
// Copyright (C) 2012-2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
//
// ---
//
// This file incorporates code from the re2c project; please see
// the file README.stapregex for details.

#ifndef STAPREGEX_PARSE_H
#define STAPREGEX_PARSE_H

#include <iostream>
#include <string>

#include "stapregex-defines.h"

namespace stapregex {

struct range; /* from stapregex-tree.h */
struct regexp; /* from stapregex-tree.h */

void print_escaped(std::ostream& o, rchar c);

struct cursor {
  const std::string *input;
  bool do_unescape;

  unsigned pos;      // pos of next char to be returned by next
  unsigned last_pos; // pos of last returned char

  bool finished;
  bool has(unsigned n); // n characters remaining?

  cursor();
  cursor(const std::string *input, bool do_unescape = false);
  rchar peek();
  rchar next();

private:
  rchar next_c;
  rchar last_c;
  void get_unescaped();
};

class regex_parser {
public:
  regex_parser (const std::string& input, bool do_unescape = true) 
    : input(input), do_unescape(do_unescape),
      do_tag(false), num_subexpressions(~0) {}
  regexp *parse (bool do_tag = true);

private:
  std::string input;
  bool do_unescape;

  cursor cur;
  bool do_tag;
  unsigned num_subexpressions;

  void parse_error (const std::string& msg, unsigned pos);
  void parse_error (const std::string& msg); // report error at last_pos

  // character classes
  bool isspecial (rchar c); // any of .[{()\*+?|^$

  // expectations
  void expect (rchar expected);

private: // nonterminals
  regexp *parse_expr ();
  regexp *parse_term ();
  regexp *parse_factor ();
  regexp *parse_char_range ();
  unsigned parse_number ();
};

/* Methods for parsing character classes: */
range *named_char_class (const std::string& name);
range *stapregex_getrange (cursor& cur);

};

#endif

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
