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
  vector<python_derived_probe* > python2_probes;
  vector<python_derived_probe* > python3_probes;

public:
  python_derived_probe_group () {}

  void enroll (python_derived_probe* probe);
  void emit_module_decls (systemtap_session& s);
  void emit_module_init (systemtap_session& ) { }
  void emit_module_exit (systemtap_session& ) { }
};


struct python_builder: public derived_probe_builder
{
private:
  int resolve(systemtap_session& s,
	      const unsigned python_ver,
	      interned_string module,
	      interned_string function,
	      vector<python_probe_info *> &results);

  // python2 related synthetic probes
  derived_probe* python2_procfs_probe;
  probe* python2_call_probe;
  probe* python2_line_probe;
  probe* python2_return_probe;

  // python3 related synthetic probes
  derived_probe* python3_procfs_probe;
  probe* python3_call_probe;
  probe* python3_line_probe;
  probe* python3_return_probe;

public:
  python_builder() : python2_procfs_probe(NULL),
		     python2_call_probe(NULL),
		     python2_line_probe(NULL),
		     python2_return_probe(NULL),
		     python3_procfs_probe(NULL),
		     python3_call_probe(NULL),
		     python3_line_probe(NULL),
		     python3_return_probe(NULL) {}

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
      s.python_derived_probes = new python_derived_probe_group ();
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
    python2_probes.push_back(p);
  else if (p->python_version == 3)
    python3_probes.push_back(p);
  else
    throw SEMANTIC_ERROR(_F("Unknown python version: %d", p->python_version));
}


void
python_derived_probe_group::emit_module_decls (systemtap_session& s)
{
  if (python2_probes.empty() && python3_probes.empty())
    return;

  s.op->newline();
  s.op->newline() << "/* ---- python probes ---- */";

  // Output the probe info buffer.
  if (python2_probes.size())
    {
      s.op->newline() << "static const char python2_probe_info[] =";
      s.op->indent(1);
      for (auto iter = python2_probes.begin(); iter != python2_probes.end();
	   iter++)
        {
	  s.op->newline() << "\"b " << (*iter)->break_definition() << "\\n\"";
	}
      s.op->line() << ";";
      s.op->indent(-1);
    }
  if (python3_probes.size())
    {
      s.op->newline() << "static const char python3_probe_info[] =";
      s.op->indent(1);
      for (auto iter = python3_probes.begin(); iter != python3_probes.end();
	   iter++)
        {
	  s.op->newline() << "\"b " << (*iter)->break_definition() << "\\n\"";
	}
      s.op->line() << ";";
      s.op->indent(-1);
    }
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
  unsigned python_version = has_null_param (parameters, TOK_PYTHON) ? 2 : 3;
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

      probe* new_probe = new probe (base, pp);
      finished_results.push_back(new python_derived_probe(sess, new_probe, pp,
							  python_version,
							  (*iter)->module,
							  (*iter)->function,
							  has_return,
							  has_call));

      if (python_version == 2)
        {
	  // Create (if necessary) the python 2 procfs probe which the
	  // HelperSDT python module reads to get probe information.
	  if (python2_procfs_probe == NULL)
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
		  throw SEMANTIC_ERROR (_F("wrong number of probes derived (%d), should be 1",
					   results.size()));
	      python2_procfs_probe = results[0];
	      finished_results.push_back(results[0]);
	      
	      // Now that we've got our procfs derived probe, point it
	      // at our internal buffer that we're going to output in
	      // python_derived_probe_group::emit_module_decls().
	      python2_procfs_probe->use_internal_buffer("python2_probe_info");
	  }

	  if (has_return && python2_return_probe == NULL)
	    {
	      stringstream code;
	      const token* tok = base->body->tok;
	      code << "probe process(\"" << PYTHON_BASENAME
		   << "\").library(\"" << PYEXECDIR
		   << "/HelperSDT/_HelperSDT.so\")"
		   << ".provider(\"HelperSDT\").mark(\"PyTrace_RETURN\") {"
		   << endl;
	      // FIXME: Placeholder...
	      code << "  printf(\"PyTrace_RETURN\")" << endl;
	      code << "}" << endl;
	      python2_return_probe = parse_synthetic_probe (sess, code, tok);
	      if (!python2_return_probe)
		throw SEMANTIC_ERROR (_("can't create python2 return probe"),
				      tok);
	      derive_probes(sess, python2_return_probe, finished_results);
	    }
	  else if (has_call && python2_call_probe == NULL)
	    {
	      stringstream code;
	      const token* tok = base->body->tok;
	      code << "probe process(\"" << PYTHON_BASENAME
		   << "\").library(\"" << PYEXECDIR
		   << "/HelperSDT/_HelperSDT.so\")"
		   << ".provider(\"HelperSDT\").mark(\"PyTrace_CALL\") {"
		   << endl;
	      // FIXME: Placeholder...
	      code << "  printf(\"PyTrace_CALL\")" << endl;
	      code << "}" << endl;
	      python2_call_probe = parse_synthetic_probe (sess, code, tok);
	      if (!python2_call_probe)
		throw SEMANTIC_ERROR (_("can't create python2 return probe"),
				      tok);
	      derive_probes(sess, python2_call_probe, finished_results);
	    }
	  else if (python2_line_probe == NULL)
	    {
	      stringstream code;
	      const token* tok = base->body->tok;
	      code << "probe process(\"" << PYTHON_BASENAME
		   << "\").library(\"" << PYEXECDIR
		   << "/HelperSDT/_HelperSDT.so\")"
		   << ".provider(\"HelperSDT\").mark(\"PyTrace_LINE\") {"
		   << endl;
	      // FIXME: Placeholder...
	      code << "  printf(\"PyTrace_LINE\")" << endl;
	      code << "}" << endl;
	      python2_line_probe = parse_synthetic_probe (sess, code, tok);
	      if (!python2_line_probe)
		throw SEMANTIC_ERROR (_("can't create python2 return probe"),
				      tok);
	      derive_probes(sess, python2_line_probe, finished_results);
	    }
	}
      else
        {
	  // Create (if necessary) the python 3 procfs probe which the
	  // HelperSDT python module reads to get probe information.
	  if (python3_procfs_probe == NULL)
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
		  throw SEMANTIC_ERROR (_F("wrong number of probes derived (%d), should be 1",
					   results.size()));
	      python3_procfs_probe = results[0];
	      finished_results.push_back(results[0]);
	      
	      // Now that we've got our procfs derived probe, point it
	      // at our internal buffer that we're going to output in
	      // python_derived_probe_group::emit_module_decls().
	      python3_procfs_probe->use_internal_buffer("python3_probe_info");
	  }

	  if (has_return && python3_return_probe == NULL)
	    {
	      stringstream code;
	      const token* tok = base->body->tok;
	      code << "probe process(\"" << PYTHON3_BASENAME
		   << "\").library(\"" << PY3EXECDIR
		   << "/HelperSDT/_HelperSDT.so\")"
		   << ".provider(\"HelperSDT\").mark(\"PyTrace_RETURN\") {"
		   << endl;
	      // FIXME: Placeholder...
	      code << "  printf(\"PyTrace_RETURN\")" << endl;
	      code << "}" << endl;
	      python3_return_probe = parse_synthetic_probe (sess, code, tok);
	      if (!python3_return_probe)
		throw SEMANTIC_ERROR (_("can't create python3 return probe"),
				      tok);
	      derive_probes(sess, python3_return_probe, finished_results);
	    }
	  else if (has_call && python3_call_probe == NULL)
	    {
	      stringstream code;
	      const token* tok = base->body->tok;
	      code << "probe process(\"" << PYTHON3_BASENAME
		   << "\").library(\"" << PY3EXECDIR
		   << "/HelperSDT/_HelperSDT.so\")"
		   << ".provider(\"HelperSDT\").mark(\"PyTrace_CALL\") {"
		   << endl;
	      // FIXME: Placeholder...
	      code << "  printf(\"PyTrace_CALL\")" << endl;
	      code << "}" << endl;
	      python3_call_probe = parse_synthetic_probe (sess, code, tok);
	      if (!python3_call_probe)
		throw SEMANTIC_ERROR (_("can't create python3 return probe"),
				      tok);
	      derive_probes(sess, python3_call_probe, finished_results);
	    }
	  else if (python3_line_probe == NULL)
	    {
	      stringstream code;
	      const token* tok = base->body->tok;
	      code << "probe process(\"" << PYTHON3_BASENAME
		   << "\").library(\"" << PY3EXECDIR
		   << "/HelperSDT/_HelperSDT.so\")"
		   << ".provider(\"HelperSDT\").mark(\"PyTrace_LINE\") {"
		   << endl;
	      // FIXME: Placeholder...
	      code << "  printf(\"PyTrace_LINE\")" << endl;
	      code << "}" << endl;
	      python3_line_probe = parse_synthetic_probe (sess, code, tok);
	      if (!python3_line_probe)
		throw SEMANTIC_ERROR (_("can't create python3 return probe"),
				      tok);
	      derive_probes(sess, python3_line_probe, finished_results);
	    }
	}

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
  roots.push_back(root->bind(TOK_PYTHON));
  //roots.push_back(root->bind_num(TOK_PYTHON));
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
