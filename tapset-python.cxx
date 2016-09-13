// tapset for python
// Copyright (C) 2016 Red Hat Inc.
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

static const string TOK_PYTHON("python");
static const string TOK_PYTHON3("python3");
static const string TOK_MODULE("module");
static const string TOK_FUNCTION("function");
static const string TOK_RETURN("return");


// ------------------------------------------------------------------------
// python derived probes
// ------------------------------------------------------------------------


struct python_probe_info
{
    interned_string module;
    interned_string function;

    python_probe_info (interned_string m, interned_string f)
	: module(m), function(f) {}
};


struct python_derived_probe: public derived_probe
{
  int python_version;
  interned_string module;
  interned_string function;
  bool has_return;

  python_derived_probe (systemtap_session &, probe* p, probe_point* l,
			int python_version, interned_string module,
			interned_string function, bool has_return);
  void join_group (systemtap_session& s);
};


struct python_derived_probe_group: public generic_dpg<python_derived_probe>
{
private:
  vector<python_derived_probe* > probes;

public:
  python_derived_probe_group () {}

  void enroll (python_derived_probe* probe);
  void emit_kernel_module_init (systemtap_session& ) {
    throw SEMANTIC_ERROR (_("python probe code generation isn't implemented yet"));
  }
  void emit_kernel_module_exit (systemtap_session& ) {
    throw SEMANTIC_ERROR (_("python probe code generation isn't implemented yet"));
  }
  void emit_module_decls (systemtap_session& ) {
    throw SEMANTIC_ERROR (_("python probe code generation isn't implemented yet"));
  }
  void emit_module_init (systemtap_session& ) {
    throw SEMANTIC_ERROR (_("python probe code generation isn't implemented yet"));
  }
  void emit_module_exit (systemtap_session& ) {
    throw SEMANTIC_ERROR (_("python probe code generation isn't implemented yet"));
  }
};


struct python_builder: public derived_probe_builder
{
private:
  int resolve(systemtap_session& s,
	      const unsigned python_ver,
	      interned_string module,
	      interned_string function,
	      vector<python_probe_info *> &results);

public:
  python_builder() {}

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
					    bool hr):
  derived_probe (p, l, true /* .components soon rewritten */ ),
  python_version(pv), module(m), function(f), has_return(hr)
{
  return;
}


void
python_derived_probe::join_group (systemtap_session &s)
{
// FIXME: Hmm, we'll need to handle python probes by putting uprobes
// on our custom python module's tracepoints.
  if (! s.python_derived_probes)
    {
      s.python_derived_probes = new python_derived_probe_group ();
    }
  s.python_derived_probes->enroll (this);
  this->group = s.python_derived_probes;
}


void
python_derived_probe_group::enroll (python_derived_probe* p)
{
  probes.push_back(p);
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
      args.push_back("python");
  else
      args.push_back("python3");
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
  // FUNCTION'
  stdio_filebuf<char> buf(child_out, ios_base::in);
  istream in(&buf);
  string line;
  while (getline(in, line))
    {
      size_t space_pos;
      interned_string module;
      interned_string function;

      if (line.empty())
	continue;
      space_pos = line.find(" ");
      if (space_pos == string::npos)
	continue;
	  
      module = line.substr(0, space_pos);
      function = line.substr(space_pos + 1);
      results.push_back(new python_probe_info(module, function));
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
  unsigned python_version = has_null_param (parameters, TOK_PYTHON) ? 2 : 3;
  bool has_module = get_param (parameters, TOK_MODULE, module);
  bool has_function = get_param (parameters, TOK_FUNCTION, function);
  bool has_return = has_null_param (parameters, TOK_RETURN);

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
      assert_no_interrupts();
      assert (location->components.size() >= 3);
      assert (location->components[1]->functor == TOK_MODULE);
      assert (location->components[2]->functor == TOK_FUNCTION);

      // Create a new probe point location.
      probe_point *pp = new probe_point (*location);

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

      probe* new_probe = new probe (base, pp);
      finished_results.push_back(new python_derived_probe(sess, new_probe, pp,
							  python_version,
							  (*iter)->module,
							  (*iter)->function,
							  has_return));

      // Delete the item and go on to the next item in the vector.
      delete *iter;
      iter = results.erase(iter);
    }
}


void
register_tapset_python(systemtap_session& s)
{
#if defined(HAVE_PYTHON2) || defined(HAVE_PYTHON3)
  match_node* root = s.pattern_root;
  derived_probe_builder *builder = new python_builder();

  vector<match_node*> roots;
#if defined(HAVE_PYTHON2)
  roots.push_back(root->bind(TOK_PYTHON));
  //roots.push_back(root->bind_num(TOK_PYTHON));
#endif
#if defined(HAVE_PYTHON3)
  roots.push_back(root->bind(TOK_PYTHON3));
  //roots.push_back(root->bind_num(TOK_PYTHON3));
#endif

  for (unsigned i = 0; i < roots.size(); ++i)
    {
      roots[i]->bind_str(TOK_MODULE)->bind_str(TOK_FUNCTION)
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
