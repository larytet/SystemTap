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

#ifndef STAPREGEX_DFA_H
#define STAPREGEX_DFA_H

#include <string>
#include <iostream>
#include <set>
#include <list>
#include <vector>
#include <queue>
#include <utility>

#include "stapregex-defines.h"

class translator_output; /* from translator-output.h */

namespace stapregex {

struct regexp; /* from stapregex-tree.h */
union ins; /* from stapregex-tree.h */

struct dfa;
struct state;

/* Coordinates of a subexpression map item m[t,s]: */
typedef std::pair<unsigned, unsigned> map_item;

std::ostream& operator << (std::ostream &o, const map_item& m);

/* A tagged DFA transition can have a number of these instructions
   attached, which are able to assign the current position to specific
   map items, or to reorder the existing elements of the map: */
struct tdfa_insn {
  map_item to, from;
  bool save_tag; // -- if true, copy from's value to final tag
  bool save_pos; // -- if true, assign cur position; else, copy from's value
};
typedef std::list<tdfa_insn> tdfa_action;

std::ostream& operator << (std::ostream &o, const tdfa_action& a);

/* The arc_priority data type is a cunning way to represent transition
   priorities, necessitated by the fact that we have FORK opcodes (two
   outgoing e-transitions) which can lead to further FORK opcodes, &c,
   requiring a binary-subdivision style of priority assignment:

                -> 3/4 ... and so forth
               /
        -> 1/2 --> 2/4 ... and so forth
       /
      /       ---> 1/4 ... and so forth
     /       /
   0 ----> 0 ----> 0   ... and so forth

   Our trick is pretty much just to allocate the possible values of an
   unsigned long in binary-search fashion.

   XXX: For a 64-bit unsigned long type, this allows a chain of FORK
   opcodes around 64 units long (without intervening CHAR match
   insns), at which point things start to get funky. Be sure to keep
   an eye on whether this turns out to be enough in practice.

   TODOXXX: May want to test how this plays out in 32-bit architectures. */
typedef std::pair<unsigned long long, unsigned> arc_priority;
arc_priority refine_higher(const arc_priority& a);
arc_priority refine_lower(const arc_priority& a);
int arc_compare(const arc_priority& a, const arc_priority& b);

std::ostream& operator << (std::ostream &o, const arc_priority& p);

/* When constructing tagged DFA sets from ins, we need to keep track
   of a set of instructions together with further bookkeeping
   information (relative preference/priority, map items affected). */
struct kernel_point {
  ins *i;
  arc_priority priority; // -- used in tagged e-closure computation
  std::list<map_item> map_items;
  std::set<ins *> parents; // -- used for infinite-loop-detection
  void print (std::ostream &o, ins *base) const;
};
typedef std::list<kernel_point> state_kernel; // TODO: does it make sense to have duplicate ins inside a state-kernel?

/* Corresponds to a tagged-DFA transition arc, complete with
   subexpression map reordering and such. */
struct span {
  rchar lb, ub; // -- segment [lb, ub]
  state *to;
  tdfa_action action;
  state_kernel *jump_pairs; // -- kernel_points that jump to this span
  state_kernel *reach_pairs; // -- starting point for te_closure computation

  void emit_jump (translator_output *o, const dfa *d) const;
  void emit_final (translator_output *o, const dfa *d) const;
};

struct state {
  dfa *owner;     // -- dfa state was made for (XXX may not contain state yet)
  unsigned label; // -- index of state in dfa
  state *next;    // -- store dfa states as a linked list
  state_kernel *kernel; // -- set of corresponding ins coordinates
  /* NB: our usage of the term 'kernel' differs from re2c's slightly
     -- there is no real need to distinguish NFA edges inside the
     state from outgoing edges, (XXX) as far as I am aware. */

  bool accepts;   // -- is this a final state?
  unsigned accept_outcome;
  kernel_point *accept_kp;
  tdfa_action finalizer; // -- run after accepting

  std::list<span> spans;

  state (dfa *dfa, state_kernel *kernel);

  void emit (translator_output *o, const dfa *d) const;

  void print (translator_output *o) const;
  void print (std::ostream& o) const;
};

std::ostream& operator << (std::ostream &o, const state* s);

// ------------------------------------------------------------------------

struct dfa {
  ins *orig_nfa;
  state *first, *last; // -- store dfa states as a linked list
  unsigned nstates;

  // Infrastructure to deal with tagging:
  unsigned nmapitems;
  unsigned ntags;
  tdfa_action initializer; // -- run before entering start state
  std::vector<std::string> outcome_snippets;

  // When using tagged DFAs, record indices of the success and failure outcome:
  int success_outcome;
  int fail_outcome;

  dfa (ins *i, int ntags, std::vector<std::string>& outcome_snippets,
       int accept_outcome = 1);
  ~dfa ();

  void emit (translator_output *o) const;

  void emit_action (translator_output *o, const tdfa_action &act) const;
  void emit_tagsave (translator_output *o, std::string tag_states,
                     std::string tag_vals, std::string tag_count) const;

  void print (translator_output *o) const;
  void print (std::ostream& o) const;

private:
  void add_map_item (const map_item &m);
  state *add_state (state* s);
  state *find_equivalent (state *s, tdfa_action &r);
  tdfa_action compute_action (state_kernel *old_k, state_kernel *new_k);
  tdfa_action compute_finalizer (state *s);
};

std::ostream& operator << (std::ostream &o, const dfa& d);
std::ostream& operator << (std::ostream &o, const dfa* d);

/* Produces a dfa that runs the specified code snippets based on match
   or fail outcomes for an unanchored (by default) match of re. */
dfa *stapregex_compile (regexp *re, const std::string& match_snippet, const std::string& fail_snippet);

};

#endif

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
