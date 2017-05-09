#ifndef LOC2STAP_H
#define LOC2STAP_H 1

#include "staptree.h"
#include <elfutils/libdw.h>

enum location_type
{
  loc_address, loc_register, loc_noncontiguous, loc_unavailable,
  loc_value, loc_constant, loc_implicit_pointer,
  loc_decl, loc_fragment, loc_final
};

struct location
{
  location_type type;
  unsigned byte_size;

  // loc_address, loc_value, loc_fragment, loc_final
  expression *program;

  // loc_register
  unsigned int regno;

  // loc_noncontiguous
  location *pieces;
  location *piece_next;

  // loc_constant
  const void *constant_block;

  // loc_implicit_pointer
  location *target;

  // loc_register, loc_implicit_pointer
  Dwarf_Word offset;

  location(location_type t = loc_unavailable)
    : type(t), byte_size(0), program(0), regno(0), pieces(0),
      piece_next(0), constant_block(0), target(0), offset(0)
  { }
};

class dwflpp;
class location_context
{
public:
  target_symbol *e;

  // These three form the argument list to the function, in sequence.
  // They will be referenced by the expression(s) computing the location.
  vardecl *pointer;
  std::vector<vardecl *> indicies;
  vardecl *value;

  Dwarf_Attribute *attr;
  Dwarf_Addr dwbias;
  Dwarf_Addr pc;
  Dwarf_Attribute *fb_attr;
  const Dwarf_Op *cfa_ops;
  dwflpp *dw;

  // Temporaries required while computing EVALS and LOCATIONS.
  symbol *frame_base;
  std::vector<vardecl *> locals;
  std::vector<expression *> evals;

  // A set of locations which have been requested to be evaluated.
  // The last one can be considered "current", and thus the result
  // is locations.back()->program.
  // ??? This is the old loc->next list.  Should we just have a
  // single pointer here instead?
  std::vector<location *> locations;	// ??? old loc->next list

  // The dwarf is within a user (vs kernel) context.
  bool userspace_p;

  location *new_location(location_type);
  location *new_location(const location &old);

  symbol *new_symref(vardecl *var);
  symbol *new_local(const char *namebase);
  expression *new_target_reg(unsigned regno);
  static expression *new_plus_const(expression *, int64_t);
  expression *save_expression(expression *);
  symbol *frame_location();

  location *translate(const Dwarf_Op *expr, size_t len, size_t start,
		      location *input, bool may_use_fb, bool computing_value);
  location *location_from_address (const Dwarf_Op *expr, size_t len,
				   location *input);
  location *translate_offset (const Dwarf_Op *expr, size_t len, size_t i,
			      location *input, Dwarf_Word offset);
  location *location_relative (const Dwarf_Op *expr, size_t len,
			       location *input);
  location *translate_array_1(Dwarf_Die *anydie, Dwarf_Word stride,
			      location *loc, expression *index);

public:
  location_context(target_symbol *, expression * = NULL);

  expression *translate_address(Dwarf_Addr a);
  location *translate_constant(Dwarf_Attribute *a);
  location *translate_location(const Dwarf_Op *locexpr,
			       size_t locexprlen, location *input);
  location *translate_argument (expression *value);
  location *translate_argument (vardecl *var);
  location *translate_array(Dwarf_Die *typedie, location *, expression *);
  location *translate_array_pointer(Dwarf_Die *typedie, location *input,
				    expression *index);
  location *translate_array_pointer (Dwarf_Die *, location *input,
				     vardecl *index);
  location *discontiguify(location *loc, Dwarf_Word total_bytes,
			  Dwarf_Word max_piece_bytes);

};

#endif
