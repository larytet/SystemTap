// bpf translation pass
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include "bpf-internal.h"
#include "elaborate.h"
#include "session.h"

using namespace std;

namespace bpf {

std::ostream &
value::print(std::ostream &o) const
{
  switch (type)
    {
    case UNINIT:
      return o << "#";
    case IMM:
      return o << "$" << imm_val;
    case HARDREG:
      return o << "r" << reg_val;
    case TMPREG:
      return o << "t" << reg_val;
    default:
      abort();
    }
}

insn::insn()
  : code(-1), id(0), off(0),
    dest(NULL), src0(NULL), src1(NULL),
    prev(NULL), next(NULL)
{ }

bool
is_jmp(opcode code)
{
  if (BPF_CLASS (code) != BPF_JMP)
    return false;
  switch (BPF_OP (code))
    {
    case BPF_JA:
    case BPF_JEQ:
    case BPF_JGT:
    case BPF_JGE:
    case BPF_JSET:
    case BPF_JNE:
    case BPF_JSGT:
    case BPF_JSGE:
      return true;
    default:
      return false;
    }
}

bool
is_move(opcode c)
{
  switch (c)
    {
    case BPF_ALU64 | BPF_MOV | BPF_X:
    case BPF_ALU64 | BPF_MOV | BPF_K:
    case BPF_ALU | BPF_MOV | BPF_K:
    case BPF_LD | BPF_IMM | BPF_DW:
    case BPF_LD_MAP:
      return true;
    default:
      return false;
    }
}

bool
is_ldst(opcode c)
{
  switch (BPF_CLASS (c))
    {
    case BPF_LDX:
    case BPF_ST:
    case BPF_STX:
      return true;
    default:
      return false;
    }
}

bool
is_binary(opcode code)
{
  if (BPF_CLASS (code) != BPF_ALU64)
    return false;
  switch (BPF_OP (code))
    {
    case BPF_ADD:
    case BPF_SUB:
    case BPF_AND:
    case BPF_OR:
    case BPF_LSH:
    case BPF_RSH:
    case BPF_XOR:
    case BPF_MUL:
    case BPF_ARSH:
    case BPF_DIV:
    case BPF_MOD:
      return true;
    default:
      return false;
    }
}

bool
is_commutative(opcode code)
{
  if (BPF_CLASS (code) != BPF_ALU64)
    return false;
  switch (BPF_OP (code))
    {
    case BPF_ADD:
    case BPF_AND:
    case BPF_OR:
    case BPF_XOR:
    case BPF_MUL:
      return true;
    default:
      return false;
    }
}

const char *
bpf_function_name (unsigned id)
{
  switch (id)
    {
    case BPF_FUNC_map_lookup_elem:	return "map_lookup_elem";
    case BPF_FUNC_map_update_elem:	return "map_update_elem";
    case BPF_FUNC_map_delete_elem:	return "map_delete_elem";
    case BPF_FUNC_probe_read:		return "probe_read";
    case BPF_FUNC_ktime_get_ns:		return "ktime_get_ns";
    case BPF_FUNC_trace_printk:		return "trace_printk";
    case BPF_FUNC_get_prandom_u32:	return "get_prandom_u32";
    case BPF_FUNC_get_smp_processor_id:	return "get_smp_processor_id";
    case BPF_FUNC_get_current_pid_tgid:	return "get_current_pid_tgid";
    case BPF_FUNC_get_current_uid_gid:	return "get_current_uid_gid";
    case BPF_FUNC_get_current_comm:	return "get_current_comm";
    case BPF_FUNC_perf_event_read:	return "perf_event_read";
    case BPF_FUNC_perf_event_output:	return "perf_event_output";
    default:				return NULL;
    }
}

unsigned
bpf_function_nargs (unsigned id)
{
  switch (id)
    {
    case BPF_FUNC_map_lookup_elem:	return 2;
    case BPF_FUNC_map_update_elem:	return 4;
    case BPF_FUNC_map_delete_elem:	return 2;
    case BPF_FUNC_probe_read:		return 3;
    case BPF_FUNC_ktime_get_ns:		return 0;
    case BPF_FUNC_trace_printk:		return 5;
    case BPF_FUNC_get_prandom_u32:	return 0;
    case BPF_FUNC_get_smp_processor_id:	return 0;
    case BPF_FUNC_get_current_pid_tgid:	return 0;
    case BPF_FUNC_get_current_uid_gid:	return 0;
    case BPF_FUNC_get_current_comm:	return 2;
    case BPF_FUNC_perf_event_read:	return 2;
    case BPF_FUNC_perf_event_output:	return 5;
    default:				return 5;
    }
}


void
insn::mark_sets(bitset::set1_ref &s, bool v) const
{
  if (is_call())
    {
      // Return value and call-clobbered registers.
      for (unsigned i = BPF_REG_0; i <= BPF_REG_5; ++i)
	s.set(i, v);
    }
  else if (dest)
    s.set(dest->reg(), v);
}

void
insn::mark_uses(bitset::set1_ref &s, bool v) const
{
  if (is_call())
    {
      unsigned n = off;
      for (unsigned i = 0; i < n; ++i)
	s.set(BPF_REG_1 + i, v);
    }
  else if (code == (BPF_JMP | BPF_EXIT))
    s.set(BPF_REG_0, v);
  else
    {
      if (src0 && src0->is_reg())
	s.set(src0->reg(), v);
      if (src1 && src1->is_reg())
	s.set(src1->reg(), v);
    }
}

static const char *
opcode_name(opcode op)
{
  const char *opn;

  switch (op)
    {
    case BPF_LDX | BPF_MEM | BPF_B:	opn = "ldxb"; break;
    case BPF_LDX | BPF_MEM | BPF_H:	opn = "ldxh"; break;
    case BPF_LDX | BPF_MEM | BPF_W:	opn = "ldxw"; break;
    case BPF_LDX | BPF_MEM | BPF_DW:	opn = "ldx"; break;

    case BPF_STX | BPF_MEM | BPF_B:	opn = "stxb"; break;
    case BPF_STX | BPF_MEM | BPF_H:	opn = "stxh"; break;
    case BPF_STX | BPF_MEM | BPF_W:	opn = "stxw"; break;
    case BPF_STX | BPF_MEM | BPF_DW:	opn = "stx"; break;

    case BPF_ST | BPF_MEM | BPF_B:	opn = "stkb"; break;
    case BPF_ST | BPF_MEM | BPF_H:	opn = "stkh"; break;
    case BPF_ST | BPF_MEM | BPF_W:	opn = "stkw"; break;
    case BPF_ST | BPF_MEM | BPF_DW:	opn = "stk"; break;

    case BPF_ALU64 | BPF_ADD | BPF_X:	opn = "addx"; break;
    case BPF_ALU64 | BPF_ADD | BPF_K:	opn = "addk"; break;
    case BPF_ALU64 | BPF_SUB | BPF_X:	opn = "subx"; break;
    case BPF_ALU64 | BPF_SUB | BPF_K:	opn = "subk"; break;
    case BPF_ALU64 | BPF_AND | BPF_X:	opn = "andx"; break;
    case BPF_ALU64 | BPF_AND | BPF_K:	opn = "andk"; break;
    case BPF_ALU64 | BPF_OR  | BPF_X:	opn = "orx"; break;
    case BPF_ALU64 | BPF_OR  | BPF_K:	opn = "ork"; break;
    case BPF_ALU64 | BPF_LSH | BPF_X:	opn = "lshx"; break;
    case BPF_ALU64 | BPF_LSH | BPF_K:	opn = "lshk"; break;
    case BPF_ALU64 | BPF_RSH | BPF_X:	opn = "rshx"; break;
    case BPF_ALU64 | BPF_RSH | BPF_K:	opn = "rshk"; break;
    case BPF_ALU64 | BPF_XOR | BPF_X:	opn = "xorx"; break;
    case BPF_ALU64 | BPF_XOR | BPF_K:	opn = "xork"; break;
    case BPF_ALU64 | BPF_MUL | BPF_X:	opn = "mulx"; break;
    case BPF_ALU64 | BPF_MUL | BPF_K:	opn = "mulk"; break;
    case BPF_ALU64 | BPF_MOV | BPF_X:	opn = "movx"; break;
    case BPF_ALU64 | BPF_MOV | BPF_K:	opn = "movk"; break;
    case BPF_ALU64 | BPF_ARSH | BPF_X:	opn = "arshx"; break;
    case BPF_ALU64 | BPF_ARSH | BPF_K:	opn = "arshk"; break;
    case BPF_ALU64 | BPF_DIV | BPF_X:	opn = "divx"; break;
    case BPF_ALU64 | BPF_DIV | BPF_K:	opn = "divk"; break;
    case BPF_ALU64 | BPF_MOD | BPF_X:	opn = "modx"; break;
    case BPF_ALU64 | BPF_MOD | BPF_K:	opn = "modk"; break;
    case BPF_ALU64 | BPF_NEG:		opn = "negx"; break;

    case BPF_ALU | BPF_MOV | BPF_X:	opn = "movwx"; break;
    case BPF_ALU | BPF_MOV | BPF_K:	opn = "movwk"; break;

    case BPF_LD | BPF_IMM | BPF_DW:	opn = "movdk"; break;
    case BPF_LD_MAP:			opn = "movmap"; break;

    case BPF_JMP | BPF_CALL:		opn = "call"; break;
    case BPF_JMP | BPF_CALL | BPF_X:	opn = "tcall"; break;
    case BPF_JMP | BPF_EXIT:		opn = "exit"; break;

    case BPF_JMP | BPF_JA:		opn = "jmp"; break;
    case BPF_JMP | BPF_JEQ | BPF_X:	opn = "jeqx"; break;
    case BPF_JMP | BPF_JEQ | BPF_K:	opn = "jeqk"; break;
    case BPF_JMP | BPF_JNE | BPF_X:	opn = "jnex"; break;
    case BPF_JMP | BPF_JNE | BPF_K:	opn = "jnek"; break;
    case BPF_JMP | BPF_JGT | BPF_X:	opn = "jugtx"; break;
    case BPF_JMP | BPF_JGT | BPF_K:	opn = "jugtk"; break;
    case BPF_JMP | BPF_JGE | BPF_X:	opn = "jugex"; break;
    case BPF_JMP | BPF_JGE | BPF_K:	opn = "jugek"; break;
    case BPF_JMP | BPF_JSGT | BPF_X:	opn = "jsgtx"; break;
    case BPF_JMP | BPF_JSGT | BPF_K:	opn = "jsgtk"; break;
    case BPF_JMP | BPF_JSGE | BPF_X:	opn = "jsgex"; break;
    case BPF_JMP | BPF_JSGE | BPF_K:	opn = "jsgek"; break;
    case BPF_JMP | BPF_JSET | BPF_X:	opn = "jsetx"; break;
    case BPF_JMP | BPF_JSET | BPF_K:	opn = "jsetk"; break;

    default:
      abort();
    }

  return opn;
}

std::ostream &
insn::print(std::ostream &o) const
{
  const char *opn = opcode_name (code);

  switch (code)
    {
    case BPF_LDX | BPF_MEM | BPF_B:
    case BPF_LDX | BPF_MEM | BPF_H:
    case BPF_LDX | BPF_MEM | BPF_W:
    case BPF_LDX | BPF_MEM | BPF_DW:
      return o << opn << "\t" << *dest
	       << ",[" << *src1
	       << showpos << off << noshowpos << "]";

    case BPF_STX | BPF_MEM | BPF_B:
    case BPF_STX | BPF_MEM | BPF_H:
    case BPF_STX | BPF_MEM | BPF_W:
    case BPF_STX | BPF_MEM | BPF_DW:
    case BPF_ST | BPF_MEM | BPF_B:
    case BPF_ST | BPF_MEM | BPF_H:
    case BPF_ST | BPF_MEM | BPF_W:
    case BPF_ST | BPF_MEM | BPF_DW:
      return o << opn << "\t[" << *src0
	       << showpos << off << noshowpos
	       << "]," << *src1;

    case BPF_ALU | BPF_MOV | BPF_X:
    case BPF_ALU | BPF_MOV | BPF_K:
    case BPF_ALU64 | BPF_MOV | BPF_X:
    case BPF_ALU64 | BPF_MOV | BPF_K:
    case BPF_LD | BPF_IMM | BPF_DW:
    case BPF_LD_MAP:
      return o << opn << "\t" << *dest << "," << *src1;

    case BPF_ALU64 | BPF_NEG:
      return o << opn << "\t" << *dest << "," << *src0;

    case BPF_ALU64 | BPF_ADD | BPF_X:
    case BPF_ALU64 | BPF_ADD | BPF_K:
    case BPF_ALU64 | BPF_SUB | BPF_X:
    case BPF_ALU64 | BPF_SUB | BPF_K:
    case BPF_ALU64 | BPF_AND | BPF_X:
    case BPF_ALU64 | BPF_AND | BPF_K:
    case BPF_ALU64 | BPF_OR  | BPF_X:
    case BPF_ALU64 | BPF_OR  | BPF_K:
    case BPF_ALU64 | BPF_LSH | BPF_X:
    case BPF_ALU64 | BPF_LSH | BPF_K:
    case BPF_ALU64 | BPF_RSH | BPF_X:
    case BPF_ALU64 | BPF_RSH | BPF_K:
    case BPF_ALU64 | BPF_XOR | BPF_X:
    case BPF_ALU64 | BPF_XOR | BPF_K:
    case BPF_ALU64 | BPF_MUL | BPF_X:
    case BPF_ALU64 | BPF_MUL | BPF_K:
    case BPF_ALU64 | BPF_ARSH | BPF_X:
    case BPF_ALU64 | BPF_ARSH | BPF_K:
    case BPF_ALU64 | BPF_DIV | BPF_X:
    case BPF_ALU64 | BPF_DIV | BPF_K:
    case BPF_ALU64 | BPF_MOD | BPF_K:
    case BPF_ALU64 | BPF_MOD | BPF_X:
      return o << opn << "\t" << *dest << "," << *src0 << "," << *src1;

    case BPF_JMP | BPF_CALL:
    case BPF_JMP | BPF_CALL | BPF_X:
      o << opn << "\t";
      if (const char *name = bpf_function_name(src1->imm()))
	o << name;
      else
	o << *src1;
      return o << "," << off;

    case BPF_JMP | BPF_EXIT:
    case BPF_JMP | BPF_JA:
      return o << opn;

    case BPF_JMP | BPF_JEQ | BPF_X:
    case BPF_JMP | BPF_JEQ | BPF_K:
    case BPF_JMP | BPF_JNE | BPF_X:
    case BPF_JMP | BPF_JNE | BPF_K:
    case BPF_JMP | BPF_JGT | BPF_X:
    case BPF_JMP | BPF_JGT | BPF_K:
    case BPF_JMP | BPF_JGE | BPF_X:
    case BPF_JMP | BPF_JGE | BPF_K:
    case BPF_JMP | BPF_JSGT | BPF_X:
    case BPF_JMP | BPF_JSGT | BPF_K:
    case BPF_JMP | BPF_JSGE | BPF_X:
    case BPF_JMP | BPF_JSGE | BPF_K:
    case BPF_JMP | BPF_JSET | BPF_X:
    case BPF_JMP | BPF_JSET | BPF_K:
      return o << opn << "\t" << *src0 << "," << *src1;

    default:
      abort ();
    }
}

edge::edge(block *p, block *n)
  : prev(p), next(n)
{
  n->prevs.insert (this);
}

edge::~edge()
{
  next->prevs.erase (this);
  if (prev->taken == this)
    prev->taken = NULL;
  if (prev->fallthru == this)
    prev->fallthru = NULL;
}

void
edge::redirect_next(block *n)
{
  next->prevs.erase (this);
  next = n;
  n->prevs.insert (this);
}

block::block(int i)
  : first(NULL), last(NULL), taken(NULL), fallthru(NULL), id(i)
{ }

block::~block()
{
  for (insn *n, *i = first; i ; i = n)
    {
      n = i->next;
      delete i;
    }
  delete taken;
  delete fallthru;
}

block *
block::is_forwarder() const
{
  if (first == NULL)
    {
      if (fallthru)
	return fallthru->next;
    }
  else if (first == last && first->code == (BPF_JMP | BPF_JA))
    return taken->next;
  return NULL;
}

void
block::print(ostream &o) const
{
  if (prevs.empty ())
    o << "\t[prevs: entry]\n";
  else
    {
      o << "\t[prevs:";
      for (edge_set::const_iterator i = prevs.begin(); i != prevs.end(); ++i)
	o << ' ' << (*i)->prev->id;
      o << "]\n";
    }

  o << id << ':' << endl;
  for (insn *i = first; i != NULL; i = i->next)
    o << '\t' << *i << endl;

  if (taken)
    o << "\t[taken: " << taken->next->id << "]" << endl;
  if (fallthru)
    o << "\t[fallthru: " << fallthru->next->id << "]" << endl;
  else if (!taken)
    o << "\t[end]" << endl;
}

insn *
insn_inserter::new_insn()
{
  insn *n = new insn;
  insert(n);
  return n;
}

void
insn_before_inserter::insert(insn *n)
{
  assert(i != NULL);
  insn *p = i->prev;
  i->prev = n;
  n->prev = p;
  n->next = i;
  if (p == NULL)
    b->first = n;
  else
    p->next = n;
}

void
insn_after_inserter::insert(insn *p)
{
  if (i == NULL)
    {
      assert(b->first == NULL && b->last == NULL);
      b->first = b->last = p;
    }
  else
    {
      insn *n = i->next;
      i->next = p;
      p->prev = i;
      p->next = n;
      if (n == NULL)
	b->last = p;
      else
	n->prev = p;
    }
  i = p;
}

program::program()
  : hardreg_vals(MAX_BPF_REG)
{
  for (unsigned i = 0; i < MAX_BPF_REG; ++i)
    hardreg_vals[i] = value::mk_hardreg(i);
}

program::~program()
{
  for (auto i = blocks.begin (); i != blocks.end (); ++i)
    delete *i;
  for (auto i = reg_vals.begin (); i != reg_vals.end (); ++i)
    delete *i;
  for (auto i = imm_map.begin (); i != imm_map.end (); ++i)
    delete i->second;
}

block *
program::new_block ()
{
  block *r = new block(blocks.size ());
  blocks.push_back (r);
  return r;
}

value *
program::lookup_reg(regno r)
{
  if (r < MAX_BPF_REG)
    return &hardreg_vals[r];
  else
    return reg_vals[r - MAX_BPF_REG];
}

value *
program::new_reg()
{
  regno r = max_reg();
  value *v = new value(value::mk_reg(r));
  reg_vals.push_back(v);
  return v;
}

value *
program::new_imm(int64_t i)
{
  auto old = imm_map.find(i);
  if (old != imm_map.end())
    return old->second;

  value *v = new value(value::mk_imm(i));
  auto ok = imm_map.insert(std::pair<int64_t, value *>(i, v));
  assert(ok.second);
  return v;
}

void
program::mk_ld(insn_inserter &ins, int sz, value *dest, value *base, int off)
{
  insn *i = ins.new_insn();
  i->code = BPF_LDX | BPF_MEM | sz;
  i->off = off;
  i->dest = dest;
  i->src1 = base;
}

void
program::mk_st(insn_inserter &ins, int sz, value *base, int off, value *src)
{
  insn *i = ins.new_insn();
  i->code = (src->is_imm() ? BPF_ST : BPF_STX) | BPF_MEM | sz;
  i->off = off;
  i->src0 = base;
  i->src1 = src;
}

void
program::mk_binary(insn_inserter &ins, opcode op, value *dest,
		   value *s0, value *s1)
{
  if (op == BPF_SUB)
    {
      if (s0->is_imm() && s0->imm() == 0)
	{
	  mk_unary(ins, BPF_NEG, dest, s1);
	  return;
	}
    }
  else if (is_commutative(op)
	   && ((s1->is_reg() && !s0->is_reg()) || dest == s1))
    std::swap (s1, s0);

  insn *i = ins.new_insn();
  i->code = BPF_ALU64 | op | (s1->is_imm() ? BPF_K : BPF_X);
  i->dest = dest;
  i->src0 = s0;
  i->src1 = s1;
}

void
program::mk_unary(insn_inserter &ins, opcode op, value *dest, value *src)
{
  insn *i = ins.new_insn();
  i->code = BPF_ALU64 | BPF_X | op;
  i->dest = dest;
  i->src0 = src;
}

void
program::mk_mov(insn_inserter &ins, value *dest, value *src)
{
  if (dest == src)
    return;

  opcode code = BPF_ALU64 | BPF_MOV | BPF_X;
  if (src->is_imm())
    {
      int64_t i = src->imm();
      if (i == (int32_t)i)
	code = BPF_ALU64 | BPF_MOV | BPF_K;
      else if (i == (uint32_t)i)
	code = BPF_ALU | BPF_MOV | BPF_K;
      else
	code = BPF_LD | BPF_IMM | BPF_DW;
    }

  insn *i = ins.new_insn();
  i->code = code;
  i->dest = dest;
  i->src1 = src;
}

void
program::mk_jmp(insn_inserter &ins, block *dest)
{
  insn *i = ins.new_insn();
  i->code = BPF_JMP | BPF_JA;

  block *b = ins.get_block();
  b->taken = new edge(b, dest);
}

void
program::mk_call(insn_inserter &ins, enum bpf_func_id id, unsigned nargs)
{
  insn *i = ins.new_insn();
  i->code = BPF_JMP | BPF_CALL;
  i->src1 = new_imm((int)id);
  i->off = nargs;
}

void
program::mk_exit(insn_inserter &ins)
{
  insn *i = ins.new_insn();
  i->code = BPF_JMP | BPF_EXIT;
}

void
program::mk_jcond(insn_inserter &ins, condition c, value *s0, value *s1,
		  block *t, block *f)
{
  bool inv = false;
  opcode code;

  if (s1->is_reg() && !s0->is_reg())
    {
      std::swap (s1, s0);
      switch (c)
	{
	case EQ:	break;
	case NE:	break;
	case TEST:	break;
	case LT:	c = GT; break;
	case LE:	c = GE; break;
	case GT:	c = LT; break;
	case GE:	c = LE; break;
	case LTU:	c = GTU; break;
	case LEU:	c = GEU; break;
	case GTU:	c = LTU; break;
	case GEU:	c = LEU; break;
	default:	abort();
	}
    }

  switch (c)
    {
    case EQ:
      code = BPF_JEQ;
      break;
    case NE:
      code = BPF_JNE;
      break;
    case LE:
      inv = true;
      /* Fallthrough */
    case GT:
      code = BPF_JSGT;
      break;
    case LT:
      inv = true;
      /* Fallthrough */
    case GE:
      code = BPF_JSGE;
      break;
    case LEU:
      inv = true;
      /* Fallthrough */
    case GTU:
      code = BPF_JGT;
      break;
    case LTU:
      inv = true;
      /* Fallthrough */
    case GEU:
      code = BPF_JGE;
      break;
    case TEST:
      code = BPF_JSET;
      break;
    default:
      abort ();
    }

  if (inv)
    std::swap (t, f);

  block *b = ins.get_block();
  b->taken = new edge(b, t);
  b->fallthru = new edge(b, f);

  insn *i = ins.new_insn();
  i->code = BPF_JMP | code | (s1->is_imm() ? BPF_K : BPF_X);
  i->src0 = s0;
  i->src1 = s1;
}

void
program::load_map(insn_inserter &ins, value *dest, int src)
{
  insn *i = ins.new_insn();
  i->code = BPF_LD_MAP;
  i->dest = dest;
  i->src1 = new_imm(src);
}

void
program::print(ostream &o) const
{
  for (unsigned n = blocks.size(), i = 0; i < n; ++i)
    {
      block *b = blocks[i];
      if (b)
	o << *b << endl;
    }
}
} // namespace bpf
