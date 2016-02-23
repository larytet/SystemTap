// -*- C++ -*-
// Copyright (C) 2005-2014 Red Hat Inc.
// Copyright (C) 2007 Bull S.A.S
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.


#ifndef PARSE_H
#define PARSE_H

#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include "stringtable.h"


struct systemtap_session;
struct stapfile;
struct probe;

struct source_loc
{
  stapfile* file;
  unsigned line;
  unsigned column;
public:
  source_loc(): file(0), line(0), column(0) {}
};

std::ostream& operator << (std::ostream& o, const source_loc& loc);

enum parse_context
  {
    con_unknown, con_probe, con_global, con_function, con_embedded
  };


enum token_type
  {
    tok_junk, tok_identifier, tok_operator, tok_string, tok_number,
    tok_embedded, tok_keyword
  };

// detailed tok_junk
enum token_junk_type
  {
    tok_junk_unknown,
    tok_junk_nested_arg,
    tok_junk_invalid_arg,
    tok_junk_unclosed_quote,
    tok_junk_unclosed_embedded,
  };


struct token
{
  source_loc location;
  interned_string content;
  const token* chain; // macro invocation that produced this token
  token_type type;
  token_junk_type junk_type;

  std::string junk_message(systemtap_session& session) const;
  
  friend class parser;
  friend class lexer;
private:
  void make_junk (token_junk_type);
  token(): chain(0), type(tok_junk), junk_type(tok_junk_unknown) {}
  token(const token& other):
    location(other.location), content(other.content),
    chain(other.chain), type(other.type), junk_type(other.junk_type) {}
};


std::ostream& operator << (std::ostream& o, const token& t);


typedef enum { ctx_library, ctx_local } macro_ctx;

/* structs from session.h: */
struct macrodecl {
  const token* tok; // NB: macrodecl owns its token
  std::string name;
  std::vector<std::string> formal_args;
  std::vector<const token*> body;
  macro_ctx context;

  // Used for identifying subclasses that represent e.g. parameter bindings.
  virtual bool is_closure() { return false; }

  macrodecl () : tok(0), context(ctx_local) { }
  virtual ~macrodecl ();
};


enum parse_flag
  {
    pf_guru = 1,
    pf_no_compatible = 2,
    pf_squash_errors = 4,
    pf_user_file = 8,
  };


stapfile* parse (systemtap_session& s,const std::string& n, std::istream& i, unsigned flags);
stapfile* parse (systemtap_session& s, const std::string& n, unsigned flags);

stapfile* parse_library_macros (systemtap_session& s, const std::string& n);

probe* parse_synthetic_probe (systemtap_session &s, std::istream& i, const token* tok);

#endif // PARSE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
