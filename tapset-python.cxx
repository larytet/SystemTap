// tapset for python
// Copyright (C) 2016-2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "session.h"
#include "tapsets.h"
#include "translate.h"
#include "util.h"

#include <cstring>
#include <string>
#include <ext/stdio_filebuf.h>

using namespace std;
using namespace __gnu_cxx;

static const string TOK_PYTHON2("python2");
static const string TOK_PYTHON3("python3");
static const string TOK_MODULE("module");
static const string TOK_FUNCTION("function");
static const string TOK_CALL("call");
static const string TOK_RETURN("return");


// ------------------------------------------------------------------------
// python derived probes
// ------------------------------------------------------------------------


struct python_probe_info
{
    interned_string module;
    interned_string function;
    bool has_call;
    
    python_probe_info (interned_string m, interned_string f, bool hc = false)
	: module(m), function(f), has_call(hc) {}
};


struct python_derived_probe: public derived_probe
{
  int python_version;
  interned_string module;
  interned_string function;
  bool has_return;
  bool has_call;

  python_derived_probe (systemtap_session &, probe* p, probe_point* l,
			int python_version, interned_string module,
			interned_string function, bool has_return,
			bool has_call);
  void join_group (systemtap_session& s);
  unsigned int flags();
  string break_definition();
};


struct python_derived_probe_group: public generic_dpg<python_derived_probe>
{
private:
  systemtap_session &s;
  vector<python_derived_probe* > python2_probes;
  embeddedcode *python2_embedded;
  vector<python_derived_probe* > python3_probes;
  embeddedcode *python3_embedded;

public:
  python_derived_probe_group(systemtap_session &s) :
      s(s), python2_embedded(NULL), python3_embedded(NULL) {}
  void enroll (python_derived_probe* probe);
  void emit_module_decls (systemtap_session& ) { }
  void emit_module_init (systemtap_session& ) { }
  void emit_module_exit (systemtap_session& ) { }
};


struct python_functioncall_expanding_visitor: public update_visitor
{
  python_functioncall_expanding_visitor (systemtap_session& s, int pv)
    : sess(s), python_version(pv) {}

  systemtap_session& sess;
  int python_version;

  void visit_functioncall (functioncall* e);
};


struct python_var_expanding_visitor: public var_expanding_visitor
{
  python_var_expanding_visitor(systemtap_session& s, int pv)
    : var_expanding_visitor(s), python_version(pv) {}

  int python_version;

  void visit_target_symbol (target_symbol* e);
};


struct python_builder: public derived_probe_builder
{
private:
  int resolve(systemtap_session& s,
	      const unsigned python_ver,
	      interned_string module,
	      interned_string function,
	      vector<python_probe_info *> &results);

  // python2-related info
  derived_probe* python2_procfs_probe;
  unsigned python2_key;

  // python3-related info
  derived_probe* python3_procfs_probe;
  unsigned python3_key;

public:
  python_builder() : python2_procfs_probe(NULL),
		     python2_key(0),
		     python3_procfs_probe(NULL),
		     python3_key(0) {}

  void build(systemtap_session & sess, probe * base,
	     probe_point * location,
	     literal_map_t const & parameters,
	     vector<derived_probe *> & finished_results);
  virtual string name() { return "python builder"; }
};


python_derived_probe::python_derived_probe (systemtap_session &, probe* p,
					    probe_point* l,
					    int pv,
					    interned_string m,
					    interned_string f,
					    bool hr,
					    bool hc):
  derived_probe (p, l, true /* .components soon rewritten */ ),
  python_version(pv), module(m), function(f), has_return(hr),
  has_call(hc)
{
  return;
}


void
python_derived_probe::join_group (systemtap_session &s)
{
  if (! s.python_derived_probes)
    {
      s.python_derived_probes = new python_derived_probe_group (s);
    }
  s.python_derived_probes->enroll (this);
  this->group = s.python_derived_probes;
}


unsigned int
python_derived_probe::flags ()
{
    return (this->has_call ? 2 : (this->has_return ? 1 : 0));
}


string
python_derived_probe::break_definition ()
{
    stringstream outstr;
    outstr << this->module << "|" << this->function << "|"
	   << hex << this->flags() << dec;
    return outstr.str();
}


void
python_derived_probe_group::enroll (python_derived_probe* p)
{
  if (p->python_version == 2)
    {
      python2_probes.push_back(p);
      // Create/update the global synthetic embedded code.
      if (python2_embedded == NULL)
        {
	  python2_embedded = new embeddedcode;
	  s.embeds.push_back(python2_embedded);
	}

      unsigned index = 0;
      stringstream data;
      data << "/* ---- python 2 probes ---- */\n";
      data << "static const char python2_probe_info[] =";
      for (auto iter = python2_probes.begin(); iter != python2_probes.end();
	   iter++)
        {
	  data << "\n  \"b " << (*iter)->break_definition()
	       << "|" << index++ << "\\n\"";
	}
      data << ";\n";
      python2_embedded->code = data.str();
    }
  else if (p->python_version == 3)
    {
      python3_probes.push_back(p);

      // Create/update the global synthetic embedded code.
      if (python3_embedded == NULL)
        {
	  python3_embedded = new embeddedcode;
	  s.embeds.push_back(python3_embedded);
	}

      unsigned index = 0;
      stringstream data;
      data << "/* ---- python 3 probes ---- */\n";
      data << "static const char python3_probe_info[] =";
      for (auto iter = python3_probes.begin(); iter != python3_probes.end();
	   iter++)
        {
	  data << "\n  \"b " << (*iter)->break_definition()
	       << "|" << index++ << "\\n\"";
	}
      data << ";\n";
      python3_embedded->code = data.str();
  }
  else
    throw SEMANTIC_ERROR(_F("Unknown python version: %d", p->python_version));
}


void
python_functioncall_expanding_visitor::visit_functioncall (functioncall* e)
{
  // If it isn't one of the functions we're interested in, we're done.
  if (e->function != "python_print_backtrace"
      && e->function != "python_sprint_backtrace")
    {
      provide (e);
      return;
    }

  // Construct a new function call that replaces the generic python
  // backtrace function call with a python version specific call with
  // the right argument.
  target_symbol *tsym = new target_symbol;
  tsym->tok = e->tok;
  tsym->name = "$arg3";
  tsym->synthetic = true;

  functioncall *fcall = new functioncall;
  fcall->tok = e->tok;
  if (python_version == 2)
    fcall->function = (e->function == "python_print_backtrace"
		       ? "python2_print_backtrace"
		       : "python2_sprint_backtrace");
  else
    fcall->function = (e->function == "python_print_backtrace"
		       ? "python3_print_backtrace"
		       : "python3_sprint_backtrace");
  fcall->type = e->type;
  fcall->type_details = e->type_details;
  fcall->args.push_back (tsym);
  provide (fcall);
}


void
python_var_expanding_visitor::visit_target_symbol (target_symbol* e)
{
    // Convert '$$parms', '$$locals', and '$$vars' references to a
    // function call.
    if (e->name == "$$parms" || e->name == "$$locals" || e->name == "$$vars")
      {
	int flags = (e->name == "$$parms"
		     ? 0 : (e->name == "$$locals" ? 1 : 2));

	functioncall* fcall = new functioncall;
	fcall->tok = e->tok;
	fcall->function = (python_version == 2
			   ? "python2_get_locals" : "python3_get_locals");
	fcall->type = pe_string;

	target_symbol *tsym = new target_symbol;
	tsym->tok = e->tok;
	tsym->name = "$arg3";
        tsym->synthetic = true;
	fcall->args.push_back(tsym);

	literal_number* ln = new literal_number(flags);
	ln->tok = e->tok;
	fcall->args.push_back(ln);

	provide (fcall);
	return;
      }

    // Here we're going to try to look up the value of a python
    // variable. However, at compile time we can't really know if that
    // variable exists. So, the user will get a runtime error if the
    // variable doesn't exist, not a compile-time error.
    //
    // Note that we also can't know what type the variable is
    // (assuming it exists). Another wrinkle is that python variables
    // are polymorphic and can change the type they hold, like this:
    //
    //    x = "abc"
    //    x = 10
    //
    // So, we can't really find out if a particular python variable
    // exists until runtime and we can't know the type. So, we treat
    // all python variables as string variables.
    //
    // Also note we've got one final problem here. This code ends up
    // in a systemtap marker probe, which has $arg1 ... $argN
    // variables. The user shouldn't be referencing those variables,
    // but we need to refer to them (to use the python frame pointer
    // in $arg3 for example). So, we'll do the python variable
    // expansion first, and later the python backtrace request
    // expansions (which will add a reference to $arg3). So, order is
    // important. See python_builder::build().
    //
    // We mark the generated $arg3 as synethic to prevent infinite
    // recursive expansion.
    //
    if (e->name[0] == '$' && !e->synthetic)
      {
	// We need a function call that turns the final object pointer
	// into a string representation of that object.
	functioncall *repr_fcall = new functioncall;
	repr_fcall->tok = e->tok;
	repr_fcall->function = (python_version == 2
				? "Py2Object_Repr" : "Py3Object_Repr");
	repr_fcall->type = pe_string;

	functioncall *var_fcall = new functioncall;
	var_fcall->tok = e->tok;
	var_fcall->function = (python_version == 2 ? "python2_get_var_obj"
			       : "python3_get_var_obj");
	var_fcall->type = pe_long;

	target_symbol *tsym = new target_symbol;
	tsym->tok = e->tok;
	tsym->name = "$arg3";
        tsym->synthetic = true;
	var_fcall->args.push_back(tsym);

	// We want 'foo', not '$foo'.
	interned_string new_name = e->name.substr(1, e->name.size());
	literal_string* ls = new literal_string(new_name);
	ls->tok = e->tok;
	var_fcall->args.push_back(ls);

	functioncall *last_fcall = var_fcall;
	// Here we try to handle array indexing. Note that we can't
	// really know if the python variable type supports array
	// indexing at compile time. If the python variable type
	// doesn't support array indexing, the user will get an error
	// at runtime.
	//
	// FIXME: Needs a while loop to handle '$foo->bar[0]->baz'...
	    
	const target_symbol::component* c =
	  e->components.empty() ? NULL : &e->components[0];
	if (c)
	  {
	    if (c->type == target_symbol::comp_literal_array_index)
	      {
		literal_number* ln = new literal_number(c->num_index);
		ln->tok = e->tok;
		last_fcall->args.push_back(ln);
	      }
	    else if (c->type == target_symbol::comp_struct_member)
	    {
		functioncall *fcall = new functioncall;
		fcall->tok = e->tok;
		fcall->function = (python_version == 2 ? "Py2Object_GetAttr"
				   : "Py3Object_GetAttr");
		fcall->type = pe_long;

		fcall->args.push_back(last_fcall);

		literal_string *ls = new literal_string(c->member);
		ls->tok = e->tok;

		fcall->args.push_back(ls);
		last_fcall = fcall;
	    }
	    else if (c->type == target_symbol::comp_expression_array_index)
	      throw SEMANTIC_ERROR(_("unhandled expression array indexing"));
	    else
	      throw SEMANTIC_ERROR(_("unhandled array indexing type"));
	  }
	repr_fcall->args.push_back(last_fcall);

	provide (repr_fcall);
	return;
      }

    // We must have been called recursively (on a synthetic $arg3 from
    // a parent expansion); nothing to do.
    provide (e);
}


int
python_builder::resolve(systemtap_session& s,
			const unsigned python_ver,
			interned_string module,
			interned_string function,
			vector<python_probe_info *> & results)
{
  vector<string> args;
  int child_out = -1;
  int child_err = -1;

  assert_no_interrupts();

  if (python_ver == 2)
      args.push_back(PYTHON_BASENAME);
  else
      args.push_back(PYTHON3_BASENAME);
  args.push_back(string(PKGLIBDIR)
		 + "/python/stap-resolve-module-function.py");
  if (s.verbose > 2)
      args.push_back("-v");
  args.push_back(module);
  args.push_back(function);

  pid_t child = stap_spawn_piped(s.verbose, args, NULL, &child_out,
				 (s.verbose > 2 ? &child_err : NULL));
  if (child <= 0)
      return -1;

  // Read stderr from the child.
  if (s.verbose > 2)
    {
      stdio_filebuf<char> in(child_err, ios_base::in);
      clog << &in;
      in.close();
    }

  // Read stdout from the child. Each line should contain 'MODULE
  // FUNCTION [FLAG]'
  stdio_filebuf<char> buf(child_out, ios_base::in);
  istream in(&buf);
  string line;
  while (getline(in, line))
    {
      vector<string> tokens;

      if (line.empty())
	continue;
      tokenize(line, tokens);
      if (tokens.size() == 2)
	results.push_back(new python_probe_info(tokens[0], tokens[1]));
      else if (tokens.size() == 3)
	results.push_back(new python_probe_info(tokens[0], tokens[1],
						tokens[2] == "call"));
      else
	throw SEMANTIC_ERROR(_F("Unknown output from stap-resolve-module-function.py: %s", line.c_str()));
    }
  buf.close();

  return stap_waitpid(s.verbose, child);
}

void
python_builder::build(systemtap_session & sess, probe * base,
		      probe_point * location,
		      literal_map_t const & parameters,
		      vector<derived_probe *> & finished_results)
{
  interned_string module, function;
  unsigned python_version = has_null_param (parameters, TOK_PYTHON2) ? 2 : 3;
  bool has_module = get_param (parameters, TOK_MODULE, module);
  bool has_function = get_param (parameters, TOK_FUNCTION, function);
  bool has_return = has_null_param (parameters, TOK_RETURN);
  bool has_call_token = has_null_param (parameters, TOK_CALL);

  if (!has_module || module == "")
    throw SEMANTIC_ERROR(_("The python module name must be specified."));
  if (!has_function || function == "")
    throw SEMANTIC_ERROR(_("The python function name must be specified."));

  vector<python_probe_info *> results;
  if (resolve(sess, python_version, module, function, results) != 0)
    throw SEMANTIC_ERROR(_("The python module/function name cannot be resolved."));

  auto iter = results.begin();
  while (iter != results.end())
    {
      bool has_call = (has_call_token || ((*iter)->has_call && !has_return));
      assert_no_interrupts();
      assert (location->components.size() >= 3);
      assert (location->components[1]->functor == TOK_MODULE);
      assert (location->components[2]->functor == TOK_FUNCTION);

      // Create a new probe point location.
      probe_point *pp = new probe_point (*location);

      // The new probe point location will have all wildcards
      // expanded, so the new location is well-formed.
      pp->well_formed = true;

      // Create a new 'module' component.
      probe_point::component* ppc
	  = new probe_point::component (TOK_MODULE,
					new literal_string ((*iter)->module),
					true /* from_glob */);
      ppc->tok = location->components[1]->tok;
      pp->components[1] = ppc;

      // Create a new 'function' component.
      ppc = new probe_point::component (TOK_FUNCTION,
					new literal_string ((*iter)->function),
					true /* from_glob */);
      ppc->tok = location->components[2]->tok;
      pp->components[2] = ppc;

      // If needed, create a new 'call' component.
      if (has_call && !has_call_token)
        {
	  ppc = new probe_point::component (TOK_CALL);
	  pp->components.push_back(ppc);
	}

      // Create (if necessary) the python 2 procfs probe which the
      // HelperSDT python module reads to get probe information.
      if (python_version == 2 && python2_procfs_probe == NULL)
        {
	  stringstream code;
	  const token* tok = base->body->tok;

	  // Notice this synthetic probe has no body. That's OK,
	  // since we'll point it at our internal buffer.
	  code << "probe procfs(\"_stp_python2_probes\").read {" << endl;
	  code << "}" << endl;
	  probe *base_probe = parse_synthetic_probe (sess, code, tok);
	  if (!base_probe)
	    throw SEMANTIC_ERROR (_("can't create python2 procfs probe"),
				  tok);
	  vector<derived_probe *> results;
	  derive_probes(sess, base_probe, results);
	  if (results.size() != 1)
	    throw SEMANTIC_ERROR (_F("wrong number of probes derived (%d),"
				     " should be 1", (int)results.size()));
	  python2_procfs_probe = results[0];
	  finished_results.push_back(results[0]);
	      
	  // Now that we've got our procfs derived probe, point it at
	  // our internal buffer that we're going to create/update in
	  // python_derived_probe_group::enroll().
	  python2_procfs_probe->use_internal_buffer("python2_probe_info");
	}

      // Create (if necessary) the python 3 procfs probe which the
      // HelperSDT python module reads to get probe information.
      else if (python_version == 3 && python3_procfs_probe == NULL)
        {
	  stringstream code;
	  const token* tok = base->body->tok;

	  // Notice this synthetic probe has no body. That's OK,
	  // since we'll point it at our internal buffer.
	  code << "probe procfs(\"_stp_python3_probes\").read {" << endl;
	  code << "}" << endl;
	  probe *base_probe = parse_synthetic_probe (sess, code, tok);
	  if (!base_probe)
	    throw SEMANTIC_ERROR (_("can't create python3 procfs probe"),
				  tok);
	  vector<derived_probe *> results;
	  derive_probes(sess, base_probe, results);
	  if (results.size() != 1)
	    throw SEMANTIC_ERROR (_F("wrong number of probes derived (%d),"
				     " should be 1", (int)results.size()));
	  python3_procfs_probe = results[0];
	  finished_results.push_back(results[0]);
	      
	  // Now that we've got our procfs derived probe, point it at
	  // our internal buffer that we're going to create/update in
	  // python_derived_probe_group::enroll().
	  python3_procfs_probe->use_internal_buffer("python3_probe_info");

	  // For python 3, the python helper module also sends us some
	  // information via a tracepoint. Hook up a probe to it.
	  stringstream code2;
	  code2 << "probe process(\"" << PYTHON3_BASENAME
		<< "\").library(\"" << PY3EXECDIR
		<< "/HelperSDT/_HelperSDT.*.so\").provider(\"HelperSDT\")"
		<< ".mark(\"Init\") {"
		<< endl;
	  code2 << "  if (user_string($arg1) != module_name()) { next }" << endl;
	  code2 << "  python3_initialize($arg2)" << endl;
	  code2 << "}" << endl;

	  probe *mark_probe = parse_synthetic_probe (sess, code2, tok);
	  if (!mark_probe)
	      throw SEMANTIC_ERROR (_("can't create python init mark probe"),
				    tok);
	  derive_probes(sess, mark_probe, finished_results);
      }

      stringstream code;
      const token* tok = base->body->tok;
      code << "probe process(\""
	   << (python_version == 2 ? PYTHON_BASENAME : PYTHON3_BASENAME)
	   << "\").library(\""
	   << (python_version == 2 ? PYEXECDIR : PY3EXECDIR)
	  // For python2, the name of the .so file is '_HelperSDT.so'. For
	  // python3, the name varies based on the python3 version number
	  // and architecture. On i686, the name of the .so file is
	  // '_HelperSDT.cpython-35m-i386-linux-gnu.so'. So, we'll use
	  // a wildcard to find it.
	   << (python_version == 2 ? "/HelperSDT/_HelperSDT.so\")"
	       : "/HelperSDT/_HelperSDT.*.so\")")
	   << ".provider(\"HelperSDT\").mark(\""
	   << (has_return ? "PyTrace_RETURN"
	       : (has_call ? "PyTrace_CALL" : "PyTrace_LINE"))
	   << "\") {" << endl;

      // Make sure we're in the right probe. To do this, we have to
      // make sure that this method has some cool properties:
      //
      // - It must be unique system-wide, to avoid collisions between
      //   concurrent users of python probes.
      //
      // - It must be unique within a particular systemtap script, so
      //   that distinct probe handlers get run.
      //
      // - It must be computable from systemtap at run-time (since
      //   compile-time can't be unique enough)
      //
      // - It must be passable to the HelperSDT python module, so that
      //   we can get it back when the mark probe hits.
      //
      // So, the method we use is by checking the module name (which
      // is unique system-wide and computed at run-time) and the probe
      // 'key' (which is unique to this script and computed at
      // compile-time).
      //
      // The 'key' (which is just an index) is passed down to the
      // HelperSDT python module via the procfs file. When the
      // HelperSDT module sees an event (function call, function line
      // number, or function return) that we're interested in, it
      // calls down into the _HelperSDT C python extension that
      // contains SDT markers. The first 2 arguments to the markers are
      // the module name and key.
      //
      // The 'key' values aren't unique system wide (and just start at
      // 0), but the combination of module name and key is unique
      // system-wide.
      code << "  if ($arg2 != "
	   << (python_version == 2 ? python2_key++ : python3_key++)
	   << " || user_string($arg1) != module_name()) { next }";
      code << "}" << endl;

      probe *mark_probe = parse_synthetic_probe (sess, code, tok);
      if (!mark_probe)
	throw SEMANTIC_ERROR (_("can't create python call mark probe"),
			      tok);

      // Note we're operating on a copy of the base, because we might
      // need to do the expansion several times.
      probe *base_copy = new probe(base, pp);

      // Link this main probe back to the original base copy, with an
      // additional probe intermediate to catch probe listing.
      mark_probe->base = new probe(base_copy, pp);

      // Note that order *is* important here. We want to expand python
      // variable requests in the probe body first, then expand python
      // backtrace requests in the probe body. The latter uses '$arg3'
      // as the python frame pointer, and we don't want the python
      // variable exander to find those maker argument references.
      python_var_expanding_visitor pvev (sess, python_version);
      var_expand_const_fold_loop (sess, base_copy->body, pvev);
      
      python_functioncall_expanding_visitor v (sess, python_version);
      v.replace (base_copy->body);

      // Splice base_copy->body in after the parsed body
      mark_probe->body = new block(mark_probe->body, base_copy->body);
      derive_probes(sess, mark_probe, finished_results);

      // Create a python_derived_probe, but don't return it in
      // 'finished_results'. Instead just add it to the
      // group, so that its information will be added to the
      // procfs file.
      probe* new_probe = new probe (base, pp);
      python_derived_probe *pdp;
      pdp = new python_derived_probe(sess, new_probe, pp, python_version,
				     (*iter)->module, (*iter)->function,
				     has_return, has_call);
      pdp->join_group(sess);

      // Delete the item and go on to the next item in the vector.
      delete *iter;
      iter = results.erase(iter);
    }
}


void
register_tapset_python(systemtap_session& s)
{
#if defined(HAVE_PYTHON2_PROBES) || defined(HAVE_PYTHON3_PROBES)
  match_node* root = s.pattern_root;
  derived_probe_builder *builder = new python_builder();

  vector<match_node*> roots;
#if defined(HAVE_PYTHON2_PROBES)
  roots.push_back(root->bind(TOK_PYTHON2));
  //roots.push_back(root->bind_num(TOK_PYTHON2));
#endif
#if defined(HAVE_PYTHON3_PROBES)
  roots.push_back(root->bind(TOK_PYTHON3));
  //roots.push_back(root->bind_num(TOK_PYTHON3));
#endif

  for (unsigned i = 0; i < roots.size(); ++i)
    {
      roots[i]->bind_str(TOK_MODULE)->bind_str(TOK_FUNCTION)
	->bind_privilege(pr_all)
	->bind(builder);
      roots[i]->bind_str(TOK_MODULE)->bind_str(TOK_FUNCTION)->bind(TOK_CALL)
	->bind_privilege(pr_all)
	->bind(builder);
      roots[i]->bind_str(TOK_MODULE)->bind_str(TOK_FUNCTION)->bind(TOK_RETURN)
	->bind_privilege(pr_all)
	->bind(builder);
    }
#else
  (void) s;
#endif
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
