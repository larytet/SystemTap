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

    python_probe_info (interned_string m, interned_string f)
	: module(m), function(f) {}
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
  void emit_kernel_module_init (systemtap_session& s);
  void emit_kernel_module_exit (systemtap_session& s);
  void emit_module_decls (systemtap_session& s);
  void emit_module_init (systemtap_session& s);
  void emit_module_exit (systemtap_session& s);
};


struct python_builder: public derived_probe_builder
{
private:
  int resolve(systemtap_session& s,
	      const unsigned python_ver,
	      interned_string module,
	      interned_string function,
	      vector<python_probe_info *> &results);
  bool python2_procfs_probe_derived_p;
  bool python3_procfs_probe_derived_p;
  probe* python2_call_probe;
  probe* python2_line_probe;
  probe* python2_return_probe;
  probe* python3_call_probe;
  probe* python3_line_probe;
  probe* python3_return_probe;

public:
  python_builder() : python2_procfs_probe_derived_p(false),
		     python3_procfs_probe_derived_p(false),
		     python2_call_probe(NULL),
		     python2_line_probe(NULL),
		     python2_return_probe(NULL),
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
// FIXME: Hmm, we'll need to handle python probes by putting uprobes
// on our custom python module's tracepoints.
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
python_derived_probe_group::emit_kernel_module_init (systemtap_session& s)
{
  if (python2_probes.empty() && python3_probes.empty())
    return;
  // Note that we're OK here, since _stp_mkdir_proc_module() can be
  // called twice without issue (in case the script also contains real
  // procfs probes).
  s.op->newline() << "rc = _stp_mkdir_proc_module();";
}


void
python_derived_probe_group::emit_kernel_module_exit (systemtap_session& s)
{
  if (python2_probes.empty() && python3_probes.empty())
    return;

  // If we're using the original transport, it uses the
  // '/proc/systemtap/{module_name}' directory to store control
  // files. Let the transport layer clean up that directory.
  s.op->newline() << "#if (STP_TRANSPORT_VERSION != 1)";
  s.op->newline() << "_stp_rmdir_proc_module();";
  s.op->newline() << "#endif";
}


void
python_derived_probe_group::emit_module_decls (systemtap_session& s)
{
  if (python2_probes.empty() && python3_probes.empty())
    return;

  s.op->newline();
  s.op->newline() << "/* ---- python probes ---- */";
  // We're reusing the procfs probe buffer machinery.
  s.op->newline() << "#include \"procfs.c\"";
  s.op->newline() << "#include \"procfs-probes.c\"";

  // The procfs probe buffer machinery emits 2 functions. We're not
  // going to use them, but runtime/procfs-probes.c wants a definition
  // of them. So, if we don't have any procfs probes, we'll have to
  // emit those functions ourselves.
  if (!s.runtime_usermode_p() && s.procfs_derived_probes == NULL)
    {
      s.op->newline() << "static int _stp_proc_fill_read_buffer(struct stap_procfs_probe *spp) {";
      s.op->indent(1);
      s.op->newline() << "return 0;";
      s.op->newline(-1) << "}";
      s.op->newline() << "static int _stp_process_write_buffer(struct stap_procfs_probe *spp, const char __user *buf, size_t count) {";
      s.op->indent(1);
      s.op->newline() << "return 0;";
      s.op->newline(-1) << "}";
    }  

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

  // Output the procfs file information.
  s.op->newline() << "static struct stap_procfs_probe stap_python_procfs_probes[] = {";
  s.op->indent(1);
  if (python2_probes.size())
    {
      s.op->newline() << "{";
      s.op->line() << " .path=\"_stp_python2_probes\",";
      s.op->line() << " .buffer=(char *)python2_probe_info,";
      s.op->line() << " .bufsize=sizeof(python2_probe_info),";
      s.op->line() << " .count=(sizeof(python2_probe_info) - 1),";
      s.op->line() << " .permissions=0444,";
      s.op->line() << " },";
    }
  if (python3_probes.size())
    {
      s.op->newline() << "{";
      s.op->line() << " .path=\"_stp_python3_probes\",";
      s.op->line() << " .buffer=(char *)python3_probe_info,";
      s.op->line() << " .bufsize=(sizeof(python3_probe_info) - 1),";
      s.op->line() << " .count=(sizeof(python3_probe_info) - 1),";
      s.op->line() << " .permissions=0444,";
      s.op->line() << " },";
    }
  s.op->newline(-1) << "};";
}


void
python_derived_probe_group::emit_module_init (systemtap_session& s)
{
  if (python2_probes.empty() && python3_probes.empty())
    return;

  if (!s.runtime_usermode_p())
    {
      // FIXME: We've got a problem here. We need to add code
      // somewhere so that the user can't create procfs probes
      // with the same filenames. (If he did, we'd just error out
      // at startup I believe, so it isn't a real big problem.)
      s.op->newline() << "for (i = 0; i < ARRAY_SIZE(stap_python_procfs_probes); i++) {";
      s.op->newline(1) << "struct stap_procfs_probe *spp = &stap_python_procfs_probes[i];";

      // FIXME: This really isn't right, but it is the best we have at
      // the moment.
      s.op->newline() << "probe_point = spp->path;";

      s.op->newline() << "_spp_init(spp);";
      s.op->newline() << "rc = _stp_create_procfs(spp->path, &_stp_proc_fops, spp->permissions, spp);";
      s.op->newline() << "if (rc) {";
      s.op->newline(1) << "_stp_close_procfs();";

      // FIXME: The problem here is that if there are any "real"
      // procfs probes that have already been created,
      // _stp_close_procfs() above will clean them up, but
      // _spp_shutdown() won't get called. Now that isn't the end of
      // the world, since _spp_shutdown() just calls mutex_destroy()
      // which doesn't do any deallocation.
      s.op->newline() << "for (i = 0; i < ARRAY_SIZE(stap_python_procfs_probes); i++) {";
      s.op->newline(1) << "spp = &stap_python_procfs_probes[i];";
      s.op->newline() << "_spp_shutdown(spp);";
      s.op->newline(-1) << "}";
      s.op->newline() << "break;";
      s.op->newline(-1) << "}";
      s.op->newline(-1) << "}"; // for loop
    }
}


void
python_derived_probe_group::emit_module_exit (systemtap_session& s)
{
  if (python2_probes.empty() && python3_probes.empty())
    return;

  if (!s.runtime_usermode_p())
    {
      s.op->newline() << "_stp_close_procfs();";
      s.op->newline() << "for (i = 0; i < ARRAY_SIZE(stap_python_procfs_probes); i++) {";
      s.op->newline(1) << "struct stap_procfs_probe *spp = &stap_python_procfs_probes[i];";
      s.op->newline() << "_spp_shutdown(spp);";
      s.op->newline(-1) << "}";
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
  bool has_call = has_null_param (parameters, TOK_CALL);

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
							  has_return,
							  has_call));

      if (python_version == 2)
        {
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

  // FIXME: We'd really like to just derive some synthetic procfs
  // probes here and then modify a couple of the fields in the new
  // procfs_derived_probe, but we really can't do that easily. It
  // might be possible to create a new virtual function in
  // derived_probe (which procfs_derived_probe inherits from), that
  // the procfs_derived_probe class would implement to do the
  // modification for us.
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
