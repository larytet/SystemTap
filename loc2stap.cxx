// dwarf location-list-to-staptree translator
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include <cinttypes>
#include <cassert>
#include <cstdlib>

#include <dwarf.h>
#include <elfutils/libdw.h>
#include <elfutils/version.h>

#include "loc2stap.h"
#include "dwflpp.h"

#if ! _ELFUTILS_PREREQ(0, 153)
#define DW_OP_GNU_entry_value 0xf3
#endif

#define N_(x) x


location_context::location_context(target_symbol *ee, expression *pp)
  : e(ee), attr(0), dwbias(0), pc(0), fb_attr(0), cfa_ops(0),
    dw(0), frame_base(0)
{
  // If this code snippet uses a precomputed pointer, create an
  // parameter variable to hold it.
  if (pp)
    {
      vardecl *v = new vardecl;
      v->type = pe_long;
      v->name = "pointer";
      v->tok = ee->tok;
      this->pointer = v;
    }

  // Any non-literal indexes need to have parameter variables too.
  for (unsigned i = 0; i < ee->components.size(); ++i)
    if (ee->components[i].type == target_symbol::comp_expression_array_index)
      {
        vardecl *v = new vardecl;
        v->type = pe_long;
        v->name = "index" + lex_cast(i);
        v->tok = e->tok;
        this->indicies.push_back(v);
      }
}

expression *
location_context::translate_address(Dwarf_Addr addr)
{
  if (dw == NULL)
    return new literal_number(addr);

  int n = dwfl_module_relocations (dw->module);
  Dwarf_Addr reloc_addr = addr;
  const char *secname = "";

  if (n > 1)
    {
      int i = dwfl_module_relocate_address (dw->module, &reloc_addr);
      secname = dwfl_module_relocation_info (dw->module, i, NULL);
    }

  if (n > 0 && !(n == 1 && secname == NULL))
    {
      std::string c;

      if (n > 1 || secname[0] != 0)
	{
	  // This gives us the module name and section name within the
	  // module, for a kernel module.
          c = "({ unsigned long addr = 0; "
              "addr = _stp_kmodule_relocate (\""
              + dw->module_name + "\", \"" + secname + "\", "
              + lex_cast_hex (reloc_addr)
	      + "); addr; })";
	}
      else if (n == 1 && dw->module_name == "kernel" && secname[0] == 0)
	{
	  // elfutils way of telling us that this is a relocatable kernel
	  // address, which we need to treat the same way here as
	  // dwarf_query::add_probe_point does.
          c = "({ unsigned long addr = 0; "
              "addr = _stp_kmodule_relocate (\"kernel\", \"_stext\", "
              + lex_cast_hex (addr - dw->sess.sym_stext)
	      + "); addr; })";
	}
      else
	{
          c = "/* pragma:vma */ "
              "({ unsigned long addr = 0; "
              "addr = _stp_umodule_relocate (\""
              + resolve_path(dw->module_name.c_str()) + "\", "
              + lex_cast_hex (addr)
	      + ", current); addr; })";
	}

      embedded_expr *r = new embedded_expr;
      r->tok = e->tok;
      r->code = c;
      return r;
    }
  else
    return new literal_number(addr);
}

location *
location_context::translate_constant(Dwarf_Attribute *attr)
{
  location *loc;

  switch (dwarf_whatform (attr))
    {
    case DW_FORM_addr:
      {
	Dwarf_Addr addr;
	if (dwarf_formaddr (attr, &addr) != 0)
	  throw SEMANTIC_ERROR(std::string("cannot get constant address: ")
                               + dwarf_errmsg (-1));

        loc = new_location(loc_value);
	loc->program = translate_address(this->dwbias + addr);
      }
      break;

    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
      {
	Dwarf_Block block;
	if (dwarf_formblock (attr, &block) != 0)
	  throw SEMANTIC_ERROR(std::string("cannot get constant block :")
			       + dwarf_errmsg (-1));

        loc = new_location(loc_constant);
	loc->byte_size = block.length;
	loc->constant_block = block.data;
      }
      break;

    case DW_FORM_string:
    case DW_FORM_strp:
      {
	const char *string = dwarf_formstring (attr);
	if (string == NULL)
	  throw SEMANTIC_ERROR(std::string("cannot get constant string:")
			       + dwarf_errmsg (-1));

	loc = new_location(loc_constant);
	loc->byte_size = strlen(string) + 1;
	loc->constant_block = string;
      }
      break;

    default:
      {
	Dwarf_Sword value;
	if (dwarf_formsdata (attr, &value) != 0)
	  throw SEMANTIC_ERROR (std::string("cannot get constant value: ")
                                + dwarf_errmsg (-1));

        loc = new_location(loc_value);
	loc->program = new literal_number(this->dwbias + value);
      }
      break;
    }

  return loc;
}


/* Die in the middle of an expression.  */
static void __attribute__((noreturn))
lose (const Dwarf_Op *lexpr, size_t len, const char *failure, size_t i)
{
  if (lexpr == NULL || i >= len)
    throw SEMANTIC_ERROR(failure);
  else
    throw SEMANTIC_ERROR(std::string(failure)
                         + " in DWARF expression ["
			 + lex_cast(i)
                         + "] at " + lex_cast(lexpr[i].offset)
                         + " (" + lex_cast(lexpr[i].atom)
                         + ": " + lex_cast(lexpr[i].number)
                         + ", " + lex_cast(lexpr[i].number2) + ")");
}

location *
location_context::new_location(location_type t)
{
  location *n = new location(t);
  this->locations.push_back(n);
  return n;
}

location *
location_context::new_location(const location &o)
{
  location *n = new location(o);
  this->locations.push_back(n);
  return n;
}

symbol *
location_context::new_symref(vardecl *var)
{
  symbol *sym = new symbol;
  sym->name = var->name;
  // sym->referent = var;
  return sym;
}

symbol *
location_context::new_local(const char *namebase)
{
  static int counter;
  vardecl *var = new vardecl;
  var->name = std::string(namebase) + lex_cast(counter++);
  var->type = pe_long;
  var->arity = 0;
  var->synthetic = true;
  this->locals.push_back(var);

  return new_symref(var);
}

expression *
location_context::new_target_reg(unsigned int regno)
{
  target_register *reg = new target_register;
  reg->tok = e->tok;
  reg->regno = regno;
  reg->userspace_p = this->userspace_p;
  return reg;
}

expression *
location_context::new_plus_const(expression *l, int64_t r)
{
  if (r == 0)
    return l;

  binary_expression *val = new binary_expression;
  val->tok = e->tok;
  val->op = "+";
  val->left = l;
  val->right = new literal_number(r);
  return val;
}

// If VAL is cheaply copied, return it.
// Otherwise compute the expression to a temp.
expression *
location_context::save_expression(expression *val)
{
  if (dynamic_cast<literal_number *>(val) || dynamic_cast<symbol *>(val))
    return val;

  symbol *sym = new_local(".tmp.");

  assignment *set = new assignment;
  set->tok = e->tok;
  set->op = "=";
  set->left = sym;
  set->right = val;
  this->evals.push_back(set);

  return sym;
}

/* Translate a (constrained) DWARF expression into STAP trees.
   If NEED_FB is null, fail on DW_OP_fbreg, else set *NEED_FB to the
   variable that should be initialized with frame_base.  */

location *
location_context::translate (const Dwarf_Op *expr, const size_t len,
			     const size_t piece_expr_start,
			     location *input, bool may_use_fb,
			     bool computing_value_orig)
{
#define DIE(msg)	lose(expr, len, N_(msg), i)

#define POP(VAR)	if (stack.empty()) goto underflow;	\
			expression *VAR = stack.back();		\
			stack.pop_back();			\
			tos_register = false;

#define PUSH(VAL)	stack.push_back(VAL);			\
			tos_register = false;

  bool saw_stack_value = false;
  bool tos_register = false;
  bool computing_value = computing_value_orig;
  Dwarf_Word piece_size = 0;
  Dwarf_Block implicit_value = Dwarf_Block();
  const Dwarf_Op *implicit_pointer = NULL;
  location temp_piece;
  size_t i;

  {
    std::vector<expression *> stack;

    if (input != NULL)
      switch (input->type)
        {
        case loc_value:
	  assert(computing_value);
          /* FALLTHRU */
        case loc_address:
	  PUSH(input->program);
	  break;
        case loc_register:
	  assert(computing_value);
	  PUSH(new_target_reg(input->regno));
	  tos_register = true;
	  break;
        default:
	  abort();
        }

    for (i = piece_expr_start; i < len; ++i)
      {
	unsigned int reg;
	uint_fast8_t sp;
	Dwarf_Word value;

	if (expr[i].atom != DW_OP_nop
	    && expr[i].atom != DW_OP_piece
	    && expr[i].atom != DW_OP_bit_piece)
	  {
	    if (saw_stack_value)
	      DIE("operations follow DW_OP_stack_value");

	    if (implicit_value.data != NULL)
	      DIE ("operations follow DW_OP_implicit_value");

	    if (implicit_pointer != NULL)
	      DIE ("operations follow DW_OP_GNU_implicit_pointer");
	  }

	switch (expr[i].atom)
	  {
	    /* Basic stack operations.  */
	  case DW_OP_nop:
	    break;

	  case DW_OP_drop:
	    {
	      POP(unused);
	      (void)unused;
	    }
	    break;

	  case DW_OP_dup:
	    sp = 0;
	    goto op_pick;
	    break;

	  case DW_OP_over:
	    sp = 1;
	    goto op_pick;

	  case DW_OP_pick:
	    sp = expr[i].number;
	  op_pick:
	    {
	      size_t stack_size = stack.size();
	      if (sp >= stack_size)
		goto underflow;

	      expression *val = stack[stack_size - 1 - sp];
	      PUSH(save_expression(val));
	    }
	    break;

	  case DW_OP_swap:
	    {
	      POP(a);
	      POP(b);
	      PUSH(a);
	      PUSH(b);
	    }
	    break;

	  case DW_OP_rot:
	    {
	      POP(a);
	      POP(b);
	      POP(c);
	      PUSH(a);
	      PUSH(c);
	      PUSH(b);
	    }
	    break;

	    /* Control flow operations.  */
	  case DW_OP_skip:
	    {
	      Dwarf_Off target = expr[i].offset + 3 + expr[i].number;
	      while (i + 1 < len && expr[i + 1].offset < target)
		++i;
	      if (expr[i + 1].offset != target)
		DIE ("invalid skip target");
	      break;
	    }

	  case DW_OP_bra:
	    // ??? Could copy the stack, recurse, and place both
	    // traces in arms of a ternary_expression.  No point
	    // in doing that if this is never used, however.
	    DIE ("conditional branches not supported");
	    break;

	    /* Memory access.  */
	  case DW_OP_deref:
	    // ??? Target sizeof, not host sizeof.
	    sp = sizeof(void *);
	    goto op_deref;

	  case DW_OP_deref_size:
	    sp = expr[i].number;
	  op_deref:
	    {
	      POP(addr);
	      target_deref *val = new target_deref;
	      val->addr = addr;
	      val->size = sp;
	      val->userspace_p = this->userspace_p;
	      val->tok = e->tok;
	      PUSH(val);
	    }
	    break;

	  case DW_OP_xderef:
	  case DW_OP_xderef_size:
	    // not yet
	    // POP (addr);
	    // POP (as);
	    // push (xderef expr[i].number, (unsigned)addr, (unsigned)as);
	    DIE ("address spaces not supported");

	    /* Constant-value operations.  */

	  case DW_OP_addr:
	    PUSH(translate_address(this->dwbias + expr[i].number));
	    break;

	  case DW_OP_lit0 ... DW_OP_lit31:
	    value = expr[i].atom - DW_OP_lit0;
	    goto op_const;

	  case DW_OP_const1u:
	  case DW_OP_const1s:
	  case DW_OP_const2u:
	  case DW_OP_const2s:
	  case DW_OP_const4u:
	  case DW_OP_const4s:
	  case DW_OP_const8u:
	  case DW_OP_const8s:
	  case DW_OP_constu:
	  case DW_OP_consts:
	    value = expr[i].number;
	  op_const:
	    {
	      literal_number *ln = new literal_number(value);
	      ln->tok = e->tok;
	      PUSH(ln);
	    }
	    break;

	    /* Arithmetic operations.  */
#define UNOP(dw_op, stap_op)					\
	    case DW_OP_##dw_op:					\
	      {							\
		POP(arg);					\
		unary_expression *val = new unary_expression;	\
		val->tok = e->tok;                              \
		val->op = #stap_op;				\
		val->operand = arg;				\
		PUSH(val);					\
	      }							\
	      break

	    UNOP (neg, -);
	    UNOP (not, ~);

#undef UNOP

#define BINOP(dw_op, type, stap_op)		\
	    case DW_OP_##dw_op:			\
	      {					\
		POP(b);				\
		POP(a);				\
		type *val = new type;		\
		val->op = #stap_op;		\
		val->left = a;			\
		val->right = b;			\
		val->tok = e->tok;              \
		PUSH(val);			\
	      }					\
	      break

	    BINOP (and, binary_expression, &);
	    BINOP (minus, binary_expression, -);
	    BINOP (mul, binary_expression, *);
	    BINOP (or, binary_expression, |);
	    BINOP (plus, binary_expression, +);
	    BINOP (shl, binary_expression, <<);
	    BINOP (shr, binary_expression, u>>);
	    BINOP (shra, binary_expression, >>);
	    BINOP (xor, binary_expression, ^);
	    BINOP (div, binary_expression, /);
	    BINOP (mod, binary_expression, %);

	    /* Comparisons are binary operators too.  */
	    BINOP (le, comparison, <=);
	    BINOP (ge, comparison, >=);
	    BINOP (eq, comparison, ==);
	    BINOP (lt, comparison, <);
	    BINOP (gt, comparison, >);
	    BINOP (ne, comparison, !=);

#undef	BINOP

	  case DW_OP_abs:
	    {
	      // Form (arg < 0 ? -arg : arg).
	      POP(arg);

	      comparison *cmp = new comparison;
	      cmp->op = "<";
	      cmp->left = arg;
              cmp->right = new literal_number(0);

	      unary_expression *neg = new unary_expression;
	      neg->op = "-";
	      neg->operand = arg;

	      ternary_expression *val = new ternary_expression;
	      val->cond = cmp;
	      val->truevalue = neg;
	      val->falsevalue = arg;

	      PUSH(val);
	    }
	    break;

	  case DW_OP_plus_uconst:
	    {
	      POP(arg);
	      PUSH(new_plus_const(arg, expr[i].number));
	    }
	    break;

	    /* Register-relative addressing.  */
	  case DW_OP_breg0 ... DW_OP_breg31:
	    reg = expr[i].atom - DW_OP_breg0;
	    value = expr[i].number;
	    goto op_breg;

	  case DW_OP_bregx:
	    reg = expr[i].number;
	    value = expr[i].number2;
	  op_breg:
	    PUSH(new_plus_const(new_target_reg(reg), value));
	    break;

	  case DW_OP_fbreg:
	    if (!may_use_fb)
	      DIE ("DW_OP_fbreg from DW_AT_frame_base");
	    PUSH(new_plus_const(frame_location(), expr[i].number));
	    break;

	    /* Direct register contents.  */
	  case DW_OP_reg0 ... DW_OP_reg31:
	    reg = expr[i].atom - DW_OP_reg0;
	    goto op_reg;

	  case DW_OP_regx:
	    reg = expr[i].number;
	  op_reg:
	    PUSH(new_target_reg(reg));
	    tos_register = true;
	    break;

	    /* Special magic.  */
	  case DW_OP_piece:
	    if (stack.size() > 1)
	      // If this ever happens we could copy the program.
	      DIE ("DW_OP_piece left multiple values on stack");
	    piece_size = expr[i].number;
	    goto end_piece;

	  case DW_OP_stack_value:
	    if (stack.empty())
	      goto underflow;
	    if (stack.size() > 1)
	      DIE ("DW_OP_stack_value left multiple values on stack");
	    saw_stack_value = true;
	    computing_value = true;
	    break;

	  case DW_OP_implicit_value:
	    if (this->attr == NULL)
	      DIE ("DW_OP_implicit_value used in invalid context"
		   " (no DWARF attribute, ABI return value location?)");

	    /* It's supposed to appear by itself, except for DW_OP_piece.  */
	    if (!stack.empty())
	      DIE ("DW_OP_implicit_value follows stack operations");

#if _ELFUTILS_PREREQ (0, 143)
	    if (dwarf_getlocation_implicit_value (this->attr,
						  (Dwarf_Op *) &expr[i],
						  &implicit_value) != 0)
	      DIE ("dwarf_getlocation_implicit_value failed");

	    /* Fake top of stack: implicit_value being set marks it.  */
	    PUSH(NULL);
	    break;
#else
	    DIE ("DW_OP_implicit_value not supported");
	    break;
#endif

#if _ELFUTILS_PREREQ (0, 149)
	  case DW_OP_GNU_implicit_pointer:
	    implicit_pointer = &expr[i];
	    /* Fake top of stack: implicit_pointer being set marks it.  */
	    PUSH(NULL);
	    break;
#endif

	  case DW_OP_call_frame_cfa:
	    // We pick this out when processing DW_AT_frame_base in
	    // so it really shouldn't turn up here.
	    if (!may_use_fb)
	      DIE ("DW_OP_call_frame_cfa while processing frame base");
	    // This is slightly weird/inefficient, but golang is known
	    // to produce DW_OP_call_frame_cfa; DW_OP_consts: 8; DW_OP_plus
	    // instead of a simple DW_OP_fbreg 8.
	    PUSH(frame_location());
	    break;

	  case DW_OP_push_object_address:
	    DIE ("unhandled DW_OP_push_object_address");
	    break;

	  case DW_OP_GNU_entry_value:
	    DIE ("unhandled DW_OP_GNU_entry_value");
	    break;

	  default:
	    DIE ("unrecognized operation");
	    break;
	  }
      }

  end_piece:
    // Finish recognizing exceptions before allocating memory.
    if (i == piece_expr_start)
      {
	assert(stack.empty());
	assert(!tos_register);
	temp_piece.type = loc_unavailable;
      }
    else if (stack.empty())
      goto underflow;
    else if (implicit_pointer != NULL)
      {
	temp_piece.type = loc_implicit_pointer;
	temp_piece.offset = implicit_pointer->number2;

#if !_ELFUTILS_PREREQ (0, 149)
	/* Then how did we get here?  */
	abort ();
#else
	Dwarf_Attribute target;
	if (dwarf_getlocation_implicit_pointer (this->attr, implicit_pointer,
						&target) != 0)
	  DIE ("invalid implicit pointer");
	switch (dwarf_whatattr (&target))
	  {
	  case DW_AT_const_value:
	    temp_piece.target = translate_constant(&target);
	    break;
	  case DW_AT_location:
	    {
	      Dwarf_Op *expr;
	      size_t len;
	      switch (dwarf_getlocation_addr(&target, pc, &expr, &len, 1))
		{
		case 1:  /* Should always happen */
		  if (len > 0)
		    break;
		  /* fallthru */
		case 0:  /* Should never happen */
		  throw SEMANTIC_ERROR("not accessible at this address: "
				       + lex_cast(pc));
		default: /* Should never happen */
		  throw SEMANTIC_ERROR(std::string("dwarf_getlocation_addr: ")
				       + dwarf_errmsg(-1));
		}
	      temp_piece.target = location_from_address(expr, len, NULL);
	    }
	    break;
	  default:
	    DIE ("unexpected implicit pointer attribute!");
	  }
#endif
      }
    else if (implicit_value.data != NULL)
      {
	temp_piece.type = loc_constant;
	temp_piece.byte_size = implicit_value.length;
	temp_piece.constant_block = implicit_value.data;
      }
    else
      {
	expression *val = stack.back();
        if (computing_value || tos_register)
	  {
	    if (target_register *reg = dynamic_cast<target_register *>(val))
	      {
		temp_piece.type = loc_register;
		temp_piece.regno = reg->regno;
	      }
	    else
	      temp_piece.type = loc_value;
	  }
	else
	  temp_piece.type = loc_address;
	temp_piece.program = val;
      }

    // stack goes out of scope here
  }

  if (piece_size != 0)
    {
      temp_piece.byte_size = piece_size;
      temp_piece.piece_next = translate(expr, len, i + 1, input, may_use_fb,
					computing_value_orig);

      // For the first DW_OP_piece, create the loc_noncontiguous container.
      // For subsequent pieces, fall through for normal location return.
      if (piece_expr_start == 0)
	{
	  location *pieces = new_location(temp_piece);
	  location *loc = new_location(loc_noncontiguous);
	  loc->pieces = pieces;

	  size_t total_size = 0;
	  for (location *p = pieces; p != NULL; p = p->piece_next)
	    total_size += p->byte_size;
	  loc->byte_size = total_size;
	  return loc;
	}
    }
  else if (piece_expr_start && piece_expr_start != i)
    DIE ("extra operations after last DW_OP_piece");

  return new_location(temp_piece);

 underflow:
  DIE ("stack underflow");

#undef DIE
#undef PUSH
#undef POP
}

symbol *
location_context::frame_location()
{
  if (this->frame_base == NULL)
    {
      // The main expression uses DW_OP_fbreg, so we need to compute
      // the DW_AT_frame_base attribute expression's value first.
      const Dwarf_Op *fb_ops;
      Dwarf_Op *fb_expr;
      size_t fb_len;

      if (this->fb_attr == NULL)
	{
	  // Lets just assume we want DW_OP_call_frame_cfa.
	  // Some (buggy golang) DWARF producers use that directly in
	  // location descriptions. And at least we should have a chance
	  // to get an actual call frame address that way.
	  goto use_cfa_ops;
	}
      else
	{
	  switch (dwarf_getlocation_addr (this->fb_attr, this->pc,
					  &fb_expr, &fb_len, 1))
	    {
	    case 1: /* Should always happen.  */
	      if (fb_len != 0)
		break;
	      /* fallthru */

	    case 0: /* Shouldn't happen.  */
	      throw SEMANTIC_ERROR("DW_AT_frame_base not accessible "
				   "at this address");

	    default: /* Shouldn't happen.  */
	    case -1:
	      throw SEMANTIC_ERROR
		("dwarf_getlocation_addr (form "
		 + lex_cast(dwarf_whatform (this->fb_attr))
		 + "): " + dwarf_errmsg (-1));
	    }
	}

      // If it is DW_OP_call_frame_cfa then get cfi cfa ops.
      if (fb_len == 1 && fb_expr[0].atom == DW_OP_call_frame_cfa)
	{
	use_cfa_ops:
	  if (this->cfa_ops == NULL)
	    throw SEMANTIC_ERROR("No cfa_ops supplied, "
				 "but needed by DW_OP_call_frame_cfa");
	  fb_ops = this->cfa_ops;
	}
      else
	fb_ops = fb_expr;

      location *fb_loc = translate (fb_ops, fb_len, 0, NULL, false, false);
      assert(fb_loc->type == loc_address);

      this->frame_base = new_local("_fb_");
      assignment *set = new assignment;
      set->op = "=";
      set->left = this->frame_base;
      set->right = fb_loc->program;
      this->evals.push_back(set);
    }
  return this->frame_base;
}

/* Translate a location starting from an address or nothing.  */
location *
location_context::location_from_address (const Dwarf_Op *expr, size_t len,
					 struct location *input)
{
  return translate (expr, len, 0, input, true, false);
}

location *
location_context::translate_offset (const Dwarf_Op *expr, size_t len,
				    size_t i, location *input,
				    Dwarf_Word offset)
{
#define DIE(msg) lose (expr, len, N_(msg), i)

  while (input->type == loc_noncontiguous)
    {
      /* We are starting from a noncontiguous object (DW_OP_piece).
	 Find the piece we want.  */

      struct location *piece = input->pieces;
      while (piece != NULL && offset >= piece->byte_size)
	{
	  offset -= piece->byte_size;
	  piece = piece->piece_next;
	}
      if (piece == NULL)
	DIE ("offset outside available pieces");

      input = piece;
    }

  location *loc = new_location(*input);
  switch (loc->type)
    {
    case loc_address:
      /* The piece we want is actually in memory.  Use the same
	 program to compute the address from the preceding input.  */
      loc->program = new_plus_const(loc->program, offset);
      break;

    case loc_register:
    case loc_implicit_pointer:
      loc->offset += offset;
      break;

    case loc_constant:
      /* This piece has a constant offset.  */
      if (offset >= loc->byte_size)
	DIE ("offset outside available constant block");
      loc->constant_block = (void *)((char *)loc->constant_block + offset);
      loc->byte_size -= offset;
      break;

    case loc_unavailable:
      /* Let it be diagnosed later.  */
      break;

    case loc_value:
      /* The piece we want is part of a computed offset.
	 If it's the whole thing, we are done.  */
      if (offset == 0)
	break;
      DIE ("extract partial rematerialized value");

    default:
      abort ();
    }

  return loc;

#undef DIE
}


/* Translate a location starting from a non-address "on the top of the
   stack".  The *INPUT location is a register name or noncontiguous
   object specification, and this expression wants to find the "address"
   of an object (or the actual value) relative to that "address".  */

location *
location_context::location_relative (const Dwarf_Op *expr, size_t len,
				     location *input)
{
#define DIE(msg)	lose(expr, len, N_(msg), i)

#define POP(VAR)	if (stack.empty())		\
			  goto underflow;		\
			Dwarf_Sword VAR = stack.back();	\
			stack.pop_back()

#define PUSH(VAL)	stack.push_back(VAL)

  std::vector<Dwarf_Sword> stack;
  size_t i;

  for (i = 0; i < len; ++i)
    {
      uint_fast8_t sp;
      Dwarf_Word value;

      switch (expr[i].atom)
	{
	  /* Basic stack operations.  */
	case DW_OP_nop:
	  break;

	case DW_OP_drop:
	  if (stack.empty())
	    {
	      if (input == NULL)
		goto underflow;
	      /* Mark that we have consumed the input.  */
	      input = NULL;
	    }
	  else
	    stack.pop_back();
	  break;

	case DW_OP_dup:
	  sp = 0;
	  goto op_pick;

	case DW_OP_over:
	  sp = 1;
	  goto op_pick;

	case DW_OP_pick:
	  sp = expr[i].number;
	op_pick:
	  {
	    size_t stack_size = stack.size();
	    if (sp < stack_size)
	      PUSH(stack[stack_size - 1 - sp]);
	    else if (sp == stack_size)
	      goto underflow;
	    else
	      goto real_underflow;
	  }
	  break;

	case DW_OP_swap:
	  {
	    POP(a);
	    POP(b);
	    PUSH(a);
	    PUSH(b);
	  }
	  break;

	case DW_OP_rot:
	  {
	    POP(a);
	    POP(b);
	    POP(c);
	    PUSH(a);
	    PUSH(c);
	    PUSH(b);
	  }
	  break;

	  /* Control flow operations.  */
	case DW_OP_bra:
	  {
	    POP (taken);
	    if (taken == 0)
	      break;
	  }
	  /*FALLTHROUGH*/

	case DW_OP_skip:
	  {
	    Dwarf_Off target = expr[i].offset + 3 + expr[i].number;
	    while (i + 1 < len && expr[i + 1].offset < target)
	      ++i;
	    if (expr[i + 1].offset != target)
	      DIE ("invalid skip target");
	    break;
	  }

	  /* Memory access.  */
	case DW_OP_deref:
	case DW_OP_deref_size:
	case DW_OP_xderef:
	case DW_OP_xderef_size:

	  /* Register-relative addressing.  */
	case DW_OP_breg0 ... DW_OP_breg31:
	case DW_OP_bregx:
	case DW_OP_fbreg:

	  /* This started from a register, but now it's following a pointer.
	     So we can do the translation starting from address here.  */
	  // ??? This doesn't really seem correct.
	  // For deref we should take the address that's already on the stack
	  // and use that, assuming that's the end of the program.  If it
	  // isn't, we're not actually going to produce an address from the
	  // result.
	  return location_from_address (expr, len, input);


	  /* Constant-value operations.  */
	case DW_OP_addr:
	  DIE ("static calculation depends on load-time address");
	  // PUSH (this->dwbias + expr[i].number);
	  break;

	case DW_OP_lit0 ... DW_OP_lit31:
	  value = expr[i].atom - DW_OP_lit0;
	  goto op_const;

	case DW_OP_const1u:
	case DW_OP_const1s:
	case DW_OP_const2u:
	case DW_OP_const2s:
	case DW_OP_const4u:
	case DW_OP_const4s:
	case DW_OP_const8u:
	case DW_OP_const8s:
	case DW_OP_constu:
	case DW_OP_consts:
	  value = expr[i].number;
	op_const:
	  PUSH (value);
	  break;

	  /* Arithmetic operations.  */
#define UNOP(dw_op, c_op)			\
	case DW_OP_##dw_op:			\
	  {					\
	    POP (tos);				\
	    PUSH (c_op (tos));			\
	  }					\
	  break
#define BINOP(dw_op, c_op)			\
	case DW_OP_##dw_op:			\
	  {					\
	    POP (b);				\
	    POP (a);				\
	    PUSH (a c_op b);			\
	  }					\
	  break

	  BINOP (and, &);
	  BINOP (div, /);
	  BINOP (mod, %);
	  BINOP (mul, *);
	  UNOP (neg, -);
	  UNOP (not, ~);
	  BINOP (or, |);
	  BINOP (shl, <<);
	  BINOP (shra, >>);
	  BINOP (xor, ^);

	  /* Comparisons are binary operators too.  */
	  BINOP (le, <=);
	  BINOP (ge, >=);
	  BINOP (eq, ==);
	  BINOP (lt, <);
	  BINOP (gt, >);
	  BINOP (ne, !=);

#undef	UNOP
#undef	BINOP

	case DW_OP_abs:
	  {
	    POP (a);
	    PUSH (a < 0 ? -a : a);
	  }
	  break;

	case DW_OP_shr:
	  {
	    POP (b);
	    POP (a);
	    PUSH ((Dwarf_Word)a >> b);
	  }
	  break;

	  /* Simple addition we may be able to handle relative to
	     the starting register name.  */
	case DW_OP_minus:
	  {
	    POP (tos);
	    value = -tos;
	    goto plus;
	  }
	case DW_OP_plus:
	  {
	    POP (tos);
	    value = tos;
	    goto plus;
	  }
	case DW_OP_plus_uconst:
	  value = expr[i].number;
	plus:
	  if (!stack.empty())
	    {
	      /* It's just private diddling after all.  */
	      POP (a);
	      PUSH (a + value);
	      break;
	    }
	  if (input == NULL)
	    goto underflow;

	  /* This is the primary real-world case: the expression takes
	     the input address and adds a constant offset.  */
	  {
	    location *loc = translate_offset (expr, len, i, input, value);
	    if (loc != NULL && i + 1 < len)
	      {
		if (loc->type != loc_address)
		  DIE ("too much computation for non-address location");

		/* This expression keeps going, but further
		   computations now have an address to start with.
		   So we can punt to the address computation generator.  */
		loc = location_from_address (&expr[i + 1], len - i - 1, loc);
	      }
	    return loc;
	  }

	  /* Direct register contents.  */
	case DW_OP_reg0 ... DW_OP_reg31:
	case DW_OP_regx:
	  DIE ("register");
	  break;

	  /* Special magic.  */
	case DW_OP_piece:
	  DIE ("DW_OP_piece");
	  break;

	case DW_OP_push_object_address:
	  DIE ("unhandled DW_OP_push_object_address");
	  break;

	case DW_OP_GNU_entry_value:
	  DIE ("unhandled DW_OP_GNU_entry_value");
	  break;

	default:
	  DIE ("unrecognized operation");
	  break;
	}
    }

  if (input == NULL)
    {
      switch (stack.size())
	{
	case 0:
	  goto real_underflow;
	case 1:
	  /* Could handle this if it ever actually happened.  */
	  DIE ("relative expression computed constant");
	default:
	  DIE ("multiple values left on stack");
	}
    }
  else
    {
      if (!stack.empty())
	DIE ("multiple values left on stack");
      return input;
    }

 underflow:
  if (input != NULL)
    DIE ("cannot handle location expression");

 real_underflow:
   DIE ("stack underflow");

#undef DIE
#undef POP
#undef PUSH
}

/* Translate a staptree fragment for the location expression, using *INPUT
   as the starting location, begin from scratch if *INPUT is null.
   If DW_OP_fbreg is used, it may have a subfragment computing from
   the FB_ATTR location expression.  Return the first fragment created,
   which is also chained onto (*INPUT)->next.  *INPUT is then updated
   with the new tail of that chain.  */

location *
location_context::translate_location (const Dwarf_Op *expr, size_t len,
				      location *input)
{
  switch (input ? input->type : loc_address)
    {
    case loc_address:
      /* We have a previous address computation.
	 This expression will compute starting with that on the stack.  */
      return location_from_address (expr, len, input);

    case loc_noncontiguous:
    case loc_register:
    case loc_value:
    case loc_constant:
    case loc_unavailable:
    case loc_implicit_pointer:
      /* The starting point is not an address computation, but a
	 register or implicit value.  We can only handle limited
	 computations from here.  */
      return location_relative (expr, len, input);

    default:
      abort ();
    }
}


/* Translate a staptree fragment for a direct argument VALUE.  */
location *
location_context::translate_argument (expression *value)
{
  location *loc = new_location(loc_address);
  loc->program = value;
  return loc;
}

location *
location_context::translate_argument (vardecl *var)
{
  return translate_argument(new_symref(var));
}

/* Slice up an object into pieces no larger than MAX_PIECE_BYTES,
   yielding a loc_noncontiguous location unless LOC is small enough.  */
location *
location_context::discontiguify(location *loc, Dwarf_Word total_bytes,
				Dwarf_Word max_piece_bytes)
{
  /* Constants are always copied byte-wise, but we may need to
     truncate to the total_bytes requested here. */
  if (loc->type == loc_constant)
    {
      if (loc->byte_size > total_bytes)
	loc->byte_size = total_bytes;
      return loc;
    }

  bool pieces_small_enough;
  if (loc->type != loc_noncontiguous)
    pieces_small_enough = total_bytes <= max_piece_bytes;
  else
    {
      pieces_small_enough = true;
      for (location *p = loc->pieces; p != NULL; p = p->piece_next)
	if (p->byte_size > max_piece_bytes)
	  {
	    pieces_small_enough = false;
	    break;
	  }
    }
  if (pieces_small_enough)
    return loc;

  location noncontig(loc_noncontiguous);
  noncontig.byte_size = total_bytes;

  switch (loc->type)
    {
    case loc_address:
      {
	expression *addr = save_expression(loc->program);

	/* Synthesize pieces that just compute "addr + N".  */
	Dwarf_Word offset = 0;
	location **last = &noncontig.pieces;
	while (offset < total_bytes)
	  {
	    Dwarf_Word size = total_bytes - offset;
	    if (size > max_piece_bytes)
	      size = max_piece_bytes;

	    location *piece = new_location(loc_address);
	    piece->byte_size = size;
	    piece->program = new_plus_const(addr, offset);
	    *last = piece;
	    last = &piece->piece_next;

	    offset += size;
	  }
	*last = NULL;
      }
      break;

    case loc_value:
      throw SEMANTIC_ERROR("stack value too big for fetch ???");

    case loc_register:
      throw SEMANTIC_ERROR("single register too big for fetch/store ???");

    case loc_implicit_pointer:
      throw SEMANTIC_ERROR("implicit pointer too big for fetch/store ???");

    case loc_noncontiguous:
      /* Could be handled if it ever happened.  */
      throw SEMANTIC_ERROR("cannot support noncontiguous location");

    default:
      abort ();
    }

  return new_location(noncontig);
}

static Dwarf_Word
pointer_stride (Dwarf_Die *typedie)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Die die_mem = *typedie;
  int typetag = dwarf_tag (&die_mem);
  while (typetag == DW_TAG_typedef ||
         typetag == DW_TAG_const_type ||
         typetag == DW_TAG_volatile_type ||
         typetag == DW_TAG_restrict_type)
    {
      if (dwarf_attr_integrate (&die_mem, DW_AT_type, &attr_mem) == NULL
          || dwarf_formref_die (&attr_mem, &die_mem) == NULL)
        // TRANSLATORS: This refers to the basic type,
	// (stripped of const/volatile/etc.)
	throw SEMANTIC_ERROR (std::string("cannot get inner type of type ")
			      + (dwarf_diename (&die_mem) ?: "<anonymous>")
			      + " " + dwarf_errmsg (-1));
      typetag = dwarf_tag (&die_mem);
    }

  if (dwarf_attr_integrate (&die_mem, DW_AT_byte_size, &attr_mem) != NULL)
    {
      Dwarf_Word stride;
      if (dwarf_formudata (&attr_mem, &stride) == 0)
        return stride;
      throw SEMANTIC_ERROR (std::string("cannot get byte_size attribute "
					"for array element type ")
			    + (dwarf_diename (&die_mem) ?: "<anonymous>")
			    + " " + dwarf_errmsg (-1));
    }

  throw SEMANTIC_ERROR("confused about array element size");
}

static Dwarf_Word
array_stride (Dwarf_Die *typedie)
{
  Dwarf_Attribute attr_mem;
  if (dwarf_attr_integrate (typedie, DW_AT_byte_stride, &attr_mem) != NULL)
    {
      Dwarf_Word stride;
      if (dwarf_formudata (&attr_mem, &stride) == 0)
        return stride;
      throw SEMANTIC_ERROR(std::string("cannot get byte_stride "
				       "attribute array type ")
			   + (dwarf_diename (typedie) ?: "<anonymous>")
			   + " " + dwarf_errmsg (-1));
    }

  Dwarf_Die die_mem;
  if (dwarf_attr_integrate (typedie, DW_AT_type, &attr_mem) == NULL
      || dwarf_formref_die (&attr_mem, &die_mem) == NULL)
    throw SEMANTIC_ERROR(std::string("cannot get element type "
				     "of array type ")
			 + (dwarf_diename (typedie) ?: "<anonymous>")
			 + " " + dwarf_errmsg (-1));

  return pointer_stride (&die_mem);
}

/* Determine the maximum size of a base type, from some DIE in the CU.  */
static Dwarf_Word
max_fetch_size (Dwarf_Die *die)
{
  Dwarf_Die cu_mem;
  uint8_t address_size;
  Dwarf_Die *cu = dwarf_diecu (die, &cu_mem, &address_size, NULL);
  if (cu == NULL)
    throw SEMANTIC_ERROR(std::string("cannot determine compilation unit "
				     "address size from ")
			 + dwarf_diename (die)
			 + " " + dwarf_errmsg (-1));

  return address_size;
}

location *
location_context::translate_array_1(Dwarf_Die *anydie, Dwarf_Word stride,
				    location *loc, expression *index)
{
  uint64_t const_index = 0;
  if (literal_number *lit = dynamic_cast<literal_number *>(index))
    {
      const_index = lit->value;
      index = NULL;
    }

  while (loc->type == loc_noncontiguous)
    {
      if (index)
	throw SEMANTIC_ERROR("cannot dynamically index noncontiguous array");

      Dwarf_Word offset = const_index * stride;
      location *piece = loc->pieces;
      while (piece && offset >= piece->byte_size)
	{
	  offset -= piece->byte_size;
	  piece = piece->piece_next;
	}
      if (piece == NULL)
	throw SEMANTIC_ERROR("constant index is outside noncontiguous array");
      if (offset % stride != 0 || piece->byte_size < stride)
	throw SEMANTIC_ERROR("noncontiguous array splits elements");
      
      const_index = offset / stride;
      loc = piece;
    }

  location *nloc = new_location(*loc);
  switch (loc->type)
    {
    case loc_address:
      if (index)
	{
	  binary_expression *m = new binary_expression;
	  m->op = "*";
	  m->left = index;
	  m->right = new literal_number(stride);
	  m->right->tok = index->tok;
	  m->tok = index->tok;
	  binary_expression *a = new binary_expression;
	  a->op = "+";
	  a->left = loc->program;
	  a->right = m;
	  a->tok = index->tok;
	  nloc->program = a;
	}
      else
	nloc->program = new_plus_const(loc->program, const_index * stride);
      break;

    case loc_register:
      if (index)
	throw SEMANTIC_ERROR("cannot index array stored in a register");
      if (const_index > max_fetch_size(anydie) / stride)
	throw SEMANTIC_ERROR("constant index is outside "
			     "array held in register");
      nloc->offset += const_index * stride;
      break;

    case loc_constant:
      if (index)
	throw SEMANTIC_ERROR("cannot index into constant value");
      if (const_index > loc->byte_size / stride)
	throw SEMANTIC_ERROR("constant index is outside "
			     "constant array value");
      nloc->byte_size = stride;
      nloc->constant_block
	= (void *)((char *)nloc->constant_block + const_index * stride);
      break;

    case loc_unavailable:
      if (index || const_index)
	throw SEMANTIC_ERROR("cannot index into unavailable value");
      break;

    case loc_value:
      if (index || const_index)
	throw SEMANTIC_ERROR("cannot index into computed value");
      break;

    default:
      abort();
    }

  return nloc;
}

location *
location_context::translate_array(Dwarf_Die *typedie,
				  location *input, expression *index)
{
  assert(dwarf_tag (typedie) == DW_TAG_array_type ||
	 dwarf_tag (typedie) == DW_TAG_pointer_type);
  return translate_array_1(typedie, array_stride (typedie), input, index);
}

location *
location_context::translate_array_pointer (Dwarf_Die *typedie,
					   location *input,
					   expression *index)
{
  return translate_array_1(typedie, pointer_stride (typedie), input, index);
}

location *
location_context::translate_array_pointer (Dwarf_Die *typedie,
					   location *input,
					   vardecl *index_var)
{
  return translate_array_pointer (typedie, input, new_symref(index_var));
}
