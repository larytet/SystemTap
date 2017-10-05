// bpf translation pass
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include "bpf-internal.h"
#include "staptree.h"
#include "elaborate.h"
#include "session.h"
#include "translator-output.h"
#include "tapsets.h"
#include <sstream>
#include <unistd.h>
#include <fcntl.h>

extern "C" {
#include <libelf.h>
/* Unfortunately strtab manipulation functions were only officially added
   to elfutils libdw in 0.167.  Before that there were internal unsupported
   ebl variants.  While libebl.h isn't supported we'll try to use it anyway
   if the elfutils we build against is too old.  */
#include <elfutils/version.h>
#if _ELFUTILS_PREREQ (0, 167)
#include <elfutils/libdwelf.h>
typedef Dwelf_Strent Stap_Strent;
typedef Dwelf_Strtab Stap_Strtab;
#define stap_strtab_init      dwelf_strtab_init
#define stap_strtab_add(X,Y)  dwelf_strtab_add(X,Y)
#define stap_strtab_free      dwelf_strtab_free
#define stap_strtab_finalize  dwelf_strtab_finalize
#define stap_strent_offset    dwelf_strent_off
#else
#include <elfutils/libebl.h>
typedef Ebl_Strent Stap_Strent;
typedef Ebl_Strtab Stap_Strtab;
#define stap_strtab_init      ebl_strtabinit
#define stap_strtab_add(X,Y)  ebl_strtabadd(X,Y,0)
#define stap_strtab_free      ebl_strtabfree
#define stap_strtab_finalize  ebl_strtabfinalize
#define stap_strent_offset    ebl_strtaboffset
#endif
#include <linux/version.h>
#include <asm/ptrace.h>
}

#ifndef EM_BPF
#define EM_BPF  0xeb9f
#endif
#ifndef R_BPF_MAP_FD
#define R_BPF_MAP_FD 1
#endif

std::string module_name;

namespace bpf {

struct side_effects_visitor : public expression_visitor
{
  bool side_effects;

  side_effects_visitor() : side_effects(false) { }

  void visit_expression(expression *) { }
  void visit_pre_crement(pre_crement *) { side_effects = true; }
  void visit_post_crement(post_crement *) { side_effects = true; }
  void visit_assignment (assignment *) { side_effects = true; }
  void visit_functioncall (functioncall *) { side_effects = true; }
  void visit_print_format (print_format *) { side_effects = true; }
  void visit_stat_op (stat_op *) { side_effects = true; }
  void visit_hist_op (hist_op *) { side_effects = true; }
};

static bool
has_side_effects (expression *e)
{
  side_effects_visitor t;
  e->visit (&t);
  return t.side_effects;
}

struct bpf_unparser : public throwing_visitor
{
  // The visitor class isn't as helpful as it might be.  As a consequence,
  // the RESULT member is set after visiting any expression type.  Use the
  // emit_expr helper to return the result properly.
  value *result;

  // The program into which we are emitting code.
  program &this_prog;
  globals &glob;
  value *this_in_arg0;

  // The "current" block into which we are currently emitting code.
  insn_append_inserter this_ins;
  void set_block(block *b)
    { this_ins.b = b; this_ins.i = b->last; }
  void clear_block()
    { this_ins.b = NULL; this_ins.i = NULL; }
  bool in_block() const
    { return this_ins.b != NULL; }

  // Destinations for "break", "continue", and "return" respectively.
  std::vector<block *> loop_break;
  std::vector<block *> loop_cont;
  std::vector<block *> func_return;
  std::vector<value *> func_return_val;
  std::vector<functiondecl *> func_calls;

  // Local variable declarations.
  typedef std::unordered_map<vardecl *, value *> locals_map;
  locals_map *this_locals;

  // Return 0.
  block *ret0_block;
  block *exit_block;
  block *get_ret0_block();
  block *get_exit_block();

  virtual void visit_block (::block *s);
  virtual void visit_embeddedcode (embeddedcode *s);
  virtual void visit_null_statement (null_statement *s);
  virtual void visit_expr_statement (expr_statement *s);
  virtual void visit_if_statement (if_statement* s);
  virtual void visit_for_loop (for_loop* s);
  virtual void visit_return_statement (return_statement* s);
  virtual void visit_break_statement (break_statement* s);
  virtual void visit_continue_statement (continue_statement* s);
  virtual void visit_delete_statement (delete_statement* s);
  virtual void visit_literal_number (literal_number* e);
  virtual void visit_binary_expression (binary_expression* e);
  virtual void visit_unary_expression (unary_expression* e);
  virtual void visit_pre_crement (pre_crement* e);
  virtual void visit_post_crement (post_crement* e);
  virtual void visit_logical_or_expr (logical_or_expr* e);
  virtual void visit_logical_and_expr (logical_and_expr* e);
  virtual void visit_comparison (comparison* e);
  virtual void visit_ternary_expression (ternary_expression* e);
  virtual void visit_assignment (assignment* e);
  virtual void visit_symbol (symbol* e);
  virtual void visit_arrayindex (arrayindex *e);
  virtual void visit_functioncall (functioncall* e);
  virtual void visit_print_format (print_format* e);
  virtual void visit_target_register (target_register* e);
  virtual void visit_target_deref (target_deref* e);

  void emit_stmt(statement *s);
  void emit_mov(value *d, value *s);
  void emit_jmp(block *b);
  void emit_cond(expression *e, block *t, block *f);
  void emit_store(expression *dest, value *src);
  value *emit_expr(expression *e);
  value *emit_bool(expression *e);
  value *parse_reg(const std::string &str, embeddedcode *s);

  locals_map *new_locals(const std::vector<vardecl *> &);

  bpf_unparser (program &c, globals &g);
  virtual ~bpf_unparser ();
};

bpf_unparser::bpf_unparser(program &p, globals &g)
  : throwing_visitor ("unhandled statement type"),
    result(NULL), this_prog(p), glob(g), this_locals(NULL),
    ret0_block(NULL), exit_block(NULL)
{ }

bpf_unparser::~bpf_unparser()
{
  delete this_locals;
}

bpf_unparser::locals_map *
bpf_unparser::new_locals(const std::vector<vardecl *> &vars)
{
  locals_map *m = new locals_map;

  for (std::vector<vardecl *>::const_iterator i = vars.begin ();
       i != vars.end (); ++i)
    {
      const locals_map::value_type v (*i, this_prog.new_reg());
      auto ok = m->insert (v);
      assert (ok.second);
    }

  return m;
}

block *
bpf_unparser::get_exit_block()
{
  if (exit_block)
    return exit_block;

  block *b = this_prog.new_block();
  insn_append_inserter ins(b);

  this_prog.mk_exit(ins);

  exit_block = b;
  return b;
}

block *
bpf_unparser::get_ret0_block()
{
  if (ret0_block)
    return ret0_block;

  block *b = this_prog.new_block();
  insn_append_inserter ins(b);

  this_prog.mk_mov(ins, this_prog.lookup_reg(BPF_REG_0), this_prog.new_imm(0));
  b->fallthru = new edge(b, get_exit_block());

  ret0_block = b;
  return b;
}

void
bpf_unparser::emit_stmt(statement *s)
{
  if (s)
    s->visit (this);
}

value *
bpf_unparser::emit_expr(expression *e)
{
  e->visit (this);
  value *v = result;
  result = NULL;
  return v;
}

void
bpf_unparser::emit_mov(value *d, value *s)
{
  this_prog.mk_mov(this_ins, d, s);
}

void
bpf_unparser::emit_jmp(block *b)
{
  // Begin by hoping that we can simply place the destination as fallthru.
  // If this assumption doesn't hold, it'll be fixed by reorder_blocks.
  block *this_block = this_ins.get_block ();
  this_block->fallthru = new edge(this_block, b);
  clear_block ();
}

void
bpf_unparser::emit_cond(expression *e, block *t_dest, block *f_dest)
{
  condition cond;
  value *s0, *s1;

  // Look for and handle logical operators first.
  if (logical_or_expr *l = dynamic_cast<logical_or_expr *>(e))
    {
      block *cont_block = this_prog.new_block ();
      emit_cond (l->left, t_dest, cont_block);
      set_block (cont_block);
      emit_cond (l->right, t_dest, f_dest);
      return;
    }
  if (logical_and_expr *l = dynamic_cast<logical_and_expr *>(e))
    {
      block *cont_block = this_prog.new_block ();
      emit_cond (l->left, cont_block, f_dest);
      set_block (cont_block);
      emit_cond (l->right, t_dest, f_dest);
      return;
    }
  if (unary_expression *u = dynamic_cast<unary_expression *>(e))
    if (u->op == "!")
      {
	emit_cond (u->operand, f_dest, t_dest);
	return;
      }

  // What is left must generate a comparison + conditional branch.
  if (comparison *c = dynamic_cast<comparison *>(e))
    {
      s0 = emit_expr (c->left);
      s1 = emit_expr (c->right);
      if (c->op == "==")
	cond = EQ;
      else if (c->op == "!=")
	cond = NE;
      else if (c->op == "<")
	cond = LT;
      else if (c->op == "<=")
	cond = LE;
      else if (c->op == ">")
	cond = GT;
      else if (c->op == ">=")
	cond = GE;
      else
	throw SEMANTIC_ERROR (_("unhandled comparison operator"), e->tok);
    }
  else
    {
      binary_expression *bin = dynamic_cast<binary_expression *>(e);
      if (bin && bin->op == "&")
	{
	  s0 = emit_expr (bin->left);
	  s1 = emit_expr (bin->right);
	  cond = TEST;
	}
      else
	{
	  // Fall back to E != 0.
	  s0 = emit_expr (e);
	  s1 = this_prog.new_imm(0);
	  cond = NE;
	}
    }

  this_prog.mk_jcond (this_ins, cond, s0, s1, t_dest, f_dest);
  clear_block ();
}

value *
bpf_unparser::emit_bool (expression *e)
{
  block *else_block = this_prog.new_block ();
  block *join_block = this_prog.new_block ();
  value *r = this_prog.new_reg();

  emit_mov (r, this_prog.new_imm(1));
  emit_cond (e, join_block, else_block);

  set_block (else_block);
  emit_mov (r, this_prog.new_imm(0));
  emit_jmp (join_block);

  set_block(join_block);
  return r;
}

void
bpf_unparser::emit_store(expression *e, value *val)
{
  if (symbol *s = dynamic_cast<symbol *>(e))
    {
      vardecl *var = s->referent;
      assert (var->arity == 0);

      auto g = glob.globals.find (var);
      if (g != glob.globals.end())
	{
	  value *frame = this_prog.lookup_reg(BPF_REG_10);
	  int key_ofs, val_ofs;

	  switch (var->type)
	    {
	    case pe_long:
	      val_ofs = -8;
	      this_prog.mk_st(this_ins, BPF_DW, frame, val_ofs, val);
	      this_prog.mk_binary(this_ins, BPF_ADD,
				  this_prog.lookup_reg(BPF_REG_3),
				  frame, this_prog.new_imm(val_ofs));
	      break;
	    // ??? pe_string
	    // ??? pe_stats
	    default:
	      goto err;
	    }

	  key_ofs = val_ofs - 4;
	  this_prog.mk_st(this_ins, BPF_W, frame, key_ofs,
			  this_prog.new_imm(g->second.second));
	  this_prog.use_tmp_space(-key_ofs);

	  this_prog.load_map(this_ins, this_prog.lookup_reg(BPF_REG_1),
			     g->second.first);
	  this_prog.mk_binary(this_ins, BPF_ADD,
			      this_prog.lookup_reg(BPF_REG_2),
			      frame, this_prog.new_imm(key_ofs));
	  emit_mov(this_prog.lookup_reg(BPF_REG_4), this_prog.new_imm(0));
	  this_prog.mk_call(this_ins, BPF_FUNC_map_update_elem, 4);
	  return;
	}

      auto i = this_locals->find (var);
      if (i != this_locals->end ())
	{
	  emit_mov (i->second, val);
	  return;
	}
    }
  else if (arrayindex *a = dynamic_cast<arrayindex *>(e))
    {
      if (symbol *a_sym = dynamic_cast<symbol *>(a->base))
	{
	  vardecl *v = a_sym->referent;
	  int key_ofs, val_ofs;

	  if (v->arity != 1)
	    throw SEMANTIC_ERROR(_("unhandled multi-dimensional array"), v->tok);

	  auto g = glob.globals.find(v);
	  if (g == glob.globals.end())
	    throw SEMANTIC_ERROR(_("unknown array variable"), v->tok);

	  value *idx = emit_expr(a->indexes[0]);
	  value *frame = this_prog.lookup_reg(BPF_REG_10);
	  switch (v->index_types[0])
	    {
	    case pe_long:
	      key_ofs = -8;
	      this_prog.mk_st(this_ins, BPF_DW, frame, key_ofs, idx);
	      this_prog.mk_binary(this_ins, BPF_ADD,
				  this_prog.lookup_reg(BPF_REG_2),
				  frame, this_prog.new_imm(key_ofs));
	      break;
	    // ??? pe_string
	    default:
	      throw SEMANTIC_ERROR(_("unhandled index type"), e->tok);
	    }
	  switch (v->type)
	    {
	    case pe_long:
	      val_ofs = key_ofs - 8;
	      this_prog.mk_st(this_ins, BPF_DW, frame, val_ofs, val);
	      this_prog.mk_binary(this_ins, BPF_ADD,
				  this_prog.lookup_reg(BPF_REG_3),
				  frame, this_prog.new_imm(val_ofs));
	      break;
	    // ??? pe_string
	    default:
	      throw SEMANTIC_ERROR(_("unhandled array type"), v->tok);
	    }

	  this_prog.load_map(this_ins, this_prog.lookup_reg(BPF_REG_1),
			     g->second.first);
	  this_prog.mk_call(this_ins, BPF_FUNC_map_update_elem, 4);
	  return;
	}
    }
 err:
  throw SEMANTIC_ERROR (_("unknown lvalue"), e->tok);
}

void
bpf_unparser::visit_block (::block *s)
{
  unsigned n = s->statements.size();
  for (unsigned i = 0; i < n; ++i)
    emit_stmt (s->statements[i]);
}

value *
bpf_unparser::parse_reg(const std::string &str, embeddedcode *s)
{
  if (str == "$$")
    {
      if (func_return.empty ())
	throw SEMANTIC_ERROR (_("no return value outside function"), s->tok);
      return func_return_val.back();
    }
  else if (str[0] == '$')
    {
      std::string var = str.substr(1);
      for (auto i = this_locals->begin(); i != this_locals->end(); ++i)
	{
	  vardecl *v = i->first;
	  if (var == v->unmangled_name)
	    return i->second;
	}
      throw SEMANTIC_ERROR (_("unknown variable"), s->tok);
    }
  else
    {
      unsigned long num = stoul(str, 0, 0);
      if (num > 10)
	throw SEMANTIC_ERROR (_("invalid bpf register"), s->tok);
      return this_prog.lookup_reg(num);
    }
}

void
bpf_unparser::visit_embeddedcode (embeddedcode *s)
{
  std::string strip;
  {
    const interned_string &code = s->code;
    unsigned n = code.size();
    bool in_comment = false;

    for (unsigned i = 0; i < n; ++i)
      {
	char c = code[i];
	if (isspace(c))
	  continue;
	if (in_comment)
	  {
	    if (c == '*' && code[i + 1] == '/')
	      ++i, in_comment = false;
	  }
	else if (c == '/' && code[i + 1] == '*')
	  ++i, in_comment = true;
	else
	  strip += c;
      }
  }

  std::istringstream ii (strip);
  ii >> std::setbase(0);

  while (true)
    {
      unsigned code;
      char s1, s2, s3, s4;
      char dest_b[256], src1_b[256];
      int64_t off, imm;

      ii >> code >> s1;
      ii.get(dest_b, sizeof(dest_b), ',') >> s2;
      ii.get(src1_b, sizeof(src1_b), ',') >> s3;
      ii >> off >> s4 >> imm;

      if (ii.fail() || s1 != ',' || s2 != ',' || s3 != ',' || s4 != ',')
	throw SEMANTIC_ERROR (_("invalid bpf embeddedcode syntax"), s->tok);

      if (code > 0xff && code != BPF_LD_MAP)
	throw SEMANTIC_ERROR (_("invalid bpf code"), s->tok);

      bool r_dest = false, r_src0 = false, r_src1 = false, i_src1 = false;
      switch (BPF_CLASS (code))
	{
	case BPF_LDX:
	  r_dest = r_src1 = true;
	  break;
	case BPF_STX:
	  r_src0 = r_src1 = true;
	  break;
	case BPF_ST:
	  r_src0 = i_src1 = true;
	  break;

	case BPF_ALU:
	case BPF_ALU64:
	  r_dest = true;
	  if (code & BPF_X)
	    r_src1 = true;
	  else
	    i_src1 = true;
	  switch (BPF_OP (code))
	    {
	    case BPF_NEG:
	    case BPF_MOV:
	      break;
	    case BPF_END:
	      /* X/K bit repurposed as LE/BE.  */
	      i_src1 = false, r_src1 = true;
	      break;
	    default:
	      r_src0 = true;
	    }
	  break;

	case BPF_JMP:
	  switch (BPF_OP (code))
	    {
	    case BPF_EXIT:
	      break;
	    case BPF_CALL:
	      i_src1 = true;
	      break;
	    default:
	      throw SEMANTIC_ERROR (_("invalid branch in bpf code"), s->tok);
	    }
	  break;

	default:
          if (code == BPF_LD_MAP)
            r_dest = true, i_src1 = true;
          else
	    throw SEMANTIC_ERROR (_("unknown opcode in bpf code"), s->tok);
	}

      std::string dest(dest_b);
      value *v_dest = NULL;
      if (r_dest || r_src0)
	v_dest = parse_reg(dest, s);
      else if (dest != "0")
	throw SEMANTIC_ERROR (_("invalid register field in bpf code"), s->tok);

      std::string src1(src1_b);
      value *v_src1 = NULL;
      if (r_src1)
	v_src1 = parse_reg(src1, s);
      else
	{
	  if (src1 != "0")
	    throw SEMANTIC_ERROR (_("invalid register field in bpf code"), s->tok);
	  if (i_src1)
	    v_src1 = this_prog.new_imm(imm);
	  else if (imm != 0)
	    throw SEMANTIC_ERROR (_("invalid immediate field in bpf code"), s->tok);
	}

      if (off != (int16_t)off)
	throw SEMANTIC_ERROR (_("offset field out of range in bpf code"), s->tok);

      insn *i = this_ins.new_insn();
      i->code = code;
      i->dest = (r_dest ? v_dest : NULL);
      i->src0 = (r_src0 ? v_dest : NULL);
      i->src1 = v_src1;
      i->off = off;

      ii >> s1;
      if (ii.eof())
	break;
      if (s1 != ';')
	throw SEMANTIC_ERROR (_("invalid bpf embeddedcode syntax"), s->tok);
    }
}

void
bpf_unparser::visit_null_statement (null_statement *)
{ }

void
bpf_unparser::visit_expr_statement (expr_statement *s)
{
  (void) emit_expr (s->value);
}

void
bpf_unparser::visit_if_statement (if_statement* s)
{
  block *then_block = this_prog.new_block ();
  block *join_block = this_prog.new_block ();

  if (s->elseblock)
    {
      block *else_block = this_prog.new_block ();
      emit_cond (s->condition, then_block, else_block);

      set_block (then_block);
      emit_stmt (s->thenblock);
      if (in_block ())
	emit_jmp (join_block);

      set_block (else_block);
      emit_stmt (s->elseblock);
      if (in_block ())
	emit_jmp (join_block);
    }
  else
    {
      emit_cond (s->condition, then_block, join_block);

      set_block (then_block);
      emit_stmt (s->thenblock);
      if (in_block ())
	emit_jmp (join_block);
    }
  set_block (join_block);
}

void
bpf_unparser::visit_for_loop (for_loop* s)
{
  block *body_block = this_prog.new_block ();
  block *iter_block = this_prog.new_block ();
  block *test_block = this_prog.new_block ();
  block *join_block = this_prog.new_block ();

  emit_stmt (s->init);
  if (!in_block ())
    return;
  emit_jmp (test_block);

  loop_break.push_back (join_block);
  loop_cont.push_back (iter_block);

  set_block (body_block);
  emit_stmt (s->block);
  if (in_block ())
    emit_jmp (iter_block);

  loop_cont.pop_back ();
  loop_break.pop_back ();

  set_block (iter_block);
  emit_stmt (s->incr);
  if (in_block ())
    emit_jmp (test_block);

  set_block (test_block);
  emit_cond (s->cond, body_block, join_block);

  set_block (join_block);
}

void
bpf_unparser::visit_break_statement (break_statement* s)
{
  if (loop_break.empty ())
    throw SEMANTIC_ERROR (_("cannot 'break' outside loop"), s->tok);
  emit_jmp (loop_break.back ());
}

void
bpf_unparser:: visit_continue_statement (continue_statement* s)
{
  if (loop_cont.empty ())
    throw SEMANTIC_ERROR (_("cannot 'continue' outside loop"), s->tok);
  emit_jmp (loop_cont.back ());
}

void
bpf_unparser::visit_return_statement (return_statement* s)
{
  if (func_return.empty ())
    throw SEMANTIC_ERROR (_("cannot 'return' outside function"), s->tok);
  assert (!func_return_val.empty ());
  emit_mov (func_return_val.back (), emit_expr (s->value));
  emit_jmp (func_return.back ());
}

void
bpf_unparser::visit_delete_statement (delete_statement *s)
{
  expression *e = s->value;
  if (symbol *s = dynamic_cast<symbol *>(e))
    {
      vardecl *var = s->referent;
      if (var->arity != 0)
	throw SEMANTIC_ERROR (_("unimplemented delete of array"), s->tok);

      auto g = glob.globals.find (var);
      if (g != glob.globals.end())
	{
	  value *frame = this_prog.lookup_reg(BPF_REG_10);
	  int key_ofs, val_ofs;

	  switch (var->type)
	    {
	    case pe_long:
	      val_ofs = -8;
	      this_prog.mk_st(this_ins, BPF_DW, frame, val_ofs,
			      this_prog.new_imm(0));
	      this_prog.mk_binary(this_ins, BPF_ADD,
				  this_prog.lookup_reg(BPF_REG_3),
				  frame, this_prog.new_imm(val_ofs));
	      break;
	    // ??? pe_string
	    default:
	      goto err;
	    }

	  key_ofs = val_ofs - 4;
	  this_prog.mk_st(this_ins, BPF_W, frame, key_ofs,
			  this_prog.new_imm(g->second.second));
	  this_prog.use_tmp_space(-key_ofs);

	  this_prog.load_map(this_ins, this_prog.lookup_reg(BPF_REG_1),
			     g->second.first);
	  this_prog.mk_binary(this_ins, BPF_ADD,
			      this_prog.lookup_reg(BPF_REG_2),
			      frame, this_prog.new_imm(key_ofs));
	  emit_mov(this_prog.lookup_reg(BPF_REG_4), this_prog.new_imm(0));
	  this_prog.mk_call(this_ins, BPF_FUNC_map_update_elem, 4);
	  return;
	}

      auto i = this_locals->find (var);
      if (i != this_locals->end ())
	{
	  emit_mov (i->second, this_prog.new_imm(0));
	  return;
	}
    }
  else if (arrayindex *a = dynamic_cast<arrayindex *>(e))
    {
      if (symbol *a_sym = dynamic_cast<symbol *>(a->base))
	{
	  vardecl *v = a_sym->referent;
	  int key_ofs;

	  if (v->arity != 1)
	    throw SEMANTIC_ERROR(_("unhandled multi-dimensional array"), v->tok);

	  auto g = glob.globals.find(v);
	  if (g == glob.globals.end())
	    throw SEMANTIC_ERROR(_("unknown array variable"), v->tok);

	  value *idx = emit_expr(a->indexes[0]);
	  value *frame = this_prog.lookup_reg(BPF_REG_10);
	  switch (v->index_types[0])
	    {
	    case pe_long:
	      key_ofs = -8;
	      this_prog.mk_st(this_ins, BPF_DW, frame, key_ofs, idx);
	      this_prog.mk_binary(this_ins, BPF_ADD,
				  this_prog.lookup_reg(BPF_REG_2),
				  frame, this_prog.new_imm(key_ofs));
	      break;
	    // ??? pe_string
	    default:
	      throw SEMANTIC_ERROR(_("unhandled index type"), e->tok);
	    }
	  this_prog.load_map(this_ins, this_prog.lookup_reg(BPF_REG_1),
			     g->second.first);
	  this_prog.mk_call(this_ins, BPF_FUNC_map_delete_elem, 2);
	  return;
	}
    }
 err:
  throw SEMANTIC_ERROR (_("unknown lvalue"), e->tok);
}

void
bpf_unparser::visit_literal_number (literal_number* e)
{
  result = this_prog.new_imm(e->value);
}

void
bpf_unparser::visit_binary_expression (binary_expression* e)
{
  int code;
  if (e->op == "+")
    code = BPF_ADD;
  else if (e->op == "-")
    code = BPF_SUB;
  else if (e->op == "*")
    code = BPF_MUL;
  else if (e->op == "&")
    code = BPF_AND;
  else if (e->op == "|")
    code = BPF_OR;
  else if (e->op == "^")
    code = BPF_XOR;
  else if (e->op == "<<")
    code = BPF_LSH;
  else if (e->op == ">>")
    code = BPF_ARSH;
  else if (e->op == ">>>")
    code = BPF_RSH;
  else if (e->op == "/")
    code = BPF_DIV;
  else if (e->op == "%")
    code = BPF_MOD;
  else
    throw SEMANTIC_ERROR (_("unhandled binary operator"), e->tok);

  value *s0 = emit_expr (e->left);
  value *s1 = emit_expr (e->right);
  value *d = this_prog.new_reg ();
  this_prog.mk_binary (this_ins, code, d, s0, s1);
  result = d;
}

void
bpf_unparser::visit_unary_expression (unary_expression* e)
{
  if (e->op == "-")
    {
      // Note that negative literals appear in the script langauge as
      // unary negations over positive literals.
      if (literal_number *lit = dynamic_cast<literal_number *>(e))
	result = this_prog.new_imm(-(uint64_t)lit->value);
      else
	{
	  value *s = emit_expr (e->operand);
	  value *d = this_prog.new_reg();
	  this_prog.mk_unary (this_ins, BPF_NEG, d, s);
	  result = d;
	}
    }
  else if (e->op == "~")
    {
      value *s1 = this_prog.new_imm(-1);
      value *s0 = emit_expr (e->operand);
      value *d = this_prog.new_reg ();
      this_prog.mk_binary (this_ins, BPF_XOR, d, s0, s1);
      result = d;
    }
  else if (e->op == "!")
    result = emit_bool (e);
  else if (e->op == "+")
    result = emit_expr (e->operand);
  else
    throw SEMANTIC_ERROR (_("unhandled unary operator"), e->tok);
}

void
bpf_unparser::visit_pre_crement (pre_crement* e)
{
  int dir;
  if (e->op == "++")
    dir = 1;
  else if (e->op == "--")
    dir = -1;
  else
    throw SEMANTIC_ERROR (_("unhandled crement operator"), e->tok);

  value *c = this_prog.new_imm(dir);
  value *v = emit_expr (e->operand);
  this_prog.mk_binary (this_ins, BPF_ADD, v, v, c);
  emit_store (e->operand, v);
  result = v;
}

void
bpf_unparser::visit_post_crement (post_crement* e)
{
  int dir;
  if (e->op == "++")
    dir = 1;
  else if (e->op == "--")
    dir = -1;
  else
    throw SEMANTIC_ERROR (_("unhandled crement operator"), e->tok);

  value *c = this_prog.new_imm(dir);
  value *r = this_prog.new_reg ();
  value *v = emit_expr (e->operand);

  emit_mov (r, v);
  this_prog.mk_binary (this_ins, BPF_ADD, v, v, c);
  emit_store (e->operand, v);
  result = r;
}

void
bpf_unparser::visit_logical_or_expr (logical_or_expr* e)
{
  result = emit_bool (e);
}

void
bpf_unparser::visit_logical_and_expr (logical_and_expr* e)
{
  result = emit_bool (e);
}

void
bpf_unparser::visit_comparison (comparison* e)
{
  result = emit_bool (e);
}

void
bpf_unparser::visit_ternary_expression (ternary_expression* e)
{
  block *join_block = this_prog.new_block ();
  value *r = this_prog.new_reg ();

  if (!has_side_effects (e->truevalue))
    {
      block *else_block = this_prog.new_block ();

      emit_mov (r, emit_expr (e->truevalue));
      emit_cond (e->cond, join_block, else_block);

      set_block (else_block);
      emit_mov (r, emit_expr (e->falsevalue));
      emit_jmp (join_block);
    }
  else if (!has_side_effects (e->falsevalue))
    {
      block *then_block = this_prog.new_block ();

      emit_mov (r, emit_expr (e->falsevalue));
      emit_cond (e->cond, join_block, then_block);

      set_block (then_block);
      emit_mov (r, emit_expr (e->truevalue));
      emit_jmp (join_block);
    }
  else
    {
      block *then_block = this_prog.new_block ();
      block *else_block = this_prog.new_block ();
      emit_cond (e->cond, then_block, else_block);

      set_block (then_block);
      emit_mov (r, emit_expr (e->truevalue));
      emit_jmp (join_block);

      set_block (else_block);
      emit_mov (r, emit_expr (e->falsevalue));
      emit_jmp (join_block);
    }

  set_block (join_block);
  result = r;
}

void
bpf_unparser::visit_assignment (assignment* e)
{
  value *r = emit_expr (e->right);

  if (e->op != "=")
    {
      int code;
      if (e->op == "+=")
	code = BPF_ADD;
      else if (e->op == "-=")
	code = BPF_SUB;
      else if (e->op == "*=")
	code = BPF_MUL;
      else if (e->op == "/=")
	code = BPF_DIV;
      else if (e->op == "%=")
	code = BPF_MOD;
      else if (e->op == "<<=")
	code = BPF_LSH;
      else if (e->op == ">>=")
	code = BPF_ARSH;
      else if (e->op == "&=")
	code = BPF_AND;
      else if (e->op == "^=")
	code = BPF_XOR;
      else if (e->op == "|=")
	code = BPF_OR;
      else
	throw SEMANTIC_ERROR (_("unhandled assignment operator"), e->tok);

      value *l = emit_expr (e->left);
      this_prog.mk_binary (this_ins, code, l, l, r);
      r = l;
    }

  emit_store (e->left, r);
  result = r;
}

void
bpf_unparser::visit_symbol (symbol *s)
{
  vardecl *v = s->referent;
  assert (v->arity == 0);

  auto g = glob.globals.find (v);
  if (g != glob.globals.end())
    {
      value *frame = this_prog.lookup_reg(BPF_REG_10);
      this_prog.mk_st(this_ins, BPF_W, frame, -4,
		      this_prog.new_imm(g->second.second));
      this_prog.use_tmp_space(4);

      this_prog.load_map(this_ins, this_prog.lookup_reg(BPF_REG_1),
			 g->second.first);
      this_prog.mk_binary(this_ins, BPF_ADD, this_prog.lookup_reg(BPF_REG_2),
			  frame, this_prog.new_imm(-4));
      this_prog.mk_call(this_ins, BPF_FUNC_map_lookup_elem, 2);

      value *r0 = this_prog.lookup_reg(BPF_REG_0);
      value *i0 = this_prog.new_imm(0);
      block *cont_block = this_prog.new_block();
      block *exit_block = get_exit_block();

      // Note that the kernel bpf verifier requires that we check that
      // the pointer is non-null.
      this_prog.mk_jcond(this_ins, EQ, r0, i0, exit_block, cont_block);

      set_block(cont_block);

      result = this_prog.new_reg();
      switch (v->type)
	{
	case pe_long:
	  this_prog.mk_ld(this_ins, BPF_DW, result, r0, 0);
	  break;
	// ??? pe_string
	default:
	  throw SEMANTIC_ERROR (_("unhandled global variable type"), s->tok);
	}
      return;
    }

  // ??? Maybe use result = this_locals.at (v);
  // to throw std::out_of_range on lookup failure.
  auto l = this_locals->find (v);
  if (l != this_locals->end())
    {
      result = (*l).second;
      return;
    }
  throw SEMANTIC_ERROR (_("unknown variable"), s->tok);
}

void
bpf_unparser::visit_arrayindex(arrayindex *e)
{
  if (symbol *sym = dynamic_cast<symbol *>(e->base))
    {
      vardecl *v = sym->referent;

      if (v->arity != 1)
	throw SEMANTIC_ERROR(_("unhandled multi-dimensional array"), v->tok);

      auto g = glob.globals.find(v);
      if (g == glob.globals.end())
	throw SEMANTIC_ERROR(_("unknown array variable"), v->tok);

      value *idx = emit_expr(e->indexes[0]);
      switch (v->index_types[0])
	{
	case pe_long:
	  {
	    value *frame = this_prog.lookup_reg(BPF_REG_10);
	    this_prog.mk_st(this_ins, BPF_DW, frame, -8, idx);
	    this_prog.use_tmp_space(8);
	    this_prog.mk_binary(this_ins, BPF_ADD,
				this_prog.lookup_reg(BPF_REG_2),
				frame, this_prog.new_imm(-8));
	  }
	  break;
	// ??? pe_string
	default:
	  throw SEMANTIC_ERROR(_("unhandled index type"), e->tok);
	}

      this_prog.load_map(this_ins, this_prog.lookup_reg(BPF_REG_1),
			 g->second.first);

      value *r0 = this_prog.lookup_reg(BPF_REG_0);
      value *i0 = this_prog.new_imm(0);
      block *cont_block = this_prog.new_block();
      block *exit_block = get_exit_block();
      this_prog.mk_call(this_ins, BPF_FUNC_map_lookup_elem, 2);
      this_prog.mk_jcond(this_ins, EQ, r0, i0, exit_block, cont_block);

      set_block(cont_block);
      result = this_prog.new_reg();
      if (v->type == pe_long)
	this_prog.mk_ld(this_ins, BPF_DW, result, r0, 0);
      else
	emit_mov(result, r0);
    }
  else
    throw SEMANTIC_ERROR(_("unhandled arrayindex expression"), e->tok);
}

void
bpf_unparser::visit_target_deref (target_deref* e)
{
  // ??? For some hosts, including x86_64, it works to read userspace
  // and kernelspace with the same function.  For others, like s390x,
  // this only works to read kernelspace.

  value *src = emit_expr (e->addr);
  value *frame = this_prog.lookup_reg (BPF_REG_10);

  this_prog.mk_mov (this_ins, this_prog.lookup_reg(BPF_REG_3), src);
  this_prog.mk_mov (this_ins, this_prog.lookup_reg(BPF_REG_2),
		    this_prog.new_imm (e->size));
  this_prog.mk_binary (this_ins, BPF_ADD, this_prog.lookup_reg(BPF_REG_1),
		       frame, this_prog.new_imm (-(int64_t)e->size));
  this_prog.use_tmp_space(e->size);

  this_prog.mk_call(this_ins, BPF_FUNC_probe_read, 3);

  value *d = this_prog.new_reg ();
  int opc;
  switch (e->size)
    {
    case 1: opc = BPF_B; break;
    case 2: opc = BPF_H; break;
    case 4: opc = BPF_W; break;
    case 8: opc = BPF_DW; break;
    default:
      throw SEMANTIC_ERROR(_("unhandled deref size"), e->tok);
    }
  this_prog.mk_ld (this_ins, opc, d, frame, -e->size);

  if (e->signed_p && e->size < 8)
    {
      value *sh = this_prog.new_imm ((8 - e->size) * 8);
      this_prog.mk_binary (this_ins, BPF_LSH, d, d, sh);
      this_prog.mk_binary (this_ins, BPF_ARSH, d, d, sh);
    }
  result = d;
}

void
bpf_unparser::visit_target_register (target_register* e)
{
  // ??? Should not hard-code register size.
  int size = sizeof(void *);
  // ??? Should not hard-code register offsets in pr_regs.
  int ofs = 0;
  switch (e->regno)
    {
#if defined(__i386__)
    case  0: ofs = offsetof(pt_regs, eax); break;
    case  1: ofs = offsetof(pt_regs, ecx); break;
    case  2: ofs = offsetof(pt_regs, edx); break;
    case  3: ofs = offsetof(pt_regs, ebx); break;
    case  4: ofs = offsetof(pt_regs, esp); break;
    case  5: ofs = offsetof(pt_regs, ebp); break;
    case  6: ofs = offsetof(pt_regs, esi); break;
    case  7: ofs = offsetof(pt_regs, edi); break;
    case  8: ofs = offsetof(pt_regs, eip); break;
#elif defined(__x86_64__)
    case  0: ofs = offsetof(pt_regs, rax); break;
    case  1: ofs = offsetof(pt_regs, rdx); break;
    case  2: ofs = offsetof(pt_regs, rcx); break;
    case  3: ofs = offsetof(pt_regs, rbx); break;
    case  4: ofs = offsetof(pt_regs, rsi); break;
    case  5: ofs = offsetof(pt_regs, rdi); break;
    case  6: ofs = offsetof(pt_regs, rbp); break;
    case  7: ofs = offsetof(pt_regs, rsp); break;
    case  8: ofs = offsetof(pt_regs, r8); break;
    case  9: ofs = offsetof(pt_regs, r9); break;
    case 10: ofs = offsetof(pt_regs, r10); break;
    case 11: ofs = offsetof(pt_regs, r11); break;
    case 12: ofs = offsetof(pt_regs, r12); break;
    case 13: ofs = offsetof(pt_regs, r13); break;
    case 14: ofs = offsetof(pt_regs, r14); break;
    case 15: ofs = offsetof(pt_regs, r15); break;
    case 16: ofs = offsetof(pt_regs, rip); break;
#elif defined(__arm__)
    case  0: ofs = offsetof(pt_regs, uregs[0]); break;
    case  1: ofs = offsetof(pt_regs, uregs[1]); break;
    case  2: ofs = offsetof(pt_regs, uregs[2]); break;
    case  3: ofs = offsetof(pt_regs, uregs[3]); break;
    case  4: ofs = offsetof(pt_regs, uregs[4]); break;
    case  5: ofs = offsetof(pt_regs, uregs[5]); break;
    case  6: ofs = offsetof(pt_regs, uregs[6]); break;
    case  7: ofs = offsetof(pt_regs, uregs[7]); break;
    case  8: ofs = offsetof(pt_regs, uregs[8]); break;
    case  9: ofs = offsetof(pt_regs, uregs[9]); break;
    case  10: ofs = offsetof(pt_regs, uregs[10]); break;
    case  11: ofs = offsetof(pt_regs, uregs[11]); break;
    case  12: ofs = offsetof(pt_regs, uregs[12]); break;
    case  13: ofs = offsetof(pt_regs, uregs[13]); break;
    case  14: ofs = offsetof(pt_regs, uregs[14]); break;
    case  15: ofs = offsetof(pt_regs, uregs[15]); break;
#elif defined(__aarch64__)
    case  0: ofs = offsetof(user_pt_regs, regs[0]); break;
    case  1: ofs = offsetof(user_pt_regs, regs[1]); break;
    case  2: ofs = offsetof(user_pt_regs, regs[2]); break;
    case  3: ofs = offsetof(user_pt_regs, regs[3]); break;
    case  4: ofs = offsetof(user_pt_regs, regs[4]); break;
    case  5: ofs = offsetof(user_pt_regs, regs[5]); break;
    case  6: ofs = offsetof(user_pt_regs, regs[6]); break;
    case  7: ofs = offsetof(user_pt_regs, regs[7]); break;
    case  8: ofs = offsetof(user_pt_regs, regs[8]); break;
    case  9: ofs = offsetof(user_pt_regs, regs[9]); break;
    case  10: ofs = offsetof(user_pt_regs, regs[10]); break;
    case  11: ofs = offsetof(user_pt_regs, regs[11]); break;
    case  12: ofs = offsetof(user_pt_regs, regs[12]); break;
    case  13: ofs = offsetof(user_pt_regs, regs[13]); break;
    case  14: ofs = offsetof(user_pt_regs, regs[14]); break;
    case  15: ofs = offsetof(user_pt_regs, regs[15]); break;
    case  16: ofs = offsetof(user_pt_regs, regs[16]); break;
    case  17: ofs = offsetof(user_pt_regs, regs[17]); break;
    case  18: ofs = offsetof(user_pt_regs, regs[18]); break;
    case  19: ofs = offsetof(user_pt_regs, regs[19]); break;
    case  20: ofs = offsetof(user_pt_regs, regs[20]); break;
    case  21: ofs = offsetof(user_pt_regs, regs[21]); break;
    case  22: ofs = offsetof(user_pt_regs, regs[22]); break;
    case  23: ofs = offsetof(user_pt_regs, regs[23]); break;
    case  24: ofs = offsetof(user_pt_regs, regs[24]); break;
    case  25: ofs = offsetof(user_pt_regs, regs[25]); break;
    case  26: ofs = offsetof(user_pt_regs, regs[26]); break;
    case  27: ofs = offsetof(user_pt_regs, regs[27]); break;
    case  28: ofs = offsetof(user_pt_regs, regs[28]); break;
    case  29: ofs = offsetof(user_pt_regs, regs[29]); break;
    case  30: ofs = offsetof(user_pt_regs, regs[30]); break;
    case  31: ofs = offsetof(user_pt_regs, sp); break;
#elif defined(__powerpc__)
    case   0: ofs = offsetof(pt_regs, gpr[0]); break;
    case   1: ofs = offsetof(pt_regs, gpr[1]); break;
    case   2: ofs = offsetof(pt_regs, gpr[2]); break;
    case   3: ofs = offsetof(pt_regs, gpr[3]); break;
    case   4: ofs = offsetof(pt_regs, gpr[4]); break;
    case   5: ofs = offsetof(pt_regs, gpr[5]); break;
    case   6: ofs = offsetof(pt_regs, gpr[6]); break;
    case   7: ofs = offsetof(pt_regs, gpr[7]); break;
    case   8: ofs = offsetof(pt_regs, gpr[8]); break;
    case   9: ofs = offsetof(pt_regs, gpr[9]); break;
    case  10: ofs = offsetof(pt_regs, gpr[10]); break;
    case  11: ofs = offsetof(pt_regs, gpr[11]); break;
    case  12: ofs = offsetof(pt_regs, gpr[12]); break;
    case  13: ofs = offsetof(pt_regs, gpr[13]); break;
    case  14: ofs = offsetof(pt_regs, gpr[14]); break;
    case  15: ofs = offsetof(pt_regs, gpr[15]); break;
    case  16: ofs = offsetof(pt_regs, gpr[16]); break;
    case  17: ofs = offsetof(pt_regs, gpr[17]); break;
    case  18: ofs = offsetof(pt_regs, gpr[18]); break;
    case  19: ofs = offsetof(pt_regs, gpr[19]); break;
    case  20: ofs = offsetof(pt_regs, gpr[20]); break;
    case  21: ofs = offsetof(pt_regs, gpr[21]); break;
    case  22: ofs = offsetof(pt_regs, gpr[22]); break;
    case  23: ofs = offsetof(pt_regs, gpr[23]); break;
    case  24: ofs = offsetof(pt_regs, gpr[24]); break;
    case  25: ofs = offsetof(pt_regs, gpr[25]); break;
    case  26: ofs = offsetof(pt_regs, gpr[26]); break;
    case  27: ofs = offsetof(pt_regs, gpr[27]); break;
    case  28: ofs = offsetof(pt_regs, gpr[28]); break;
    case  29: ofs = offsetof(pt_regs, gpr[29]); break;
    case  30: ofs = offsetof(pt_regs, gpr[30]); break;
    case  31: ofs = offsetof(pt_regs, gpr[31]); break;
    case  64: ofs = offsetof(pt_regs, ccr); break;
    case  66: ofs = offsetof(pt_regs, msr); break;
    case 101: ofs = offsetof(pt_regs, xer); break;
    case 108: ofs = offsetof(pt_regs, link); break;
    case 109: ofs = offsetof(pt_regs, ctr); break;
    case 118: ofs = offsetof(pt_regs, dsisr); break;
    case 119: ofs = offsetof(pt_regs, dar); break;
# if !defined(__powerpc64__)
    case 100: ofs = offsetof(pt_regs, mq); break;
# endif
    // ??? NIP is not assigned to a dwarf register number at all.
#elif defined(__s390__)
    case  0: ofs = offsetof(user_regs_struct, gprs[0]); break;
    case  1: ofs = offsetof(user_regs_struct, gprs[1]); break;
    case  2: ofs = offsetof(user_regs_struct, gprs[2]); break;
    case  3: ofs = offsetof(user_regs_struct, gprs[3]); break;
    case  4: ofs = offsetof(user_regs_struct, gprs[4]); break;
    case  5: ofs = offsetof(user_regs_struct, gprs[5]); break;
    case  6: ofs = offsetof(user_regs_struct, gprs[6]); break;
    case  7: ofs = offsetof(user_regs_struct, gprs[7]); break;
    case  8: ofs = offsetof(user_regs_struct, gprs[8]); break;
    case  9: ofs = offsetof(user_regs_struct, gprs[9]); break;
    case 10: ofs = offsetof(user_regs_struct, gprs[10]); break;
    case 11: ofs = offsetof(user_regs_struct, gprs[11]); break;
    case 12: ofs = offsetof(user_regs_struct, gprs[12]); break;
    case 13: ofs = offsetof(user_regs_struct, gprs[13]); break;
    case 14: ofs = offsetof(user_regs_struct, gprs[14]); break;
    case 15: ofs = offsetof(user_regs_struct, gprs[15]); break;
    // Note that the FPRs are not numbered sequentially
    case 16: ofs = offsetof(user_regs_struct, fp_regs.fprs[0]); break;
    case 17: ofs = offsetof(user_regs_struct, fp_regs.fprs[2]); break;
    case 18: ofs = offsetof(user_regs_struct, fp_regs.fprs[4]); break;
    case 19: ofs = offsetof(user_regs_struct, fp_regs.fprs[6]); break;
    case 20: ofs = offsetof(user_regs_struct, fp_regs.fprs[1]); break;
    case 21: ofs = offsetof(user_regs_struct, fp_regs.fprs[3]); break;
    case 22: ofs = offsetof(user_regs_struct, fp_regs.fprs[5]); break;
    case 23: ofs = offsetof(user_regs_struct, fp_regs.fprs[7]); break;
    case 24: ofs = offsetof(user_regs_struct, fp_regs.fprs[8]); break;
    case 25: ofs = offsetof(user_regs_struct, fp_regs.fprs[10]); break;
    case 26: ofs = offsetof(user_regs_struct, fp_regs.fprs[12]); break;
    case 27: ofs = offsetof(user_regs_struct, fp_regs.fprs[14]); break;
    case 28: ofs = offsetof(user_regs_struct, fp_regs.fprs[9]); break;
    case 29: ofs = offsetof(user_regs_struct, fp_regs.fprs[11]); break;
    case 30: ofs = offsetof(user_regs_struct, fp_regs.fprs[13]); break;
    case 31: ofs = offsetof(user_regs_struct, fp_regs.fprs[15]); break;
    // ??? Omitting CTRs (not in user_regs_struct)
    // ??? Omitting ACRs (lazy, and unlikely to appear in unwind)
    case 64: ofs = offsetof(user_regs_struct, psw.mask); break;
    case 65: ofs = offsetof(user_regs_struct, psw.addr); break;
#endif
    default:
      throw SEMANTIC_ERROR(_("unhandled register number"), e->tok);
    }

  value *frame = this_prog.lookup_reg (BPF_REG_10);
  this_prog.mk_binary (this_ins, BPF_ADD, this_prog.lookup_reg(BPF_REG_3),
                       this_in_arg0, this_prog.new_imm (ofs));
  this_prog.mk_mov (this_ins, this_prog.lookup_reg(BPF_REG_2),
		    this_prog.new_imm (size));
  this_prog.mk_binary (this_ins, BPF_ADD, this_prog.lookup_reg(BPF_REG_1),
		       frame, this_prog.new_imm (-size));
  this_prog.use_tmp_space(size);

  this_prog.mk_call(this_ins, BPF_FUNC_probe_read, 3);

  value *d = this_prog.new_reg ();
  int opc;
  switch (size)
    {
    case 4: opc = BPF_W; break;
    case 8: opc = BPF_DW; break;
    default:
      throw SEMANTIC_ERROR(_("unhandled register size"), e->tok);
    }
  this_prog.mk_ld (this_ins, opc, d, frame, -size);
  result = d;
}

void
bpf_unparser::visit_functioncall (functioncall *e)
{
  // ??? For now, always inline the function call.
  // ??? Function overloading isn't handled.
  if (e->referents.size () != 1)
    throw SEMANTIC_ERROR (_("unhandled function overloading"), e->tok);
  functiondecl *f = e->referents[0];

  for (auto i = func_calls.begin(); i != func_calls.end(); ++i)
    if (f == *i)
      throw SEMANTIC_ERROR (_("unhandled function recursion"), e->tok);

  assert (e->args.size () == f->formal_args.size ());

  // Create a new map for the function's local variables.
  locals_map *locals = new_locals(f->locals);

  // Evaluate the function arguments and install in the map.
  for (unsigned n = e->args.size (), i = 0; i < n; ++i)
    {
      value *r = this_prog.new_reg ();
      emit_mov (r, emit_expr (e->args[i]));
      const locals_map::value_type v (f->formal_args[i], r);
      auto ok = locals->insert (v);
      assert (ok.second);
    }

  locals_map *old_locals = this_locals;
  this_locals = locals;

  block *join_block = this_prog.new_block ();
  value *retval = this_prog.new_reg ();

  func_calls.push_back (f);
  func_return.push_back (join_block);
  func_return_val.push_back (retval);
  emit_stmt (f->body);
  func_return_val.pop_back ();
  func_return.pop_back ();
  func_calls.pop_back ();

  if (in_block ())
    emit_jmp (join_block);
  set_block (join_block);

  this_locals = old_locals;
  delete locals;

  result = retval;
}

static void
print_format_add_tag(print_format *e)
{
  // surround the string with <MODNAME>...</MODNAME> to facilitate
  // stapbpf recovering it from debugfs.
  std::string start_tag = module_name;
  start_tag = "<" + start_tag.erase(4, 1) + ">";
  std::string end_tag = start_tag + "\n";
  end_tag.insert(1, "/");
  e->raw_components.insert(0, start_tag);
  e->raw_components.append(end_tag);

  if (e->components.empty())
    {
      print_format::format_component c;
      c.literal_string = start_tag + end_tag;
      e->components.insert(e->components.begin(), c);
    }
  else
    {
      if (e->components[0].type == print_format::conv_literal)
        {
          std::string s = start_tag
                            + e->components[0].literal_string.to_string();
          e->components[0].literal_string = s;
        }
      else
        {
          print_format::format_component c;
          c.literal_string = start_tag;
          e->components.insert(e->components.begin(), c);
        }

      if (e->components.back().type == print_format::conv_literal)
        {
          std::string s = end_tag
                            + e->components.back().literal_string.to_string();
          e->components.back().literal_string = s;
        }
      else
        {
          print_format::format_component c;
          c.literal_string = end_tag;
          e->components.insert(e->components.end(), c);
        }
    }
}

void
bpf_unparser::visit_print_format (print_format *e)
{
  if (e->hist)
    throw SEMANTIC_ERROR (_("unhandled histogram print"), e->tok);

  print_format_add_tag(e);

  // ??? Traditional stap allows max 32 args; trace_printk allows only 3.
  // ??? Could split the print into multiple calls, such that each is
  // under the limit.
  size_t nargs = e->args.size();
  size_t i;
  if (nargs > 3)
    throw SEMANTIC_ERROR(_NF("additional argument to print",
			     "too many arguments to print (%zu)",
			     e->args.size(), e->args.size()), e->tok);

  value *actual[3] = { NULL, NULL, NULL };
  for (i = 0; i < nargs; ++i)
    actual[i] = emit_expr(e->args[i]);

  std::string format;
  if (e->print_with_format)
    {
      // ??? If this is a long string with no actual arguments,
      // intern the string as a global and use "%s" as the format.
      // Translate string escape characters.
      interned_string fstr = e->raw_components;
      bool saw_esc = false;
      for (interned_string::const_iterator j = fstr.begin();
	   j != fstr.end(); ++j)
	{
	  if (saw_esc)
	    {
	      saw_esc = false;
	      switch (*j)
		{
		case 'f': format += '\f'; break;
		case 'n': format += '\n'; break;
		case 'r': format += '\r'; break;
		case 't': format += '\t'; break;
		case 'v': format += '\v'; break;
		default:  format += *j; break;
		}
	    }
	  else if (*j == '\\')
	    saw_esc = true;
	  else
	    format += *j;
	}
    }
  else
    {
      // Synthesize a print-format string if the user didn't
      // provide one; the synthetic string simply contains one
      // directive for each argument.
      std::string delim;
      if (e->print_with_delim)
	{
	  interned_string dstr = e->delimiter;
	  for (interned_string::const_iterator j = dstr.begin();
	       j != dstr.end(); ++j)
	    {
	      if (*j == '%')
		delim += '%';
	      delim += *j;
	    }
	}

      for (i = 0; i < nargs; ++i)
	{
	  if (i > 0 && e->print_with_delim)
	    format += delim;
	  switch (e->args[i]->type)
	    {
	    default:
	    case pe_unknown:
	      throw SEMANTIC_ERROR(_("cannot print unknown expression type"),
				   e->args[i]->tok);
	    case pe_stats:
	      throw SEMANTIC_ERROR(_("cannot print a raw stats object"),
				   e->args[i]->tok);
	    case pe_long:
	      format += "%lld";
	      break;
	    case pe_string:
	      format += "%s";
	      break;
	    }
	}
      if (e->print_with_newline)
	format += '\n';
    }

  // The bpf verifier requires that the format string be stored on the
  // bpf program stack.  Write it out in 4 byte units.
  // ??? Endianness of the target comes into play here.
  size_t format_bytes = format.size() + 1;
  if (format_bytes > 256)
    throw SEMANTIC_ERROR(_("Format string for print too long"), e->tok);

  size_t format_words = (format_bytes + 3) / 4;
  value *frame = this_prog.lookup_reg(BPF_REG_10);
  for (i = 0; i < format_words; ++i)
    {
      uint32_t word = 0;
      for (unsigned j = 0; j < 4; ++j)
	if (i * 4 + j < format_bytes - 1)
	  {
	    // ??? little-endian target
	    word |= (uint32_t)format[i * 4 + j] << (j * 8);
	  }
      this_prog.mk_st(this_ins, BPF_W, frame,
		      (int32_t)(-format_words + i) * 4,
		      this_prog.new_imm(word));
    }
  this_prog.use_tmp_space(format_words * 4);

  this_prog.mk_binary(this_ins, BPF_ADD, this_prog.lookup_reg(BPF_REG_1),
		      frame, this_prog.new_imm(-(int32_t)format_words * 4));
  emit_mov(this_prog.lookup_reg(BPF_REG_2), this_prog.new_imm(format_bytes));
  for (i = 0; i < nargs; ++i)
    emit_mov(this_prog.lookup_reg(BPF_REG_3 + i), actual[i]);

  this_prog.mk_call(this_ins, BPF_FUNC_trace_printk, nargs + 2);
}

// } // anon namespace

void
build_internal_globals(globals& glob)
{
  struct vardecl exit;
  exit.name = "__global___STAPBPF_exit";
  exit.unmangled_name = "__STAPBPF_exit";
  exit.type = pe_long;
  exit.arity = 0;
  glob.internal_exit = exit;

  glob.globals.insert(std::pair<vardecl *, globals::map_slot>
                      (&glob.internal_exit,
                       globals::map_slot(0, globals::EXIT)));
  glob.maps.push_back
    ({ BPF_MAP_TYPE_ARRAY, 4, 8, globals::NUM_INTERNALS, 0 });
}

static void
translate_globals (globals &glob, systemtap_session& s)
{
  int long_map = -1;
  build_internal_globals(glob);

  for (auto i = s.globals.begin(); i != s.globals.end(); ++i)
    {
      vardecl *v = *i;
      int this_map, this_idx;

      switch (v->arity)
	{
	case 0: // scalars
	  switch (v->type)
	    {
	    case pe_long:
	      if (long_map < 0)
		{
		  globals::bpf_map_def m = {
		    BPF_MAP_TYPE_ARRAY, 4, 8, 0, 0
		  };
		  long_map = glob.maps.size();
		  glob.maps.push_back(m);
		}
	      this_map = long_map;
	      this_idx = glob.maps[long_map].max_entries++;
	      break;

	    // ??? pe_string
	    // ??? pe_stats
	    default:
	      throw SEMANTIC_ERROR (_("unhandled scalar type"), v->tok);
	    }
	  break;

	case 1: // single dimension array
	  {
	    globals::bpf_map_def m = { BPF_MAP_TYPE_HASH, 0, 0, 0, 0 };

	    switch (v->index_types[0])
	      {
	      case pe_long:
		m.key_size = 8;
		break;
	      // ??? pe_string:
	      default:
		throw SEMANTIC_ERROR (_("unhandled index type"), v->tok);
	      }
	    switch (v->type)
	      {
	      case pe_long:
		m.value_size = 8;
		break;
	      // ??? pe_string
	      // ??? pe_stats
	      default:
		throw SEMANTIC_ERROR (_("unhandled array element type"), v->tok);
	      }

	    m.max_entries = v->maxsize;
	    this_map = glob.maps.size();
	    glob.maps.push_back(m);
	    this_idx = 0;
	  }
	  break;

	default:
	  // Multi-dimensional arrays not supported for now.
	  throw SEMANTIC_ERROR (_("unhandled multi-dimensional array"), v->tok);
	}

      assert(this_map != globals::internal_map_idx);
      auto ok = (glob.globals.insert
		 (std::pair<vardecl *, globals::map_slot>
		  (v, globals::map_slot(this_map, this_idx))));
      assert(ok.second);
    }
}

struct BPF_Section
{
  Elf_Scn *scn;
  Elf64_Shdr *shdr;
  std::string name;
  Stap_Strent *name_ent;
  Elf_Data *data;
  bool free_data; // NB: then data must have been malloc()'d!

  BPF_Section(const std::string &n);
  ~BPF_Section();
};

BPF_Section::BPF_Section(const std::string &n)
  : scn(0), name(n), name_ent(0), data(0), free_data(false)
{ }

BPF_Section::~BPF_Section()
{
  if (free_data)
    free(data->d_buf);
}

struct BPF_Symbol
{
  std::string name;
  Stap_Strent *name_ent;
  Elf64_Sym sym;

  BPF_Symbol(const std::string &n, BPF_Section *, long);
};

BPF_Symbol::BPF_Symbol(const std::string &n, BPF_Section *sec, long off)
  : name(n), name_ent(0)
{
  memset(&sym, 0, sizeof(sym));
  sym.st_shndx = elf_ndxscn(sec->scn);
  sym.st_value = off;
}

struct BPF_Output
{
  Elf *elf;
  Elf64_Ehdr *ehdr;
  Stap_Strtab *str_tab;

  std::vector<BPF_Section *> sections;
  std::vector<BPF_Symbol *> symbols;

  BPF_Output(int fd);
  ~BPF_Output();
  BPF_Section *new_scn(const std::string &n);
  BPF_Symbol *new_sym(const std::string &n, BPF_Section *, long);
  BPF_Symbol *append_sym(const std::string &n, BPF_Section *, long);
};

BPF_Output::BPF_Output(int fd)
  : elf(elf_begin(fd, ELF_C_WRITE_MMAP, NULL)),
    ehdr(elf64_newehdr(elf)),
    str_tab(stap_strtab_init(true))
{
  ehdr->e_type = ET_REL;
  ehdr->e_machine = EM_BPF;
}

BPF_Output::~BPF_Output()
{
  stap_strtab_free(str_tab);

  for (auto i = symbols.begin(); i != symbols.end(); ++i)
    delete *i;
  for (auto i = sections.begin(); i != sections.end(); ++i)
    delete *i;

  elf_end(elf);
}

BPF_Section *
BPF_Output::new_scn(const std::string &name)
{
  BPF_Section *n = new BPF_Section(name);
  Elf_Scn *scn = elf_newscn(elf);

  n->scn = scn;
  n->shdr = elf64_getshdr(scn);
  n->data = elf_newdata(scn);
  n->name_ent = stap_strtab_add(str_tab, n->name.c_str());

  sections.push_back(n);
  return n;
}

BPF_Symbol *
BPF_Output::new_sym(const std::string &name, BPF_Section *sec, long off)
{
  BPF_Symbol *s = new BPF_Symbol(name, sec, off);
  s->name_ent = stap_strtab_add(str_tab, s->name.c_str());
  return s;
}

BPF_Symbol *
BPF_Output::append_sym(const std::string &name, BPF_Section *sec, long off)
{
  BPF_Symbol *s = new_sym(name, sec, off);
  symbols.push_back(s);
  return s;
}

static void
output_kernel_version(BPF_Output &eo, const std::string &base_version)
{
  unsigned long maj = 0, min = 0, rel = 0;
  char *q;

  maj = strtoul(base_version.c_str(), &q, 10);
  if (*q == '.')
    {
      min = strtoul(q + 1, &q, 10);
      if (*q == '.')
	rel = strtoul(q + 1, NULL, 10);
    }

  BPF_Section *so = eo.new_scn("version");
  Elf_Data *data = so->data;
  data->d_buf = malloc(sizeof(uint32_t));
  assert (data->d_buf);
  * (uint32_t*) data->d_buf = KERNEL_VERSION(maj, min, rel);
  data->d_type = ELF_T_BYTE;
  data->d_size = 4;
  data->d_align = 4;
  so->free_data = true;
  so->shdr->sh_type = SHT_PROGBITS;
  so->shdr->sh_entsize = 4;
}

static void
output_license(BPF_Output &eo)
{
  BPF_Section *so = eo.new_scn("license");
  Elf_Data *data = so->data;
  data->d_buf = (void *)"GPL";
  data->d_type = ELF_T_BYTE;
  data->d_size = 4;
  so->shdr->sh_type = SHT_PROGBITS;
}

static void
output_maps(BPF_Output &eo, globals &glob)
{
  unsigned nmaps = glob.maps.size();
  if (nmaps == 0)
    return;

  assert(sizeof(unsigned) == sizeof(Elf64_Word));

  const size_t bpf_map_def_sz = sizeof(globals::bpf_map_def);
  BPF_Section *so = eo.new_scn("maps");
  Elf_Data *data = so->data;
  data->d_buf = glob.maps.data();
  data->d_type = ELF_T_BYTE;
  data->d_size = nmaps * bpf_map_def_sz;
  data->d_align = 4;
  so->shdr->sh_type = SHT_PROGBITS;
  so->shdr->sh_entsize = bpf_map_def_sz;

  // Allow the global arrays to have their actual names.
  eo.symbols.reserve(nmaps);
  for (unsigned i = 0; i < nmaps; ++i)
    eo.symbols.push_back(NULL);

  for (auto i = glob.globals.begin(); i != glob.globals.end(); ++i)
    {
      vardecl *v = i->first;
      if (v->arity <= 0)
	continue;
      unsigned m = i->second.first;
      assert(eo.symbols[m] == NULL);

      BPF_Symbol *s = eo.new_sym(v->name, so, m * bpf_map_def_sz);
      s->sym.st_info = ELF64_ST_INFO(STB_LOCAL, STT_OBJECT);
      s->sym.st_size = bpf_map_def_sz;
      eo.symbols[m] = s;
    }

  // Give internal names to other maps.
  for (unsigned i = 0; i < nmaps; ++i)
    {
      if (eo.symbols[i] != NULL)
	continue;

      BPF_Symbol *s = eo.new_sym(std::string("map.") + std::to_string(i),
				 so, i * bpf_map_def_sz);
      s->sym.st_info = ELF64_ST_INFO(STB_LOCAL, STT_OBJECT);
      s->sym.st_size = bpf_map_def_sz;
      eo.symbols[i] = s;
    }
}

static void
translate_probe(program &prog, globals &glob, derived_probe *dp)
{
  bpf_unparser u(prog, glob);
  u.this_locals = u.new_locals(dp->locals);

  u.set_block(prog.new_block ());

  // Save the input argument early.
  // ??? Ideally this would be deleted as dead code if it were unused;
  // we don't implement that at the moment.  Nor is it easy to support
  // inserting a new start block that would enable retroactively saving
  // this only when needed.
  u.this_in_arg0 = prog.new_reg();
  prog.mk_mov(u.this_ins, u.this_in_arg0, prog.lookup_reg(BPF_REG_1));

  dp->body->visit (&u);
  if (u.in_block())
    u.emit_jmp(u.get_ret0_block());
}

static void
translate_probe_v(program &prog, globals &glob,
		  const std::vector<derived_probe *> &v)
{
  bpf_unparser u(prog, glob);
  block *this_block = prog.new_block();

  for (size_t n = v.size(), i = 0; i < n; ++i)
    {
      u.set_block(this_block);

      derived_probe *dp = v[i];
      u.this_locals = u.new_locals(dp->locals);
      dp->body->visit (&u);
      delete u.this_locals;
      u.this_locals = NULL;

      if (i == n - 1)
	this_block = u.get_ret0_block();
      else
	this_block = prog.new_block();
      if (u.in_block())
	u.emit_jmp(this_block);
    }
}

static BPF_Section *
output_probe(BPF_Output &eo, program &prog,
	     const std::string &name, unsigned flags)
{
  unsigned ninsns = 0, nreloc = 0;

  // Count insns and relocations; drop in jump offset.
  for (auto i = prog.blocks.begin(); i != prog.blocks.end(); ++i)
    {
      block *b = *i;

      for (insn *j = b->first; j != NULL; j = j->next)
	{
	  unsigned code = j->code;
	  if ((code & 0xff) == (BPF_LD | BPF_IMM | BPF_DW))
	    {
	      if (code == BPF_LD_MAP)
		nreloc += 1;
	      ninsns += 2;
	    }
	  else
	    {
	      if (j->is_jmp())
		j->off = b->taken->next->first->id - (j->id + 1);
	      else if (j->is_call())
		j->off = 0;
	      ninsns += 1;
	    }
	}
    }

  bpf_insn *buf = (bpf_insn*) calloc (sizeof(bpf_insn), ninsns);
  assert (buf);
  Elf64_Rel *rel = (Elf64_Rel*) calloc (sizeof(Elf64_Rel), nreloc);
  assert (rel);

  unsigned i = 0, r = 0;
  for (auto bi = prog.blocks.begin(); bi != prog.blocks.end(); ++bi)
    {
      block *b = *bi;

      for (insn *j = b->first; j != NULL; j = j->next)
	{
	  unsigned code = j->code;
	  value *d = j->dest;
	  value *s = j->src1;

	  if (code == BPF_LD_MAP)
	    {
	      unsigned val = s->imm();

	      // Note that we arrange for the map symbols to be first.
	      rel[r].r_offset = i * sizeof(bpf_insn);
	      rel[r].r_info = ELF64_R_INFO(val + 1, R_BPF_MAP_FD);
	      r += 1;

	      buf[i + 0].code = code;
	      buf[i + 0].dst_reg = d->reg();
	      buf[i + 0].src_reg = code >> 8;
	      i += 2;
	    }
	  else if (code == (BPF_LD | BPF_IMM | BPF_DW))
	    {
	      uint64_t val = s->imm();
	      buf[i + 0].code = code;
	      buf[i + 0].dst_reg = d->reg();
	      buf[i + 0].src_reg = code >> 8;
	      buf[i + 0].imm = val;
	      buf[i + 1].imm = val >> 32;
	      i += 2;
	    }
	  else
	    {
	      buf[i].code = code;
	      if (!d)
		d = j->src0;
	      if (d)
		buf[i].dst_reg = d->reg();
	      if (s)
		{
		  if (s->is_reg())
		    buf[i].src_reg = s->reg();
		  else
		    buf[i].imm = s->imm();
		}
	      buf[i].off = j->off;
	      i += 1;
	    }
	}
    }
  assert(i == ninsns);
  assert(r == nreloc);

  BPF_Section *so = eo.new_scn(name);
  Elf_Data *data = so->data;
  data->d_buf = buf;
  data->d_type = ELF_T_BYTE;
  data->d_size = ninsns * sizeof(bpf_insn);
  data->d_align = 8;
  so->free_data = true;
  so->shdr->sh_type = SHT_PROGBITS;
  so->shdr->sh_flags = SHF_EXECINSTR | flags;

  if (nreloc)
    {
      BPF_Section *ro = eo.new_scn(std::string(".rel.") + name);
      Elf_Data *rdata = ro->data;
      rdata->d_buf = rel;
      rdata->d_type = ELF_T_REL;
      rdata->d_size = nreloc * sizeof(Elf64_Rel);
      ro->free_data = true;
      ro->shdr->sh_type = SHT_REL;
      ro->shdr->sh_info = elf_ndxscn(so->scn);
    }

  return so;
}

static void
output_symbols_sections(BPF_Output &eo)
{
  BPF_Section *str = eo.new_scn(".strtab");
  str->shdr->sh_type = SHT_STRTAB;
  str->shdr->sh_entsize = 1;

  unsigned nsym = eo.symbols.size();
  unsigned isym = 0;
  if (nsym > 0)
    {
      BPF_Section *sym = eo.new_scn(".symtab");
      sym->shdr->sh_type = SHT_SYMTAB;
      sym->shdr->sh_link = elf_ndxscn(str->scn);
      sym->shdr->sh_info = nsym + 1;

      Elf64_Sym *buf = new Elf64_Sym[nsym + 1];
      memset(buf, 0, sizeof(Elf64_Sym));

      sym->data->d_buf = buf;
      sym->data->d_type = ELF_T_SYM;
      sym->data->d_size = (nsym + 1) * sizeof(Elf64_Sym);

      stap_strtab_finalize(eo.str_tab, str->data);

      for (unsigned i = 0; i < nsym; ++i)
	{
	  BPF_Symbol *s = eo.symbols[i];
	  Elf64_Sym *b = buf + (i + 1);
	  *b = s->sym;
	  b->st_name = stap_strent_offset(s->name_ent);
	}

      isym = elf_ndxscn(sym->scn);
    }
  else
    stap_strtab_finalize(eo.str_tab, str->data);

  eo.ehdr->e_shstrndx = elf_ndxscn(str->scn);

  for (auto i = eo.sections.begin(); i != eo.sections.end(); ++i)
    {
      BPF_Section *s = *i;
      s->shdr->sh_name = stap_strent_offset(s->name_ent);
      if (s->shdr->sh_type == SHT_REL)
	s->shdr->sh_link = isym;
    }
}

} // namespace bpf

int
translate_bpf_pass (systemtap_session& s)
{
  using namespace bpf;

  if (elf_version(EV_CURRENT) == EV_NONE)
    return 1;

  module_name = s.module_name;
  const std::string module = s.tmpdir + "/" + s.module_filename();
  int fd = open(module.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    return 1;

  BPF_Output eo(fd);
  globals glob;
  int ret = 0;
  try
    {
      translate_globals(glob, s);
      output_maps(eo, glob);

      if (s.be_derived_probes)
        {
          std::vector<derived_probe *> begin_v, end_v;
          sort_for_bpf(s.be_derived_probes, begin_v, end_v);

          if (!begin_v.empty())
            {
              program p;
              translate_probe_v(p, glob, begin_v);
              p.generate();
              output_probe(eo, p, "stap_begin", 0);
            }
          if (!end_v.empty())
            {
              program p;
              translate_probe_v(p, glob, end_v);
              p.generate();
              output_probe(eo, p, "stap_end", 0);
            }
        }
      if (s.generic_kprobe_derived_probes)
        {
          sort_for_bpf_probe_arg_vector kprobe_v;
          sort_for_bpf(s.generic_kprobe_derived_probes, kprobe_v);

          for (auto i = kprobe_v.begin(); i != kprobe_v.end(); ++i)
            {
              program p;
              translate_probe(p, glob, i->first);
              p.generate();
              output_probe(eo, p, i->second, SHF_ALLOC);
            }
        }

      output_kernel_version(eo, s.kernel_base_release);
      output_license(eo);
      output_symbols_sections(eo);

      int64_t r = elf_update(eo.elf, ELF_C_WRITE_MMAP);
      if (r < 0)
	{
	  std::clog << "Error writing output file: "
		    << elf_errmsg(elf_errno()) << std::endl;
	  ret = 1;
	}
    }
  catch (const semantic_error &e)
    {
      s.print_error(e);
      ret = 1;
    }
  catch (...)
    {
      ret = 1;
    }

  close(fd);
  if (ret == 1)
    unlink(s.translated_source.c_str());
  return ret;
}
