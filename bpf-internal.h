// bpf internal classes
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef BPF_INTERNAL_H
#define BPF_INTERNAL_H

#include <iostream>
#include <vector>
#include <cassert>
#include <unordered_set>
#include <unordered_map>
#include "bpf-bitset.h"
#include "staptree.h"

extern "C" {
#include <linux/bpf.h>
}

struct systemtap_session;
struct derived_probe;
struct vardecl;

namespace bpf {

typedef unsigned short regno;
static const regno max_regno = BPF_MAXINSNS;
static const regno noreg = -1;

typedef unsigned short opcode;

struct insn;

// BPF itself does not provide a full set of comparison codes.
// To make things easy for ourselves, emulate them.
enum condition
{
  EQ, NE, LT, LE, GT, GE, LTU, LEU, GTU, GEU, TEST
};

struct value
{
  enum value_type { UNINIT, IMM, HARDREG, TMPREG };

  value_type type	: 16;
  regno reg_val		: 16;
  int64_t imm_val;

  value(value_type t = UNINIT, regno r = noreg, int64_t c = 0)
    : type(t), reg_val(r), imm_val(c)
  { }

  static value mk_imm(int64_t i) { return value(IMM, noreg, i); }
  static value mk_reg(regno r) { return value(TMPREG, r); }
  static value mk_hardreg(regno r) { return value(HARDREG, r); }

  bool is_reg() const { return type >= HARDREG; }
  bool is_imm() const { return type == IMM; }
  regno reg() const { assert(is_reg()); return reg_val; }
  int64_t imm() const { assert(is_imm()); return imm_val; }

  std::ostream& print(std::ostream &) const;
};

inline std::ostream&
operator<< (std::ostream &o, const value &v)
{
  return v.print (o);
}

inline bool is_call(opcode c) { return c == (BPF_JMP | BPF_CALL); }
bool is_jmp(opcode c);
bool is_move(opcode c);
bool is_ldst(opcode c);
bool is_binary(opcode c);
bool is_commutative(opcode c);
const char *bpf_function_name (unsigned id);
unsigned bpf_function_nargs (unsigned id);

const opcode BPF_LD_MAP = BPF_LD | BPF_IMM | BPF_DW | (BPF_PSEUDO_MAP_FD << 8);

struct insn
{
  opcode code		: 16;	// The usual bpf opcode
  unsigned id		: 16;	// Context-dependent unique identifier
  signed off		: 16;	// The memory offset operand
  value *dest;			// The usual destination operand
  value *src0;			// The destination input, pre-allocation
  value *src1;			// The usual source register operand
  insn *prev, *next;		// Linked list of insns in the block

  insn();

  bool is_jmp() const { return bpf::is_jmp(code); }
  bool is_call() const { return bpf::is_call(code); }
  bool is_move() const { return bpf::is_move(code); }
  bool is_ldst() const { return bpf::is_ldst(code); }
  bool is_binary() const { return bpf::is_binary(code); }
  bool is_commutative() const { return bpf::is_commutative(code); }

  void mark_sets(bitset::set1_ref &s, bool v) const;
  void mark_uses(bitset::set1_ref &s, bool v) const;

  std::ostream& print(std::ostream &o) const;
};

inline std::ostream&
operator<< (std::ostream &o, const insn &i)
{
  return i.print (o);
}

struct block;
struct edge
{
  block *prev, *next;

  edge(block *p, block *n);
  ~edge();
  void redirect_next(block *n);
};

struct block
{
  typedef std::unordered_set<edge *> edge_set;
  edge_set prevs;		// All incoming edges

  insn *first, *last;		// Linked list of insns in the block
  edge *taken;			// Taken (conditional or not) edge
  edge *fallthru;		// Not taken conditional edge
  unsigned short id;		// Index in cfg

  block(int);
  ~block();
  block *is_forwarder() const;
  void print(std::ostream &o) const;
};

inline std::ostream&
operator<< (std::ostream &o, const block &b)
{
  b.print (o);
  return o;
}

class insn_inserter
{
private:
  insn_inserter();		// not present

public:
  block *b;
  insn *i;

  insn_inserter(block *bb, insn *ii) : b(bb), i(ii) { }
  virtual ~insn_inserter() { }
  virtual void insert(insn *i) = 0;

  insn *new_insn();
  block *get_block() const { return b; }
  insn_inserter& operator++ () { if (i) i = i->next; return *this; }
  insn_inserter& operator-- () { if (i) i = i->prev; return *this; }
  operator bool () const { return i != NULL; }
};

struct insn_before_inserter : public insn_inserter
{
  insn_before_inserter() : insn_inserter(NULL, NULL) { }
  insn_before_inserter(block *b, insn *i) : insn_inserter(b,i) { }
  virtual void insert(insn *i);
};

struct insn_after_inserter : public insn_inserter
{
  insn_after_inserter() : insn_inserter(NULL, NULL) { }
  insn_after_inserter(block *b, insn *i) : insn_inserter(b,i) { }
  virtual void insert(insn *i);
};

struct insn_append_inserter : public insn_after_inserter
{
  insn_append_inserter() : insn_after_inserter(NULL, NULL) { }
  insn_append_inserter(block *b) : insn_after_inserter(b, NULL) { }
};

struct program
{
  std::vector<block *> blocks;	// All blocks in the program
  block *new_block();

  std::vector<value> hardreg_vals;
  std::vector<value *> reg_vals;
  std::unordered_map<int64_t, value *> imm_map;

  regno max_reg() const { return reg_vals.size() + MAX_BPF_REG; }
  value *lookup_reg(regno r);
  value *new_reg();
  value *new_imm(int64_t);

  // The BPF local stack is [0, -512] indexed off BPF_REG_10.
  // The translator has dibs on the low bytes, [0, -max_tmp_space],
  // for use with various function calls that pass data by reference.
  // The register allocator may use [-max_tmp_space, -512] for spills.
  unsigned max_tmp_space;
  void use_tmp_space(unsigned bytes)
  {
    if (max_tmp_space < bytes)
      max_tmp_space = bytes;
  }

  void mk_ld(insn_inserter &ins, int sz, value *dest, value *base, int off);
  void mk_st(insn_inserter &ins, int sz, value *base, int off, value *src);
  void mk_unary(insn_inserter &ins, opcode op, value *dest, value *src);
  void mk_binary(insn_inserter &ins, opcode op, value *d,
		 value *s0, value *s1);
  void mk_mov(insn_inserter &ins, value *dest, value *src);
  void mk_call(insn_inserter &ins, enum bpf_func_id id, unsigned nargs);
  void mk_exit(insn_inserter &ins);
  void mk_jmp(insn_inserter &ins, block *dest);
  void mk_jcond(insn_inserter &ins, condition c, value *s0, value *s1,
		block *t, block *f);
  void load_map(insn_inserter &ins, value *dest, int src);

  program();
  ~program();

  void generate();
  void print(std::ostream &) const;
};

inline std::ostream&
operator<< (std::ostream &o, const program &c)
{
  c.print (o);
  return o;
}

struct globals
{
  // This is an index within a numbered bpf_map.
  typedef std::pair<unsigned short, unsigned short> map_slot;

  // This associates a global variable with a given slot.
  typedef std::unordered_map<vardecl *, map_slot> globals_map;

  // This defines the shape of each bpf_map.
  struct bpf_map_def
  {
    unsigned type;
    unsigned key_size;
    unsigned value_size;
    unsigned max_entries;
    unsigned map_flags;
  };
  typedef std::vector<bpf_map_def> map_vect;

  // The maps required by the bpf program, and how the global variables
  // index into these maps.
  map_vect maps;
  globals_map globals;

  bool empty() { return this->globals.empty(); }

  // Index into globals. This element represents the map of internal globals
  // used for communication between stapbpf and kernel-side bpf programs.
  static const int internal_map_idx = 0;

  // Indicates whether exit() has been called from within a bpf program.
  struct vardecl internal_exit;

  // Indexes into the bpf map of internal globals.
  enum internal_global_idx
  {
    EXIT = 0,
    NUM_INTERNALS, // non-ABI
  };
};
} // namespace bpf

#endif // BPF_INTERNAL_H
