// Tapset for per-method based probes
// Copyright (C) 2014-2016 Red Hat Inc.

// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "session.h"
#include "tapsets.h"
#include "translate.h"
#include "util.h"
#include "config.h"
#include "staptree.h"

#include "unistd.h"
#include "sys/wait.h"
#include "sys/types.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>

extern "C" {
#include <fnmatch.h>
}

using namespace std;
using namespace __gnu_cxx;

static const string TOK_CLASS ("class");
static const string TOK_METHOD ("method");
static const string TOK_PROCESS ("process");
static const string TOK_PROVIDER ("provider");
static const string TOK_MARK ("mark");
static const string TOK_JAVA ("java");
static const string TOK_RETURN ("return");
static const string TOK_BEGIN ("begin");
static const string TOK_END ("end");
static const string TOK_ERROR ("error");

// --------------------------------------------------------------------------

struct java_details_inspection: public functioncall_traversing_visitor
{

  bool java_backtrace;
  java_details_inspection(): java_backtrace(false) {}

  void visit_functioncall(functioncall* e);
};

void
java_details_inspection::visit_functioncall(functioncall* e)
{
  assert(e->referents.empty()); // we haven't elborated yet, so there should never be referents, bail if here is
  if (e->function == "sprint_java_backtrace" || e->function == "print_java_backtrace" ){
    java_backtrace = true;
    return; // no need to search anymore we know we'll need the extra information
  }
  traversing_visitor::visit_functioncall(e);
}

struct java_builder: public derived_probe_builder
{
private:
  typedef multimap<string, string> java_cache_t;
  typedef multimap<string, string>::const_iterator java_cache_const_iterator_t;
  typedef pair<java_cache_const_iterator_t, java_cache_const_iterator_t>
    java_cache_const_iterator_pair_t;
  java_cache_t java_cache;

public:
  java_builder () {}

  void build (systemtap_session & sess,
	      probe * base,
	      probe_point * location,
	      literal_map_t const & parameters,
	      vector <derived_probe *> & finished_results);

  virtual string name() { return "java builder"; }
};

void
java_builder::build (systemtap_session & sess,
		     probe * base,
		     probe_point * loc,
		     literal_map_t const & parameters,
		     vector <derived_probe *> & finished_results)
{
  interned_string method_str_val;
  interned_string method_line_val;
  bool has_method_str = get_param (parameters, TOK_METHOD, method_str_val);
  int short_method_pos = method_str_val.find ('(');
  //only if it exists, run check
  bool one_arg = false; // used to check if there is an argument in the method
  if (short_method_pos)
    {
      int second_method_pos = 0;
      second_method_pos = method_str_val.find (')');
      if ((second_method_pos - short_method_pos) > 1)
	one_arg = true;
    }
  int64_t _java_pid = 0;
  interned_string _java_proc_class = "";
  // interned_string short_method_str = method_str_val.substr (0, short_method_pos);
  interned_string class_str_val; // fully qualified class string
  bool has_class_str = get_param (parameters, TOK_CLASS, class_str_val);
  bool has_pid_int = get_param (parameters, TOK_JAVA, _java_pid);
  bool has_pid_str = get_param (parameters, TOK_JAVA, _java_proc_class);
  bool has_return = has_null_param (parameters, TOK_RETURN);
  bool has_line_number = false;

  // wildcards in Java probes are not allowed, so the location is already
  // well-formed
  loc->well_formed = true;

  //find if we're probing at a specific line number
  size_t line_position = 0;

  size_t method_end_pos = method_str_val.size();
  line_position = method_str_val.find_first_of(":"); //this will return the position ':' is found at
  if (line_position == string::npos)
    has_line_number = false;
  else
    {
      has_line_number = true;
      method_line_val = method_str_val.substr(line_position+1, method_end_pos);
      method_str_val = method_str_val.substr(0, line_position);
      line_position = method_line_val.find_first_of(":");
      if (line_position != string::npos)
        throw SEMANTIC_ERROR (_("maximum of one line number (:NNN)"));
      if (has_line_number && has_return)
        throw SEMANTIC_ERROR (_("conflict :NNN and .return probe"));
    }

  //need to count the number of parameters, exit if more than 10

  int method_params_count = count (method_str_val.begin (), method_str_val.end (), ',');
  if (one_arg)
    method_params_count++; // in this case we know there was at least a var, but no ','

  if (method_params_count > 10)
    throw SEMANTIC_ERROR (_("maximum of 10 java method parameters may be specified"));

  assert (has_method_str);
  (void) has_method_str;
  assert (has_class_str);
  (void) has_class_str;

  interned_string java_pid_str = "";
  if(has_pid_int)
    java_pid_str = lex_cast(_java_pid);
  else
    java_pid_str = _java_proc_class;

  if (! (has_pid_int || has_pid_str) )
    throw SEMANTIC_ERROR (_("missing JVMID"));

  /* Java native backtrace probe point

     In the event a java backtrace is requested (signaled by a '1' returned by the
     _bt() stap function and appended to the stapbm call),  we need to place a
     probe point on the method__bt marker in the libHelperSDT_*.so .  We've created
     this new marker as we don't want it interfering with the method__X markers of
     the same rulename.  The overall flow of backtraces are the same as 'regular'
     method probes, however run through STAP_BACKTRACE helper method, and
     METHOD_STAP_BT native method in turn.

     STAP_BACKTRACE also converts the throwable object to a string for us to pass/report

     The end result is we need to place another probe point automatically for the user;
     process("$pkglibdir/libHelperSDT_*.so").provider("HelperSDT").mark("method__bt")
     and pass the backtrace string to the java_backtrace_string variable, which then gets
     immediately passed to the subsequent mark("method_XX") probe through the java_backtrace()
     function's return value.
   */

  struct java_details_inspection jdi;
  base->body->visit(&jdi);

  // the wildcard is deliberate to catch all architectures
  string libhelper = string(PKGLIBDIR) + "/libHelperSDT_*.so";
  string rule_name = "module_name() . " + lex_cast_qstring(base->name());
  const token* tok = base->body->tok;

  if (jdi.java_backtrace)
    {
      stringstream bt_code;
      bt_code << "probe process(" << literal_string(libhelper) << ")"
              << ".provider(\"HelperSDT\").mark(\"method__bt\") {" << endl;

      // Make sure the rule name in the last arg matches this probe
      bt_code << "if (user_string($arg3) != " << rule_name << ") next;" << endl;

      // $arg1 is the backtrace string, $arg2 is the stack depth
      bt_code << "__assign_stacktrace($arg1, $arg2);" << endl;

      bt_code << "}" << endl; // End of probe

      probe* new_mark_bt_probe = parse_synthetic_probe (sess, bt_code, tok);
      if (!new_mark_bt_probe)
        throw SEMANTIC_ERROR (_("can't create java backtrace probe"), tok);
      derive_probes(sess, new_mark_bt_probe, finished_results);


      // Now to delete the backtrace string
      stringstream btd_code;
      btd_code << "probe process(" << literal_string(libhelper) << ")"
               << ".provider(\"HelperSDT\").mark(\"method__bt__delete\") {" << endl;

      // make sure the rule name in the last arg matches this probe
      btd_code << "if (user_string($arg1) != " << rule_name << ") next;" << endl;

      btd_code << "__delete_backtrace();" << endl;

      btd_code << "}" << endl; // End of probe

      probe* new_mark_btd_probe = parse_synthetic_probe (sess, btd_code, tok);
      if (!new_mark_btd_probe)
        throw SEMANTIC_ERROR (_("can't create java backtrace delete probe"), tok);
      derive_probes(sess, new_mark_btd_probe, finished_results);
    }

  // PR21020 - support both java<->stap abis
  string stap31 = (strverscmp(sess.compatible.c_str(), "3.1") >= 0) ? "31" : "";
  
  /* The overall flow of control during a probed java method is something like this:

     (java) java-method ->
     (java) byteman ->
     (java) HelperSDT::METHOD_STAP*_PROBENN ->
     (JNI) HelperSDT_arch.so ->
     (C) sys/sdt.h marker STAP_PROBEN(hotspot,method__N,...,rulename)

     To catch the java-method hit that belongs to this very systemtap
     probe, we use the rulename string as the identifier.  It has to have
     some cool properties:
     - be unique system-wide, so as to avoid collisions between concurrent users, even if
       they run the same stap script
     - be unique within the script, so distinct probe handlers get run if specified
     - be computable from systemtap at run-time (since compile-time can't be unique enough)
     - be passable to stapbm, back through a .btm (byteman rule) file, back through sdt.h parameters

     The rulename is thusly synthesized as the string-concatenation expression
            (module_name() . "probe_NNN")
  */

  stringstream code;
  code << "probe process(" << literal_string(libhelper) << ")" << ".provider(\"HelperSDT\")"
       << ".mark(" << literal_string (string("method")+stap31+"__"+lex_cast(method_params_count)) << ") {" << endl;


  // Make sure the rule name in the last arg matches this probe
  code << "if (user_string($arg" << (method_params_count+1)
       << ") != " << rule_name << ") next;" << endl;

  // add the implicit user_string_warn()s for conversion
  if (stap31 == "31")
    for (int i=0; i<method_params_count; i++)
      code << "arg" << i+1 << " = user_string_warn($arg" << i+1 << ");" << endl;

  code << "}" << endl; // End of probe

  probe* new_mark_probe = parse_synthetic_probe (sess, code, tok);
  if (!new_mark_probe)
    throw SEMANTIC_ERROR (_("can't create java method probe"), tok);

  // Link this main probe back to the original base, with an
  // additional probe intermediate to catch probe listing.
  new_mark_probe->base = new probe(base, loc);

  // Splice base->body in after the parsed body
  new_mark_probe->body = new block (new_mark_probe->body, base->body);

  derive_probes (sess, new_mark_probe, finished_results);


  // the begin portion of the probe to install byteman rules in the target jvm
  stringstream begin_code;
  begin_code << "probe begin {" << endl;

  /* stapbm takes the following arguments:
     $1 - install/uninstall {,31}
     $2 - JVM PID/unique name
     $3 - RULE name  <--- identifies this probe uniquely at run time
     $4 - class
     $5 - method
     $6 - number of args
     $7 - entry/exit/line
     $8 - backtrace
  */

  string leftbits = string(PKGLIBDIR) + "/stapbm install"+stap31+" " +
    lex_cast_qstring(has_pid_int ? java_pid_str : _java_proc_class) + " ";

  string rightbits = " " + lex_cast_qstring(class_str_val) +
    " " + lex_cast_qstring(method_str_val) +
    " " + lex_cast(method_params_count) +
    " " + ((!has_return && !has_line_number) ? string("entry") :
           ((has_return && !has_line_number) ? string("exit") :
            (string)method_line_val)) +
    " " + (jdi.java_backtrace ? string("1") : string("0"));

  begin_code << "system(" << literal_string(leftbits) << " . " << rule_name
             << " . " << literal_string(rightbits) << ");" << endl;

  begin_code << "}" << endl; // End of probe

  probe* new_begin_probe = parse_synthetic_probe (sess, begin_code, tok);
  if (!new_begin_probe)
    throw SEMANTIC_ERROR (_("can't create java begin probe"), tok);
  derive_probes (sess, new_begin_probe, finished_results);


  // the end/error portion of the probe to uninstall byteman rules from the target jvm
  stringstream end_code;
  end_code << "probe end, error {" << endl;

  leftbits = string(PKGLIBDIR) + "/stapbm uninstall"+stap31+" " +
    lex_cast_qstring(has_pid_int ? java_pid_str : _java_proc_class) + " ";
  // rightbits are the same as the begin probe

  end_code << "system(" << literal_string(leftbits) << " . " << rule_name
             << " . " << literal_string(rightbits) << ");" << endl;

  end_code << "}" << endl; // End of probe

  probe* new_end_probe = parse_synthetic_probe (sess, end_code, tok);
  if (!new_end_probe)
    throw SEMANTIC_ERROR (_("can't create java end probe"), tok);
  derive_probes (sess, new_end_probe, finished_results);
}

void
register_tapset_java (systemtap_session& s)
{
  (void) s;

#ifdef HAVE_JAVA
  match_node* root = s.pattern_root;
  derived_probe_builder *builder = new java_builder ();

  root->bind_str (TOK_JAVA)
    ->bind_str (TOK_CLASS)->bind_str (TOK_METHOD)
    ->bind_privilege(pr_all)
    ->bind(builder);

  root->bind_str (TOK_JAVA)
    ->bind_str (TOK_CLASS)->bind_str (TOK_METHOD)
    ->bind (TOK_RETURN)
    ->bind_privilege(pr_all)
    ->bind(builder);

  root->bind_num (TOK_JAVA)
    ->bind_str (TOK_CLASS)->bind_str (TOK_METHOD)
    ->bind_privilege(pr_all)
    ->bind (builder);

  root->bind_num (TOK_JAVA)
    ->bind_str (TOK_CLASS)->bind_str (TOK_METHOD)
    ->bind (TOK_RETURN)
    ->bind_privilege(pr_all)
    ->bind (builder);
#endif
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
