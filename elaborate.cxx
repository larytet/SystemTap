// elaboration functions
// Copyright (C) 2005-2016 Red Hat Inc.
// Copyright (C) 2008 Intel Corporation
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include "elaborate.h"
#include "translate.h"
#include "parse.h"
#include "tapsets.h"
#include "session.h"
#include "util.h"
#include "task_finder.h"
#include "stapregex.h"
#include "stringtable.h"

extern "C" {
#include <sys/utsname.h>
#include <fnmatch.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
}

#include <algorithm>
#include <fstream>
#include <map>
#include <cassert>
#include <set>
#include <vector>
#include <algorithm>
#include <iterator>
#include <climits>


using namespace std;


// ------------------------------------------------------------------------

// Used in probe_point condition construction.  Either argument may be
// NULL; if both, return NULL too.  Resulting expression is a deep
// copy for symbol resolution purposes.
expression* add_condition (expression* a, expression* b)
{
  if (!a && !b) return 0;
  if (! a) return deep_copy_visitor::deep_copy(b);
  if (! b) return deep_copy_visitor::deep_copy(a);
  logical_and_expr la;
  la.op = "&&";
  la.left = a;
  la.right = b;
  la.tok = a->tok; // or could be b->tok
  return deep_copy_visitor::deep_copy(& la);
}

// ------------------------------------------------------------------------



derived_probe::derived_probe (probe *p, probe_point *l, bool rewrite_loc):
  base (p), base_pp(l), group(NULL), sdt_semaphore_addr(0),
  session_index((unsigned)-1)
{
  assert (p);
  this->tok = p->tok;
  this->privileged = p->privileged;
  this->body = deep_copy_visitor::deep_copy(p->body);

  assert (l);
  // make a copy for subclasses which want to rewrite the location
  if (rewrite_loc)
    l = new probe_point(*l);
  this->locations.push_back (l);
}


void
derived_probe::printsig (ostream& o) const
{
  probe::printsig (o);
  printsig_nested (o);
}

void
derived_probe::printsig_nested (ostream& o) const
{
  // We'd like to enclose the probe derivation chain in a /* */
  // comment delimiter.  But just printing /* base->printsig() */ is
  // not enough, since base might itself be a derived_probe.  So we,
  // er, "cleverly" encode our nesting state as a formatting flag for
  // the ostream.
  ios::fmtflags f = o.flags (ios::internal);
  if (f & ios::internal)
    {
      // already nested
      o << " <- ";
      base->printsig (o);
    }
  else
    {
      // outermost nesting
      o << " /* <- ";
      base->printsig (o);
      o << " */";
    }
  // restore flags
  (void) o.flags (f);
}


void
derived_probe::collect_derivation_chain (std::vector<probe*> &probes_list) const
{
  probes_list.push_back(const_cast<derived_probe*>(this));
  base->collect_derivation_chain(probes_list);
}


void
derived_probe::collect_derivation_pp_chain (std::vector<probe_point*> &pp_list) const
{
  pp_list.push_back(const_cast<probe_point*>(this->sole_location()));
  base->collect_derivation_pp_chain(pp_list);
}


string
derived_probe::derived_locations (bool firstFrom)
{
  ostringstream o;
  vector<probe_point*> reference_point;
  collect_derivation_pp_chain(reference_point);
  if (reference_point.size() > 0)
    for(unsigned i=1; i<reference_point.size(); ++i)
      {
        if (firstFrom || i>1)
          o << " from: ";
        o << reference_point[i]->str(false); // no ?,!,etc
      }
  return o.str();
}


probe_point*
derived_probe::sole_location () const
{
  if (locations.size() == 0 || locations.size() > 1)
    throw SEMANTIC_ERROR (_N("derived_probe with no locations",
                             "derived_probe with too many locations",
                             locations.size()), this->tok);
  else
    return locations[0];
}


probe_point*
derived_probe::script_location () const
{
  // This feeds function::pn() in the tapset, which is documented as the
  // script-level probe point expression, *after wildcard expansion*.
  vector<probe_point*> chain;
  collect_derivation_pp_chain (chain);

  // Go backwards until we hit the first well-formed probe point
  for (int i=chain.size()-1; i>=0; i--)
    if (chain[i]->well_formed)
      return chain[i];

  // If that didn't work, just fallback to -something-.
  return sole_location();
}


void
derived_probe::emit_privilege_assertion (translator_output* o)
{
  // Emit code which will cause compilation to fail if it is compiled in
  // unprivileged mode.
  o->newline() << "#if ! STP_PRIVILEGE_CONTAINS (STP_PRIVILEGE, STP_PR_STAPDEV) && \\";
  o->newline() << "    ! STP_PRIVILEGE_CONTAINS (STP_PRIVILEGE, STP_PR_STAPSYS)";
  o->newline() << "#error Internal Error: Probe ";
  probe::printsig (o->line());
  o->line()    << " generated in --unprivileged mode";
  o->newline() << "#endif";
}


void
derived_probe::emit_process_owner_assertion (translator_output* o)
{
  // Emit code which will abort should the current target not belong to the
  // user in unprivileged mode.
  o->newline() << "#if ! STP_PRIVILEGE_CONTAINS (STP_PRIVILEGE, STP_PR_STAPDEV) && \\";
  o->newline() << "    ! STP_PRIVILEGE_CONTAINS (STP_PRIVILEGE, STP_PR_STAPSYS)";
  o->newline(1)  << "if (! is_myproc ()) {";
  o->newline(1)  << "snprintf(c->error_buffer, sizeof(c->error_buffer),";
  o->newline()   << "         \"Internal Error: Process %d does not belong to user %d in probe %s in --unprivileged mode\",";
  o->newline()   << "         current->tgid, _stp_uid, c->probe_point);";
  o->newline()   << "c->last_error = c->error_buffer;";
  // NB: since this check occurs before probe locking, its exit should
  // not be a "goto out", which would attempt unlocking.
  o->newline()   << "return;";
  o->newline(-1) << "}";
  o->newline(-1) << "#endif";
}

void
derived_probe::print_dupe_stamp_unprivileged(ostream& o)
{
  o << _("unprivileged users: authorized") << endl;
}

void
derived_probe::print_dupe_stamp_unprivileged_process_owner(ostream& o)
{
  o << _("unprivileged users: authorized for process owner") << endl;
}

// ------------------------------------------------------------------------
// Members of derived_probe_builder

void
derived_probe_builder::build_with_suffix(systemtap_session &,
                                         probe *,
                                         probe_point *,
                                         literal_map_t const &,
                                         std::vector<derived_probe *> &,
                                         std::vector<probe_point::component *>
                                           const &) {
  // XXX perhaps build the probe if suffix is empty?
  // if (suffix.empty()) {
  //   build (sess, use, location, parameters, finished_results);
  //   return;
  // }
  throw SEMANTIC_ERROR (_("invalid suffix for probe"));
}

bool
derived_probe_builder::get_param (literal_map_t const & params,
                                  interned_string key,
                                  interned_string& value)
{
  literal_map_t::const_iterator i = params.find (key);
  if (i == params.end())
    return false;
  literal_string * ls = dynamic_cast<literal_string *>(i->second);
  if (!ls)
    return false;
  value = ls->value;
  return true;
}


bool
derived_probe_builder::get_param (literal_map_t const & params,
                                  interned_string key,
                                  int64_t& value)
{
  literal_map_t::const_iterator i = params.find (key);
  if (i == params.end())
    return false;
  if (i->second == NULL)
    return false;
  literal_number * ln = dynamic_cast<literal_number *>(i->second);
  if (!ln)
    return false;
  value = ln->value;
  return true;
}


bool
derived_probe_builder::has_null_param (literal_map_t const & params,
                                       interned_string key)
{
  literal_map_t::const_iterator i = params.find(key);
  return (i != params.end() && i->second == NULL);
}

bool
derived_probe_builder::has_param (literal_map_t const & params,
                                  interned_string key)
{
  return (params.find(key) != params.end());
}

// ------------------------------------------------------------------------
// Members of match_key.

match_key::match_key(interned_string n)
  : name(n),
    have_parameter(false),
    parameter_type(pe_unknown)
{
}

match_key::match_key(probe_point::component const & c)
  : name(c.functor),
    have_parameter(c.arg != NULL),
    parameter_type(c.arg ? c.arg->type : pe_unknown)
{
}

match_key &
match_key::with_number()
{
  have_parameter = true;
  parameter_type = pe_long;
  return *this;
}

match_key &
match_key::with_string()
{
  have_parameter = true;
  parameter_type = pe_string;
  return *this;
}

string
match_key::str() const
{
  string n = name;
  if (have_parameter)
    switch (parameter_type)
      {
      case pe_string: return n + "(string)";
      case pe_long: return n + "(number)";
      default: return n + "(...)";
      }
  return n;
}

bool
match_key::operator<(match_key const & other) const
{
  return ((name < other.name)

	  || (name == other.name
	      && have_parameter < other.have_parameter)

	  || (name == other.name
	      && have_parameter == other.have_parameter
	      && parameter_type < other.parameter_type));
}


// NB: these are only used in the probe point name components, where
// only "*" is permitted.
//
// Within module("bar"), function("foo"), process("baz") strings, real
// wildcards are permitted too. See also util.h:contains_glob_chars

static bool
isglob(interned_string str)
{
  return(str.find('*') != str.npos);
}

static bool
isdoubleglob(interned_string str)
{
  return(str.find("**") != str.npos);
}

bool
match_key::globmatch(match_key const & other) const
{
  const string & name_str = name;
  const string & other_str = other.name;

  return ((fnmatch(name_str.c_str(), other_str.c_str(), FNM_NOESCAPE) == 0)
	  && have_parameter == other.have_parameter
	  && parameter_type == other.parameter_type);
}

// ------------------------------------------------------------------------
// Members of match_node
// ------------------------------------------------------------------------

match_node::match_node() :
  privilege(privilege_t (pr_stapdev | pr_stapsys))
{
}

match_node *
match_node::bind(match_key const & k)
{
  if (k.name == "*")
    throw SEMANTIC_ERROR(_("invalid use of wildcard probe point component"));

  map<match_key, match_node *>::const_iterator i = sub.find(k);
  if (i != sub.end())
    return i->second;
  match_node * n = new match_node();
  sub.insert(make_pair(k, n));
  return n;
}

void
match_node::bind(derived_probe_builder * e)
{
  ends.push_back (e);
}

match_node *
match_node::bind(interned_string k)
{
  return bind(match_key(k));
}

match_node *
match_node::bind_str(string const & k)
{
  return bind(match_key(k).with_string());
}

match_node *
match_node::bind_num(string const & k)
{
  return bind(match_key(k).with_number());
}

match_node *
match_node::bind_privilege(privilege_t p)
{
  privilege = p;
  return this;
}

void
match_node::find_and_build (systemtap_session& s,
                            probe* p, probe_point *loc, unsigned pos,
                            vector<derived_probe *>& results)
{
  assert (pos <= loc->components.size());
  if (pos == loc->components.size()) // matched all probe point components so far
    {
      if (ends.empty())
        {
          string alternatives;
          for (sub_map_iterator_t i = sub.begin(); i != sub.end(); i++)
            alternatives += string(" ") + i->first.str();

          throw SEMANTIC_ERROR (_F("probe point truncated (follow: %s)",
                                   alternatives.c_str()),
                                   loc->components.back()->tok);
        }

      if (! pr_contains (privilege, s.privilege))
	{
          throw SEMANTIC_ERROR (_F("probe point is not allowed for --privilege=%s",
				   pr_name (s.privilege)),
                                loc->components.back()->tok);
	}

      literal_map_t param_map;
      for (unsigned i=0; i<pos; i++)
        param_map[loc->components[i]->functor] = loc->components[i]->arg;
      // maybe 0

      // Iterate over all bound builders
      for (unsigned k=0; k<ends.size(); k++) 
        {
          derived_probe_builder *b = ends[k];
          b->build (s, p, loc, param_map, results);
        }
    }
  else if (isdoubleglob(loc->components[pos]->functor)) // ** wildcard?
    {
      unsigned int num_results = results.size();

      // When faced with "foo**bar", we try "foo*bar" and "foo*.**bar"

      const probe_point::component *comp = loc->components[pos];
      string functor = comp->functor;
      size_t glob_start = functor.find("**");
      size_t glob_end = functor.find_first_not_of('*', glob_start);
      string prefix = functor.substr(0, glob_start);
      string suffix = ((glob_end != string::npos) ?
                           functor.substr(glob_end) : "");

      // Synthesize "foo*bar"
      probe_point *simple_pp = new probe_point(*loc);
      probe_point::component *simple_comp = new probe_point::component(*comp);
      simple_comp->functor = prefix + "*" + suffix;
      simple_comp->from_glob = true;
      simple_pp->components[pos] = simple_comp;
      try
        {
          find_and_build (s, p, simple_pp, pos, results);
        }
      catch (const semantic_error& e)
        {
          // Ignore semantic_errors.
        }

      // Cleanup if we didn't find anything
      if (results.size() == num_results)
        {
          delete simple_pp;
          delete simple_comp;
        }

      num_results = results.size();

      // Synthesize "foo*.**bar"
      // NB: any component arg should attach to the latter part only
      probe_point *expanded_pp = new probe_point(*loc);
      probe_point::component *expanded_comp_pre = new probe_point::component(*comp);
      expanded_comp_pre->functor = prefix + "*";
      expanded_comp_pre->from_glob = true;
      expanded_comp_pre->arg = NULL;
      probe_point::component *expanded_comp_post = new probe_point::component(*comp);
      expanded_comp_post->functor = string("**") + suffix;
      expanded_pp->components[pos] = expanded_comp_pre;
      expanded_pp->components.insert(expanded_pp->components.begin() + pos + 1,
                                     expanded_comp_post);
      try
        {
          find_and_build (s, p, expanded_pp, pos, results);
        }
      catch (const semantic_error& e)
        {
          // Ignore semantic_errors.
        }

      // Cleanup if we didn't find anything
      if (results.size() == num_results)
        {
          delete expanded_pp;
          delete expanded_comp_pre;
          delete expanded_comp_post;
        }

      // Try suffix expansion only if no matches found:
      if (num_results == results.size())
        this->try_suffix_expansion (s, p, loc, pos, results);

      if (! loc->optional && num_results == results.size())
        {
          // We didn't find any wildcard matches (since the size of
          // the result vector didn't change).  Throw an error.
          string sugs = suggest_functors(s, functor);
          throw SEMANTIC_ERROR (_F("probe point mismatch: didn't find any wildcard matches%s",
                                   sugs.empty() ? "" : (" (similar: " + sugs + ")").c_str()),
                                comp->tok);
        }
    }
  else if (isglob(loc->components[pos]->functor)) // wildcard?
    {
      match_key match (* loc->components[pos]);

      // Call find_and_build for each possible match.  Ignore errors -
      // unless we don't find any match.
      unsigned int num_results = results.size();
      for (sub_map_iterator_t i = sub.begin(); i != sub.end(); i++)
        {
	  const match_key& subkey = i->first;
	  match_node* subnode = i->second;

          assert_no_interrupts();

	  if (match.globmatch(subkey))
	    {
	      if (s.verbose > 2)
                clog << _F("wildcard '%s' matched '%s'",
                           loc->components[pos]->functor.to_string().c_str(),
                           subkey.name.to_string().c_str()) << endl;
              
	      // When we have a wildcard, we need to create a copy of
	      // the probe point.  Then we'll create a copy of the
	      // wildcard component, and substitute the non-wildcard
	      // functor.
	      probe_point *non_wildcard_pp = new probe_point(*loc);
	      probe_point::component *non_wildcard_component
		= new probe_point::component(*loc->components[pos]);
	      non_wildcard_component->functor = subkey.name;
	      non_wildcard_component->from_glob = true;
	      non_wildcard_pp->components[pos] = non_wildcard_component;

              // NB: probe conditions are not attached at the wildcard
              // (component/functor) level, but at the overall
              // probe_point level.

	      unsigned int inner_results = results.size();

	      // recurse (with the non-wildcard probe point)
	      try
	        {
		  subnode->find_and_build (s, p, non_wildcard_pp, pos+1,
					   results);
	        }
	      catch (const semantic_error& e)
	        {
		  // Ignore semantic_errors while expanding wildcards.
		  // If we get done and nothing was expanded, the code
		  // following the loop will complain.
		}

	      if (results.size() == inner_results)
		{
		  // If this wildcard didn't match, cleanup.
		  delete non_wildcard_pp;
		  delete non_wildcard_component;
	        }
	    }
	}

      // Try suffix expansion only if no matches found:
      if (num_results == results.size())
        this->try_suffix_expansion (s, p, loc, pos, results);

      if (! loc->optional && num_results == results.size())
        {
	  // We didn't find any wildcard matches (since the size of
	  // the result vector didn't change).  Throw an error.
          string sugs = suggest_functors(s, loc->components[pos]->functor);
          throw SEMANTIC_ERROR (_F("probe point mismatch: didn't find any wildcard matches%s",
                                   sugs.empty() ? "" : (" (similar: " + sugs + ")").c_str()),
                                loc->components[pos]->tok);
	}
    }
  else
    {
      match_key match (* loc->components[pos]);
      sub_map_iterator_t i = sub.find (match);

      if (i != sub.end()) // match found
        {
          match_node* subnode = i->second;
          // recurse
          subnode->find_and_build (s, p, loc, pos+1, results);
          return;
        }

      unsigned int num_results = results.size();
      this->try_suffix_expansion (s, p, loc, pos, results);

      // XXX: how to correctly report alternatives + position numbers
      // for alias suffixes?  file a separate PR to address the issue
      if (! loc->optional && num_results == results.size())
        {
          // We didn't find any alias suffixes (since the size of the
          // result vector didn't change).  Throw an error.
          string sugs = suggest_functors(s, loc->components[pos]->functor);
          throw SEMANTIC_ERROR (_F("probe point mismatch%s",
                                   sugs.empty() ? "" : (" (similar: " + sugs + ")").c_str()),
                                loc->components[pos]->tok);
        }
    }
}

string
match_node::suggest_functors(systemtap_session& s, string functor)
{
  // only use prefix if globby (and prefix is non-empty)
  size_t glob = functor.find('*');
  if (glob != string::npos && glob != 0)
    functor.erase(glob);
  if (functor.empty())
    return "";

  // PR18577: There isn't any point in generating a suggestion list if
  // we're not going to display it.
  if ((s.dump_mode == systemtap_session::dump_matched_probes
       || s.dump_mode == systemtap_session::dump_matched_probes_vars)
      && s.verbose < 2)
    return "";

  set<string> functors;
  for (sub_map_iterator_t i = sub.begin(); i != sub.end(); i++)
    {
      string ftor = i->first.str();
      if (ftor.find('(') != string::npos)  // trim any parameter
        ftor.erase(ftor.find('('));
      functors.insert(ftor);
    }
  return levenshtein_suggest(functor, functors, 5); // print top 5
}

void
match_node::try_suffix_expansion (systemtap_session& s,
                                  probe *p, probe_point *loc, unsigned pos,
                                  vector<derived_probe *>& results)
{
  // PR12210: match alias suffixes. If the components thus far
  // have been matched, but there is an additional unknown
  // suffix, we have a potential alias suffix on our hands. We
  // need to expand the preceding components as probe aliases,
  // reattach the suffix, and re-run derive_probes() on the
  // resulting expansion. This is done by the routine
  // build_with_suffix().

  if (strverscmp(s.compatible.c_str(), "2.0") >= 0)
    {
      // XXX: technically, param_map isn't used here.  So don't
      // bother actually assembling it unless some
      // derived_probe_builder appears that actually takes
      // suffixes *and* consults parameters (currently no such
      // builders exist).
      literal_map_t param_map;
      // for (unsigned i=0; i<pos; i++)
      //   param_map[loc->components[i]->functor] = loc->components[i]->arg;
      // maybe 0
      
      vector<probe_point::component *> suffix (loc->components.begin()+pos,
                                               loc->components.end());
      
      // Multiple derived_probe_builders may be bound at a
      // match_node due to the possibility of multiply defined
      // aliases.
      for (unsigned k=0; k < ends.size(); k++)
        {
          derived_probe_builder *b = ends[k];
          try
            {
              b->build_with_suffix (s, p, loc, param_map, results, suffix);
            }
          catch (const recursive_expansion_error &e)
            {
              // Re-throw:
              throw semantic_error(e);
            }
          catch (const semantic_error &e)
            {
              // Adjust source coordinate and re-throw:
              if (! loc->optional)
                throw semantic_error(e.errsrc, e.what(), loc->components[pos]->tok);
            }
        }
    }
}


void
match_node::build_no_more (systemtap_session& s)
{
  for (sub_map_iterator_t i = sub.begin(); i != sub.end(); i++)
    i->second->build_no_more (s);
  for (unsigned k=0; k<ends.size(); k++) 
    {
      derived_probe_builder *b = ends[k];
      b->build_no_more (s);
    }
}

void
match_node::dump (systemtap_session &s, const string &name)
{
  // Dump this node, if it is complete.
  for (unsigned k=0; k<ends.size(); k++)
    {
      // Don't print aliases at all (for now) until we can figure out how to determine whether
      // the probes they resolve to are ok in unprivileged mode.
      if (ends[k]->is_alias ())
	continue;

      // In unprivileged mode, don't show the probes which are not allowed for unprivileged
      // users.
      if (pr_contains (privilege, s.privilege))
	{
	  cout << name << endl;
	  break; // we need only print one instance.
	}
    }

  // Recursively dump the children of this node
  string dot;
  if (! name.empty ())
    dot = ".";
  for (sub_map_iterator_t i = sub.begin(); i != sub.end(); i++)
    {
      i->second->dump (s, name + dot + i->first.str());
    }
}


// ------------------------------------------------------------------------
// Alias probes
// ------------------------------------------------------------------------

struct alias_derived_probe: public derived_probe
{
  alias_derived_probe (probe* base, probe_point *l, const probe_alias *a,
                       const vector<probe_point::component *> *suffix = 0);
  ~alias_derived_probe();

  void upchuck () { throw SEMANTIC_ERROR (_("inappropriate"), this->tok); }

  // Alias probes are immediately expanded to other derived_probe
  // types, and are not themselves emitted or listed in
  // systemtap_session.probes

  void join_group (systemtap_session&) { upchuck (); }

  virtual const probe_alias *get_alias () const { return alias; }
  virtual probe_point *get_alias_loc () const { return alias_loc; }
  virtual probe_point *sole_location () const;

private:
  const probe_alias *alias; // Used to check for recursion
  probe_point *alias_loc; // Hack to recover full probe name
};


alias_derived_probe::alias_derived_probe(probe *base, probe_point *l,
                                         const probe_alias *a,
                                         const vector<probe_point::component *>
                                           *suffix):
  derived_probe (base, l), alias(a)
{
  // XXX pretty nasty -- this was cribbed from printscript() in main.cxx
  assert (alias->alias_names.size() >= 1);
  alias_loc = new probe_point(*alias->alias_names[0]); // XXX: [0] is arbitrary; it would make just as much sense to collect all of the names
  alias_loc->well_formed = true;
  vector<probe_point::component*>::const_iterator it;
  for (it = suffix->begin(); it != suffix->end(); ++it)
    {
      alias_loc->components.push_back(*it);
      if (isglob((*it)->functor))
        alias_loc->well_formed = false; // needs further derivation
    }
}

alias_derived_probe::~alias_derived_probe ()
{
  delete alias_loc;
}


probe_point*
alias_derived_probe::sole_location () const
{
  return const_cast<probe_point*>(alias_loc);
}


void
alias_expansion_builder::build(systemtap_session & sess,
			       probe * use,
			       probe_point * location,
			       literal_map_t const & parameters,
			       vector<derived_probe *> & finished_results)
{
  vector<probe_point::component *> empty_suffix;
  build_with_suffix (sess, use, location, parameters,
                     finished_results, empty_suffix);
}

void
alias_expansion_builder::build_with_suffix(systemtap_session & sess,
                                           probe * use,
                                           probe_point * location,
                                           literal_map_t const &,
                                           vector<derived_probe *>
                                             & finished_results,
                                           vector<probe_point::component *>
                                             const & suffix)
{
  // Don't build the alias expansion if infinite recursion is detected.
  if (checkForRecursiveExpansion (use)) {
    stringstream msg;
    msg << _F("recursive loop in alias expansion of %s at %s",
              lex_cast(*location).c_str(), lex_cast(location->components.front()->tok->location).c_str());
    // semantic_errors thrown here might be ignored, so we need a special class:
    throw recursive_expansion_error (msg.str());
    // XXX The point of throwing this custom error is to suppress a
    // cascade of "probe mismatch" messages that appear in addition to
    // the error. The current approach suppresses most of the error
    // cascade, but leaves one spurious error; in any case, the way
    // this particular error is reported could be improved.
  }

  // We're going to build a new probe and wrap it up in an
  // alias_expansion_probe so that the expansion loop recognizes it as
  // such and re-expands its expansion.

  alias_derived_probe * n = new alias_derived_probe (use, location /* soon overwritten */, this->alias, &suffix);
  n->body = new block();

  // The new probe gets a deep copy of the location list of the alias
  // (with incoming condition joined) plus the suffix (if any),
  n->locations.clear();
  for (unsigned i=0; i<alias->locations.size(); i++)
    {
      probe_point *pp = new probe_point(*alias->locations[i]);
      pp->components.insert(pp->components.end(), suffix.begin(), suffix.end());
      pp->condition = add_condition (pp->condition, location->condition);
      n->locations.push_back(pp);
    }

  // the token location of the alias,
  n->tok = location->components.front()->tok;

  // and statements representing the concatenation of the alias'
  // body with the use's.
  //
  // NB: locals are *not* copied forward, from either alias or
  // use. The expansion should have its locals re-inferred since
  // there's concatenated code here and we only want one vardecl per
  // resulting variable.

  if (alias->epilogue_style)
    n->body = new block (use->body, alias->body);
  else
    n->body = new block (alias->body, use->body);

  unsigned old_num_results = finished_results.size();
  // If expanding for an alias suffix, be sure to pass on any errors
  // to the caller instead of printing them in derive_probes():
  derive_probes (sess, n, finished_results, location->optional, !suffix.empty());

  // Check whether we resolved something. If so, put the
  // whole library into the queue if not already there.
  if (finished_results.size() > old_num_results)
    {
      stapfile *f = alias->tok->location.file;
      if (find (sess.files.begin(), sess.files.end(), f)
	  == sess.files.end())
	sess.files.push_back (f);
    }
}

bool
alias_expansion_builder::checkForRecursiveExpansion (probe *use)
{
  // Collect the derivation chain of this probe.
  vector<probe*>derivations;
  use->collect_derivation_chain (derivations);

  // Check all probe points in the alias expansion against the currently-being-expanded probe point
  // of each of the probes in the derivation chain, looking for a match. This
  // indicates infinite recursion.
  // The first element of the derivation chain will be the derived_probe representing 'use', so
  // start the search with the second element.
  assert (derivations.size() > 0);
  assert (derivations[0] == use);
  for (unsigned d = 1; d < derivations.size(); ++d) {
    if (use->get_alias() == derivations[d]->get_alias())
      return true; // recursion detected
  }
  return false;
}


// ------------------------------------------------------------------------
// Pattern matching
// ------------------------------------------------------------------------

static unsigned max_recursion = 100;

struct
recursion_guard
{
  unsigned & i;
  recursion_guard(unsigned & i) : i(i)
    {
      if (i > max_recursion)
	throw SEMANTIC_ERROR(_("recursion limit reached"));
      ++i;
    }
  ~recursion_guard()
    {
      --i;
    }
};

// The match-and-expand loop.
void
derive_probes (systemtap_session& s,
               probe *p, vector<derived_probe*>& dps,
               bool optional,
               bool rethrow_errors)
{
  // We need a static to track whether the current probe is optional so that
  // even if we recurse into derive_probes with optional = false, errors will
  // still be ignored. The undo_parent_optional bool ensures we reset the
  // static at the same level we had it set.
  static bool parent_optional = false;
  bool undo_parent_optional = false;

  if (optional && !parent_optional)
    {
      parent_optional = true;
      undo_parent_optional = true;
    }

  vector <semantic_error> optional_errs;

  for (unsigned i = 0; i < p->locations.size(); ++i)
    {
      assert_no_interrupts();

      probe_point *loc = p->locations[i];

      if (s.verbose > 4)
        clog << "derive-probes " << *loc << endl;

      try
        {
          unsigned num_atbegin = dps.size();

          try
	    {
	      s.pattern_root->find_and_build (s, p, loc, 0, dps); // <-- actual derivation!
	    }
          catch (const semantic_error& e)
	    {
              if (!loc->optional && !parent_optional)
                throw semantic_error(e);
              else /* tolerate failure for optional probe */
                {
                  // remember err, we will print it (in catch block) if any
                  // non-optional loc fails to resolve
                  semantic_error err(ERR_SRC, _("while resolving probe point"),
                                     loc->components[0]->tok, NULL, &e);
                  optional_errs.push_back(err);
                  continue;
                }
	    }

          unsigned num_atend = dps.size();

          if (! (loc->optional||parent_optional) && // something required, but
              num_atbegin == num_atend) // nothing new derived!
            throw SEMANTIC_ERROR (_("no match"));

          if (loc->sufficient && (num_atend > num_atbegin))
            {
              if (s.verbose > 1)
                {
                  clog << "Probe point ";
                  p->locations[i]->print(clog);
                  clog << " sufficient, skipped";
                  for (unsigned j = i+1; j < p->locations.size(); ++j)
                    {
                      clog << " ";
                      p->locations[j]->print(clog);
                    }
                  clog << endl;
                }
              break; // we need not try to derive for any other locations
            }
        }
      catch (const semantic_error& e)
        {
          // The rethrow_errors parameter lets the caller decide an
          // alternative to printing the error. This is necessary when
          // calling derive_probes() recursively during expansion of
          // an alias with suffix -- any message printed here would
          // point to the alias declaration and not the invalid suffix
          // usage, so the caller needs to catch the error themselves
          // and print a more appropriate message.
          if (rethrow_errors)
            {
              throw semantic_error(e);
            }
	  // Only output in dump mode if -vv is supplied:
          else if (!s.dump_mode || (s.verbose > 1))
            {
              // print this one manually first because it's more important than
              // the optional errs
              semantic_error err(ERR_SRC, _("while resolving probe point"),
                                 loc->components[0]->tok, NULL, &e);
              s.print_error(err);

              // print optional errs accumulated while visiting other probe points
              for (vector<semantic_error>::const_iterator it = optional_errs.begin();
                   it != optional_errs.end(); ++it)
                {
                  s.print_error(*it);
                }
            }
        }
    }

  if (undo_parent_optional)
    parent_optional = false;
}



// ------------------------------------------------------------------------
//
// Indexable usage checks
//

struct symbol_fetcher
  : public throwing_visitor
{
  symbol *&sym;

  symbol_fetcher (symbol *&sym): sym(sym)
  {}

  void visit_symbol (symbol* e)
  {
    sym = e;
  }

  void visit_arrayindex (arrayindex* e)
  {
    e->base->visit (this);
  }

  void throwone (const token* t)
  {
    throw SEMANTIC_ERROR (_("Expecting symbol or array index expression"), t);
  }
};

symbol *
get_symbol_within_expression (expression *e)
{
  symbol *sym = NULL;
  symbol_fetcher fetcher(sym);
  e->visit (&fetcher);
  return sym; // NB: may be null!
}

static symbol *
get_symbol_within_indexable (indexable *ix)
{
  symbol *array = NULL;
  hist_op *hist = NULL;
  classify_indexable(ix, array, hist);
  if (array)
    return array;
  else
    return get_symbol_within_expression (hist->stat);
}

struct mutated_var_collector
  : public traversing_visitor
{
  set<vardecl *> * mutated_vars;

  mutated_var_collector (set<vardecl *> * mm)
    : mutated_vars (mm)
  {}

  void visit_assignment(assignment* e)
  {
    if (e->type == pe_stats && e->op == "<<<")
      {
	vardecl *vd = get_symbol_within_expression (e->left)->referent;
	if (vd)
	  mutated_vars->insert (vd);
      }
    traversing_visitor::visit_assignment(e);
  }

  void visit_arrayindex (arrayindex *e)
  {
    if (is_active_lvalue (e))
      {
	symbol *sym;
	if (e->base->is_symbol (sym))
	  mutated_vars->insert (sym->referent);
	else
	  throw SEMANTIC_ERROR(_("Assignment to read-only histogram bucket"), e->tok);
      }
    traversing_visitor::visit_arrayindex (e);
  }
};


struct no_var_mutation_during_iteration_check
  : public traversing_visitor
{
  systemtap_session & session;
  map<functiondecl *,set<vardecl *> *> & function_mutates_vars;
  vector<vardecl *> vars_being_iterated;

  no_var_mutation_during_iteration_check
  (systemtap_session & sess,
   map<functiondecl *,set<vardecl *> *> & fmv)
    : session(sess), function_mutates_vars (fmv)
  {}

  void visit_arrayindex (arrayindex *e)
  {
    if (is_active_lvalue(e))
      {
	vardecl *vd = get_symbol_within_indexable (e->base)->referent;
	if (vd)
	  {
	    for (unsigned i = 0; i < vars_being_iterated.size(); ++i)
	      {
		vardecl *v = vars_being_iterated[i];
		if (v == vd)
		  {
                    string err = _F("variable '%s' modified during 'foreach' iteration",
                                    v->unmangled_name.to_string().c_str());
		    session.print_error (SEMANTIC_ERROR (err, e->tok));
		  }
	      }
	  }
      }
    traversing_visitor::visit_arrayindex (e);
  }

  void visit_functioncall (functioncall* e)
  {
    for (unsigned fd = 0; fd < e->referents.size(); fd++)
      {
        map<functiondecl *,set<vardecl *> *>::const_iterator i
          = function_mutates_vars.find (e->referents[fd]);

        if (i != function_mutates_vars.end())
          {
            for (unsigned j = 0; j < vars_being_iterated.size(); ++j)
              {
                vardecl *m = vars_being_iterated[j];
                if (i->second->find (m) != i->second->end())
                  {
                    string err = _F("function call modifies var '%s' during 'foreach' iteration",
                                    m->unmangled_name.to_string().c_str());
                    session.print_error (SEMANTIC_ERROR (err, e->tok));
                  }
              }
          }
      }

    traversing_visitor::visit_functioncall (e);
  }

  void visit_foreach_loop(foreach_loop* s)
  {
    vardecl *vd = get_symbol_within_indexable (s->base)->referent;

    if (vd)
      vars_being_iterated.push_back (vd);

    traversing_visitor::visit_foreach_loop (s);

    if (vd)
      vars_being_iterated.pop_back();
  }
};


// ------------------------------------------------------------------------

struct stat_decl_collector
  : public traversing_visitor
{
  systemtap_session & session;

  stat_decl_collector(systemtap_session & sess)
    : session(sess)
  {}

  void visit_stat_op (stat_op* e)
  {
    symbol *sym = get_symbol_within_expression (e->stat);
    if (session.stat_decls.find(sym->name) == session.stat_decls.end())
      session.stat_decls[sym->name] = statistic_decl();
  }

  void visit_assignment (assignment* e)
  {
    if (e->op == "<<<")
      {
	symbol *sym = get_symbol_within_expression (e->left);
	if (session.stat_decls.find(sym->name) == session.stat_decls.end())
	  session.stat_decls[sym->name] = statistic_decl();
      }
    else
      traversing_visitor::visit_assignment(e);
  }

  void visit_hist_op (hist_op* e)
  {
    symbol *sym = get_symbol_within_expression (e->stat);
    statistic_decl new_stat;

    if (e->htype == hist_linear)
      {
	new_stat.type = statistic_decl::linear;
	assert (e->params.size() == 3);
	new_stat.linear_low = e->params[0];
	new_stat.linear_high = e->params[1];
	new_stat.linear_step = e->params[2];
      }
    else
      {
	assert (e->htype == hist_log);
	new_stat.type = statistic_decl::logarithmic;
	assert (e->params.size() == 0);
      }

    map<interned_string, statistic_decl>::iterator i = session.stat_decls.find(sym->name);
    if (i == session.stat_decls.end())
      session.stat_decls[sym->name] = new_stat;
    else
      {
	statistic_decl & old_stat = i->second;
	if (!(old_stat == new_stat))
	  {
	    if (old_stat.type == statistic_decl::none)
	      i->second = new_stat;
	    else
	      {
		// FIXME: Support multiple co-declared histogram types
		semantic_error se(ERR_SRC, _F("multiple histogram types declared on '%s'",
                                              sym->name.to_string().c_str()), e->tok);
		session.print_error (se);
	      }
	  }
      }
  }

};

static int
semantic_pass_stats (systemtap_session & sess)
{
  stat_decl_collector sdc(sess);

  for (map<string,functiondecl*>::iterator it = sess.functions.begin(); it != sess.functions.end(); it++)
    it->second->body->visit (&sdc);

  for (unsigned i = 0; i < sess.probes.size(); ++i)
    sess.probes[i]->body->visit (&sdc);

  for (unsigned i = 0; i < sess.globals.size(); ++i)
    {
      vardecl *v = sess.globals[i];
      if (v->type == pe_stats)
	{

	  if (sess.stat_decls.find(v->name) == sess.stat_decls.end())
	    {
              semantic_error se(ERR_SRC, _F("unable to infer statistic parameters for global '%s'",
                                            v->unmangled_name.to_string().c_str()));
	      sess.print_error (se);
	    }
	}
    }

  return sess.num_errors();
}

// ------------------------------------------------------------------------

// Enforce variable-related invariants: no modification of
// a foreach()-iterated array.
static int
semantic_pass_vars (systemtap_session & sess)
{

  map<functiondecl *, set<vardecl *> *> fmv;
  no_var_mutation_during_iteration_check chk(sess, fmv);

  for (map<string,functiondecl*>::iterator it = sess.functions.begin(); it != sess.functions.end(); it++)
    {
      functiondecl * fn = it->second;
      if (fn->body)
	{
	  set<vardecl *> * m = new set<vardecl *>();
	  mutated_var_collector mc (m);
	  fn->body->visit (&mc);
	  fmv[fn] = m;
	}
    }

  for (map<string,functiondecl*>::iterator it = sess.functions.begin(); it != sess.functions.end(); it++)
    {
      functiondecl * fn = it->second;
      if (fn->body) fn->body->visit (&chk);
    }

  for (unsigned i = 0; i < sess.probes.size(); ++i)
    {
      if (sess.probes[i]->body)
	sess.probes[i]->body->visit (&chk);
    }

  return sess.num_errors();
}


// ------------------------------------------------------------------------

// Rewrite probe condition expressions into probe bodies.  Tricky and
// exciting business, this.  This:
//
// probe foo if (g1 || g2) { ... }
// probe bar { ... g1 ++ ... }
//
// becomes:
//
// probe foo { if (! (g1 || g2)) next; ... }
// probe bar { ... g1 ++ ...;
//             if (g1 || g2) %{ enable_probe_foo %} else %{ disable_probe_foo %}
//           }
//
// In other words, we perform two transformations:
//    (1) Inline probe condition into its body.
//    (2) For each probe that modifies a global var in use in any probe's
//        condition, re-evaluate those probes' condition at the end of that
//        probe's body.
//
// Here, we do all of (1), and half of (2): we simply collect the dependency
// info between probes, which the translator will use to emit the affected
// probes' condition re-evaluation. The translator will also ensure that the
// conditions are evaluated using the globals' starting values prior to any
// probes starting.

// Adds the condition expression to the front of the probe's body
static void
derived_probe_condition_inline (derived_probe *p)
{
  expression* e = p->sole_location()->condition;
  assert(e);

  if_statement *ifs = new if_statement ();
  ifs->tok = e->tok;
  ifs->thenblock = new next_statement ();
  ifs->thenblock->tok = e->tok;
  ifs->elseblock = NULL;
  unary_expression *notex = new unary_expression ();
  notex->op = "!";
  notex->tok = e->tok;
  notex->operand = e;
  ifs->condition = notex;
  p->body = new block (ifs, p->body);
}

static int
semantic_pass_conditions (systemtap_session & sess)
{
  map<derived_probe*, set<vardecl*> > vars_read_in_cond;
  map<derived_probe*, set<vardecl*> > vars_written_in_body;

  // do a first pass through the probes to ensure safety, inline any condition,
  // and collect var usage
  for (unsigned i = 0; i < sess.probes.size(); ++i)
    {
      derived_probe* p = sess.probes[i];
      expression* e = p->sole_location()->condition;

      if (e)
        {
          varuse_collecting_visitor vcv_cond(sess);
          e->visit (& vcv_cond);

          if (!vcv_cond.written.empty())
            sess.print_error (SEMANTIC_ERROR (_("probe condition must not "
                                                "modify any variables"),
                                              e->tok));
          else if (vcv_cond.embedded_seen)
            sess.print_error (SEMANTIC_ERROR (_("probe condition must not "
                                                "include impure embedded-C"),
                                              e->tok));

          derived_probe_condition_inline(p);

          vars_read_in_cond[p].insert(vcv_cond.read.begin(),
                                      vcv_cond.read.end());
        }

      varuse_collecting_visitor vcv_body(sess);
      p->body->visit (& vcv_body);

      vars_written_in_body[p].insert(vcv_body.written.begin(),
                                     vcv_body.written.end());
    }

  // do a second pass to collect affected probes
  for (unsigned i = 0; i < sess.probes.size(); ++i)
    {
      derived_probe *p = sess.probes[i];

      // for each variable this probe modifies...
      set<vardecl*>::const_iterator var;
      for (var  = vars_written_in_body[p].begin();
           var != vars_written_in_body[p].end(); ++var)
        {
          // collect probes which could be affected
          for (unsigned j = 0; j < sess.probes.size(); ++j)
            {
              if (vars_read_in_cond[sess.probes[j]].count(*var))
                {
                  if (!p->probes_with_affected_conditions.count(sess.probes[j]))
                    {
                      p->probes_with_affected_conditions.insert(sess.probes[j]);
                      if (sess.verbose > 2)
                        clog << "probe " << i << " can affect condition of "
                                "probe " << j << endl;
                    }
                }
            }
        }
    }

  // PR18115: We create a begin probe which is artificially registered as
  // affecting every other probe. This will serve as the initializer so that
  // other probe types with false conditions can be skipped (or registered as
  // disabled) during module initialization.

  set<derived_probe*> targets;
  for (unsigned i = 0; i < sess.probes.size(); ++i)
    if (!vars_read_in_cond[sess.probes[i]].empty())
      targets.insert(sess.probes[i]);

  if (!targets.empty())
    {
      stringstream ss("probe begin {}");

      // no good token to choose here... let's just use the condition expression
      // of one of the probes as the token
      const token *tok = (*targets.begin())->sole_location()->condition->tok;

      probe *p = parse_synthetic_probe(sess, ss, tok);
      if (!p)
        throw SEMANTIC_ERROR (_("can't create cond initializer probe"), tok);

      vector<derived_probe*> dps;
      derive_probes(sess, p, dps);

      // there should only be one
      assert(dps.size() == 1);

      derived_probe* dp = dps[0];
      dp->probes_with_affected_conditions.insert(targets.begin(),
                                                 targets.end());
      sess.probes.push_back (dp);
      dp->join_group (sess);

      // no need to manually do symresolution since body is empty
    }

  return sess.num_errors();
}

// ------------------------------------------------------------------------


// Simple visitor that just goes through all embedded code blocks that
// are available at the end  all the optimizations to register any
// relevant pragmas or other indicators found, so that session flags can
// be set that can be inspected at translation time to trigger any
// necessary initialization of code needed by the embedded code functions.

// This is only for pragmas that don't have any other side-effect than
// needing some initialization at module init time. Currently handles
// /* pragma:vma */ /* pragma:unwind */ /* pragma:symbols */ /* pragma:lines */

// /* pragma:uprobes */ is handled during the typeresolution_info pass.
// /* pure */, /* unprivileged */. /* myproc-unprivileged */ and /* guru */
// are handled by the varuse_collecting_visitor.

struct embeddedcode_info: public functioncall_traversing_visitor
{
protected:
  systemtap_session& session;

public:
  embeddedcode_info (systemtap_session& s): session(s) { }

  void visit_embeddedcode (embeddedcode* c)
  {
    if (! vma_tracker_enabled(session)
	&& c->code.find("/* pragma:vma */") != string::npos)
      {
	if (session.verbose > 2)
          clog << _F("Turning on task_finder vma_tracker, pragma:vma found in %s",
                     current_function->unmangled_name.to_string().c_str()) << endl;

	// PR15052: stapdyn doesn't have VMA-tracking yet.
	if (session.runtime_usermode_p())
	  throw SEMANTIC_ERROR(_("VMA-tracking is only supported by the kernel runtime (PR15052)"), c->tok);

	enable_vma_tracker(session);
      }

    if (! session.need_unwind
	&& c->code.find("/* pragma:unwind */") != string::npos)
      {
	if (session.verbose > 2)
	  clog << _F("Turning on unwind support, pragma:unwind found in %s",
		    current_function->unmangled_name.to_string().c_str()) << endl;
	session.need_unwind = true;
      }

    if (! session.need_symbols
	&& c->code.find("/* pragma:symbols */") != string::npos)
      {
	if (session.verbose > 2)
	  clog << _F("Turning on symbol data collecting, pragma:symbols found in %s",
		    current_function->unmangled_name.to_string().c_str())
	       << endl;
	session.need_symbols = true;
      }

    if (! session.need_lines
        && c->code.find("/* pragma:lines */") != string::npos)
      {
        if (session.verbose > 2)
	  clog << _F("Turning on debug line data collecting, pragma:lines found in %s",
		    current_function->unmangled_name.to_string().c_str())
	       << endl;
	session.need_lines = true;
      }
  }
};

void embeddedcode_info_pass (systemtap_session& s)
{
  embeddedcode_info eci (s);
  for (unsigned i=0; i<s.probes.size(); i++)
    s.probes[i]->body->visit (& eci);
}

// ------------------------------------------------------------------------


// Simple visitor that collects all the regular expressions in the
// file and adds them to the session DFA table.

struct regex_collecting_visitor: public functioncall_traversing_visitor
{
protected:
  systemtap_session& session;

public:
  regex_collecting_visitor (systemtap_session& s): session(s) { }

  void visit_regex_query (regex_query *q) {
    functioncall_traversing_visitor::visit_regex_query (q);

    string re = q->right->value;
    regex_to_stapdfa (&session, re, q->right->tok);
  }
};

// Go through the regex match invocations and generate corresponding DFAs.
int gen_dfa_table (systemtap_session& s)
{
  regex_collecting_visitor rcv(s);

  for (unsigned i=0; i<s.probes.size(); i++)
    {
      try
        {
          s.probes[i]->body->visit (& rcv);
          
          if (s.probes[i]->sole_location()->condition)
            s.probes[i]->sole_location()->condition->visit (& rcv);
        }
      catch (const semantic_error& e)
        {
          s.print_error (e);
        }
    }

  return s.num_errors();
}

// ------------------------------------------------------------------------


static int semantic_pass_symbols (systemtap_session&);
static int semantic_pass_optimize1 (systemtap_session&);
static int semantic_pass_optimize2 (systemtap_session&);
static int semantic_pass_types (systemtap_session&);
static int semantic_pass_vars (systemtap_session&);
static int semantic_pass_stats (systemtap_session&);
static int semantic_pass_conditions (systemtap_session&);


struct expression_build_no_more_visitor : public expression_visitor
{
  // Clear extra details from every expression, like DWARF type info, so that
  // builders can safely release them in build_no_more.  From here on out,
  // we're back to basic types only.
  void visit_expression(expression *e)
    {
      e->type_details.reset();
    }
};

static void
build_no_more (systemtap_session& s)
{
  expression_build_no_more_visitor v;

  for (unsigned i=0; i<s.probes.size(); i++)
    s.probes[i]->body->visit(&v);

  for (map<string,functiondecl*>::iterator it = s.functions.begin();
       it != s.functions.end(); it++)
    it->second->body->visit(&v);

  // Inform all derived_probe builders that we're done with
  // all resolution, so it's time to release caches.
  s.pattern_root->build_no_more (s);
}



// Link up symbols to their declarations.  Set the session's
// files/probes/functions/globals vectors from the transitively
// reached set of stapfiles in s.library_files, starting from
// s.user_file.  Perform automatic tapset inclusion and probe
// alias expansion.
static int
semantic_pass_symbols (systemtap_session& s)
{
  symresolution_info sym (s);

  // If we're listing functions, then we need to include all the files. Probe
  // aliases won't be visited/derived so all we gain are the functions, global
  // variables, and any real probes (e.g. begin probes). NB: type resolution for
  // a specific function arg may fail if it could only be determined from a
  // function call in one of the skipped aliases.
  if (s.dump_mode == systemtap_session::dump_functions)
    {
      s.files.insert(s.files.end(), s.library_files.begin(),
                                    s.library_files.end());
    }
  else if (!s.user_files.empty())
    {
      // Normal run: seed s.files with user_files and let it grow through the
      // find_* functions. NB: s.files can grow during this iteration, so
      // size() can return gradually increasing numbers.
      s.files.insert (s.files.end(), s.user_files.begin(), s.user_files.end());
    }

  for (unsigned i = 0; i < s.files.size(); i++)
    {
      assert_no_interrupts();
      stapfile* dome = s.files[i];

      // Pass 1: add globals and functions to systemtap-session master list,
      //         so the find_* functions find them
      //
      // NB: tapset global/function definitions may duplicate or conflict
      // with those already in s.globals/functions.  We need to deconflict
      // here.

      for (unsigned i=0; i<dome->globals.size(); i++)
        {
          vardecl* g = dome->globals[i];
          for (unsigned j=0; j<s.globals.size(); j++)
            {
              vardecl* g2 = s.globals[j];
              if (g->name == g2->name)
                {
                  s.print_error (SEMANTIC_ERROR (_("conflicting global variables"),
                                                 g->tok, g2->tok));
                }
            }
          s.globals.push_back (g);
        }

      for (unsigned i=0; i<dome->functions.size(); i++)
        {
          functiondecl* f = dome->functions[i];
          functiondecl* f2 = s.functions[f->name];
          if (f2 && f != f2)
            {
              s.print_error (SEMANTIC_ERROR (_("conflicting functions"), 
                                             f->tok, f2->tok));
            }
          s.functions[f->name] = f;
        }

      // NB: embeds don't conflict with each other
      for (unsigned i=0; i<dome->embeds.size(); i++)
        s.embeds.push_back (dome->embeds[i]);

      // Pass 2: derive probes and resolve any further symbols in the
      // derived results.

      for (unsigned i=0; i<dome->probes.size(); i++)
        {
          assert_no_interrupts();
          probe* p = dome->probes [i];
          vector<derived_probe*> dps;

          // much magic happens here: probe alias expansion, wildcard
          // matching, low-level derived_probe construction.
          derive_probes (s, p, dps);

          for (unsigned j=0; j<dps.size(); j++)
            {
              assert_no_interrupts();
              derived_probe* dp = dps[j];
              s.probes.push_back (dp);
              dp->join_group (s);

              try
                {
                  for (unsigned k=0; k<s.code_filters.size(); k++)
                    s.code_filters[k]->replace (dp->body);

                  sym.current_function = 0;
                  sym.current_probe = dp;
                  dp->body->visit (& sym);

                  // Process the probe-point condition expression.
                  sym.current_function = 0;
                  sym.current_probe = 0;
                  if (dp->sole_location()->condition)
                    dp->sole_location()->condition->visit (& sym);
                }
              catch (const semantic_error& e)
                {
                  s.print_error (e);
                }
            }
        }

      // Pass 3: process functions

      for (unsigned i=0; i<dome->functions.size(); i++)
        {
          assert_no_interrupts();
          functiondecl* fd = dome->functions[i];

          try
            {
              for (unsigned j=0; j<s.code_filters.size(); j++)
                s.code_filters[j]->replace (fd->body);

              sym.current_function = fd;
              sym.current_probe = 0;
              fd->body->visit (& sym);
            }
          catch (const semantic_error& e)
            {
              s.print_error (e);
            }
        }
    }

  if(s.systemtap_v_check){ 
    for(unsigned i=0;i<s.globals.size();i++){
      if(s.globals[i]->systemtap_v_conditional)
        s.print_warning(_("This global uses tapset constructs that are dependent on systemtap version"), s.globals[i]->tok);
    }

    for(map<string, functiondecl*>::const_iterator i=s.functions.begin();i != s.functions.end();++i){
      if(i->second->systemtap_v_conditional)
        s.print_warning(_("This function uses tapset constructs that are dependent on systemtap version"), i->second->tok);
    }

    for(unsigned i=0;i<s.probes.size();i++){
      vector<probe*> sysvc;
      s.probes[i]->collect_derivation_chain(sysvc);
      for(unsigned j=0;j<sysvc.size();j++){
        if(sysvc[j]->systemtap_v_conditional)
          s.print_warning(_("This probe uses tapset constructs that are dependent on systemtap version"), sysvc[j]->tok);
        if(sysvc[j]->get_alias() && sysvc[j]->get_alias()->systemtap_v_conditional)
          s.print_warning(_("This alias uses tapset constructs that are dependent on systemtap version"), sysvc[j]->get_alias()->tok);
      }
    }
  }

  return s.num_errors(); // all those print_error calls
}


// Keep unread global variables for probe end value display.
void add_global_var_display (systemtap_session& s)
{
  // Don't generate synthetic end probes when in listing mode; it would clutter
  // up the list of probe points with "end ...". In fact, don't bother in any
  // dump mode at all, since it'll never be used.
  if (s.dump_mode) return;

  varuse_collecting_visitor vut(s);

  for (unsigned i=0; i<s.probes.size(); i++)
    {
      s.probes[i]->body->visit (& vut);

      if (s.probes[i]->sole_location()->condition)
	s.probes[i]->sole_location()->condition->visit (& vut);
    }

  for (unsigned g=0; g < s.globals.size(); g++)
    {
      vardecl* l = s.globals[g];
      if ((vut.read.find (l) != vut.read.end()
           && vut.used.find (l) != vut.used.end())
          || vut.written.find (l) == vut.written.end())
	continue;

      // Don't generate synthetic end probes for unread globals
      // declared only within tapsets. (RHBZ 468139), but rather
      // only within the end-user script.

      bool tapset_global = false;
      for (size_t m=0; m < s.library_files.size(); m++)
	{
	  for (size_t n=0; n < s.library_files[m]->globals.size(); n++)
	    {
	      if (l->name == s.library_files[m]->globals[n]->name)
		{tapset_global = true; break;}
	    }
	}
      if (tapset_global)
	continue;

      stringstream code;
      code << "probe end {" << endl;

      string format = l->unmangled_name;

      string indexes;
      string foreach_value;
      if (!l->index_types.empty())
	{
	  // Add index values to the printf format, and prepare
	  // a simple list of indexes for passing around elsewhere
	  format += "[";
	  for (size_t i = 0; i < l->index_types.size(); ++i)
	    {
	      if (i > 0)
		{
		  indexes += ",";
		  format += ",";
		}
	      indexes += "__idx" + lex_cast(i);
	      if (l->index_types[i] == pe_string)
		format += "\\\"%#s\\\"";
	      else
		format += "%#d";
	    }
	  format += "]";

	  // Iterate over all indexes in the array, sorted by decreasing value
	  code << "foreach (";
          if (l->type != pe_stats)
            {
              foreach_value = "__val";
              code << foreach_value << " = ";
            }
	  code << "[" << indexes << "] in " << l->unmangled_name << "-)" << endl;
	}
      else if (l->type == pe_stats)
	{
	  // PR7053: Check scalar globals for empty aggregate
	  code << "if (@count(" << l->unmangled_name << ") == 0)" << endl;
	  code << "printf(\"" << l->unmangled_name << " @count=0x0\\n\")" << endl;
	  code << "else" << endl;
	}

      static const string stats[] = { "@count", "@min", "@max", "@sum", "@avg" };
      const string stats_format =
	(strverscmp(s.compatible.c_str(), "1.4") >= 0) ? "%#d" : "%#x";

      // Fill in the printf format for values
      if (l->type == pe_stats)
	for (size_t i = 0; i < sizeof(stats)/sizeof(stats[0]); ++i)
	  format += " " + stats[i] + "=" + stats_format;
      else if (l->type == pe_string)
	format += "=\\\"%#s\\\"";
      else
	format += "=%#x";
      format += "\\n";

      // Output the actual printf
      code << "printf (\"" << format << "\"";

      // Feed indexes to the printf, and include them in the value
      string value = !foreach_value.empty() ? foreach_value : string(l->unmangled_name);
      if (!l->index_types.empty())
	{
	  code << "," << indexes;
          if (foreach_value.empty())
            value += "[" + indexes + "]";
	}

      // Feed the actual values to the printf
      if (l->type == pe_stats)
	for (size_t i = 0; i < sizeof(stats)/sizeof(stats[0]); ++i)
	  code << "," << stats[i] << "(" << value << ")";
      else
	code << "," << value;
      code << ")" << endl;

      // End of probe
      code << "}" << endl;

      probe *p = parse_synthetic_probe (s, code, l->tok);
      if (!p)
	throw SEMANTIC_ERROR (_("can't create global var display"), l->tok);

      vector<derived_probe*> dps;
      derive_probes (s, p, dps);
      for (unsigned i = 0; i < dps.size(); i++)
	{
	  derived_probe* dp = dps[i];
	  s.probes.push_back (dp);
	  dp->join_group (s);

          // Repopulate symbol and type info
          symresolution_info sym (s);
          sym.current_function = 0;
          sym.current_probe = dp;
          dp->body->visit (& sym);
	}

      semantic_pass_types(s);
      // Mark that variable is read
      vut.read.insert (l);
    }
}

static void monitor_mode_read(systemtap_session& s)
{
  if (!s.monitor) return;

  stringstream code;

  unsigned long rough_max_json_size = 100 +
    s.globals.size() * 100 +
    s.probes.size() * 200;
  
  code << "probe procfs(\"monitor_status\").read.maxsize(" << rough_max_json_size << ") {" << endl;
  code << "try {"; // absorb .= overflows!
  code << "elapsed = (jiffies()-__monitor_module_start)/HZ()" << endl;
  code << "hrs = elapsed/3600; mins = elapsed%3600/60; secs = elapsed%3600%60;" << endl;
  code << "$value .= sprintf(\"{\\n\")" << endl;
  code << "$value .= sprintf(\"\\\"uptime\\\": \\\"%02d:%02d:%02d\\\",\\n\", hrs, mins, secs)" << endl;
  code << "$value .= sprintf(\"\\\"uid\\\": \\\"%d\\\",\\n\", uid())" << endl;
  code << "$value .= sprintf(\"\\\"memory\\\": \\\"%s\\\",\\n\", module_size())" << endl;
  code << "$value .= sprintf(\"\\\"module_name\\\": \\\"%s\\\",\\n\", module_name())" << endl;

  code << "$value .= sprintf(\"\\\"globals\\\": {\\n\")" << endl;
  for (vector<vardecl*>::const_iterator it = s.globals.begin();
      it != s.globals.end(); ++it)
    {
      if ((*it)->synthetic) continue;

      if (it != s.globals.begin())
        code << "$value .= sprintf(\",\\n\")" << endl;

      code << "$value .= sprintf(\"\\\"%s\\\":\", \"" << (*it)->unmangled_name << "\")" << endl;
      if ((*it)->arity == 0)
        code << "$value .= string_quoted(sprint(" << (*it)->name << "))" << endl;
      else if ((*it)->arity > 0)
        code << "$value .= sprintf(\"\\\"[%d]\\\"\", " << (*it)->maxsize << ")" << endl;
    }
  code << "$value .= sprintf(\"\\n},\\n\")" << endl;

  code << "$value .= sprintf(\"\\\"probe_list\\\": [\\n\")" << endl;
  for (vector<derived_probe*>::const_iterator it = s.probes.begin();
      it != s.probes.end(); ++it)
    {
      if (it != s.probes.begin())
        code << "$value .= sprintf(\",\\n\")" << endl;

      istringstream probe_point((*it)->sole_location()->str());
      string name;
      probe_point >> name;
      /* Escape quotes once for systemtap parser and once more for json parser */
      name = lex_cast_qstring(lex_cast_qstring(name));

      code << "$value .= sprintf(\"{%s\", __private___monitor_data_function_probes("
           << it-s.probes.begin() << "))" << endl;
      code << "$value .= sprintf(\"\\\"name\\\": %s}\", " << name << ")" << endl;
    }
  code << "$value .= sprintf(\"\\n],\\n\")" << endl;

  code << "$value .= sprintf(\"}\\n\")" << endl;

  code << "} catch(ex) { warn(\"JSON construction error: \" . ex) }" << endl;
  code << "}" << endl;
  probe* p = parse_synthetic_probe(s, code, 0);
  if (!p)
    throw SEMANTIC_ERROR (_("can't create procfs probe"), 0);

  vector<derived_probe*> dps;
  derive_probes (s, p, dps);

  derived_probe* dp = dps[0];
  s.probes.push_back (dp);
  dp->join_group (s);

  // Repopulate symbol info
  symresolution_info sym (s);
  sym.current_function = 0;
  sym.current_probe = dp;
  dp->body->visit (&sym);
}

static void monitor_mode_write(systemtap_session& s)
{
  if (!s.monitor) return;

  for (vector<derived_probe*>::const_iterator it = s.probes.begin();
      it != s.probes.end()-1; ++it) // Skip monitor read probe
    {
      vardecl* v = new vardecl;
      v->unmangled_name = v->name = "__monitor_" + lex_cast(it-s.probes.begin()) + "_enabled";
      v->tok = (*it)->tok;
      v->set_arity(0, (*it)->tok);
      v->type = pe_long;
      v->init = new literal_number(1);
      v->synthetic = true;
      s.globals.push_back(v);

      symbol* sym = new symbol;
      sym->name = v->name;
      sym->tok = v->tok;
      sym->type = pe_long;
      sym->referent = v;

      if ((*it)->sole_location()->condition)
        {
          logical_and_expr *e = new logical_and_expr;
          e->tok = v->tok;
          e->left = sym;
          e->op = "&&";
          e->type = pe_long;
          e->right = (*it)->sole_location()->condition;
          (*it)->sole_location()->condition = e;
        }
      else
        {
          (*it)->sole_location()->condition = sym;
        }
    }

  stringstream code;

  code << "probe procfs(\"monitor_control\").write {" << endl;

  code << "if ($value == \"clear\") {";
  for (vector<vardecl*>::const_iterator it = s.globals.begin();
      it != s.globals.end(); ++it)
    {
      vardecl* v = *it;

      if (v->synthetic) continue;

      if (v->arity == 0 && v->init)
        {
          if (v->type == pe_long)
            {
              literal_number* ln = dynamic_cast<literal_number*>(v->init);
              code << v->name << " = " << ln->value << endl;
            }
          else if (v->type == pe_string)
            {
              literal_string* ln = dynamic_cast<literal_string*>(v->init);
              code << v->name << " = " << lex_cast_qstring(ln->value) << endl;
            }
        }
      else
        {
          // For scalar elements with no initial values, we reset to 0 or empty as
          // done with arrays and aggregates.
          code << "delete " << v->name << endl;
        }
    }

  code << "} else if ($value == \"resume\") {" << endl;
  for (vector<derived_probe*>::const_iterator it = s.probes.begin();
      it != s.probes.end()-1; ++it)
    {
      code << "  __monitor_" << it-s.probes.begin() << "_enabled" << " = 1" << endl;
    }

  code << "} else if ($value == \"pause\") {" << endl;
  for (vector<derived_probe*>::const_iterator it = s.probes.begin();
      it != s.probes.end()-1; ++it)
    {
      code << "  __monitor_" << it-s.probes.begin() << "_enabled" << " = 0" << endl;
    }
  code << "} else if ($value == \"quit\") {" << endl;
  code << "  exit()" << endl;
  code << "}";

  for (vector<derived_probe*>::const_iterator it = s.probes.begin();
      it != s.probes.end()-1; ++it)
    {
      code << "  if ($value == \"" << it-s.probes.begin() << "\")"
           << "  __monitor_" << it-s.probes.begin() << "_enabled" << " ^= 1" << endl;
    }

  code << "}" << endl;

  probe* p = parse_synthetic_probe(s, code, 0);
  if (!p)
    throw SEMANTIC_ERROR (_("can't create procfs probe"), 0);

  vector<derived_probe*> dps;
  derive_probes (s, p, dps);

  derived_probe* dp = dps[0];
  s.probes.push_back (dp);
  dp->join_group (s);

  // Repopulate symbol info
  symresolution_info sym (s, /* omniscient-unmangled */ true);
  sym.current_function = 0;
  sym.current_probe = dp;
  dp->body->visit (&sym);
}

static void create_monitor_function(systemtap_session& s)
{
  functiondecl* fd = new functiondecl;
  fd->synthetic = true;
  fd->unmangled_name = fd->name = "__private___monitor_data_function_probes";
  fd->type = pe_string;

  vardecl* v = new vardecl;
  v->type = pe_long;
  v->unmangled_name = v->name = "index";
  fd->formal_args.push_back(v);

  embeddedcode* ec = new embeddedcode;
  string code;
  code = "/* unprivileged */ /* pure */"
         "const struct stap_probe *const p = &stap_probes[STAP_ARG_index];\n"
         "if (likely (probe_timing(STAP_ARG_index))) {\n"
         "struct stat_data *stats = _stp_stat_get (probe_timing(STAP_ARG_index), 0);\n"
         "if (stats->count) {\n"
         "int64_t avg = _stp_div64 (NULL, stats->sum, stats->count);\n"
         "snprintf(_monitor_buf, STAP_MONITOR_READ,\n"
         "\"\\\"index\\\": %zu, \\\"state\\\": \\\"%s\\\", \\\"hits\\\": %lld, "
         "\\\"min\\\": %lld, \\\"avg\\\": %lld, \\\"max\\\": %lld, \",\n"
         "p->index, p->cond_enabled ? \"on\" : \"off\", (long long) stats->count,\n"
         "(long long) stats->min, (long long) avg, (long long) stats->max);\n"
         "} else {\n"
         "snprintf(_monitor_buf, STAP_MONITOR_READ,\n"
         "\"\\\"index\\\": %zu, \\\"state\\\": \\\"%s\\\", \\\"hits\\\": %d, "
         "\\\"min\\\": %d, \\\"avg\\\": %d, \\\"max\\\": %d, \",\n"
         "p->index, p->cond_enabled ? \"on\" : \"off\", 0, 0, 0, 0);}}\n"
         "STAP_RETURN(_monitor_buf);\n";
  ec->code = code;
  fd->body = ec;

  s.functions[fd->name] = fd;
}

static void monitor_mode_init(systemtap_session& s)
{
  if (!s.monitor) return;

  vardecl* v = new vardecl;
  v->unmangled_name = v->name = "__global___monitor_module_start";
  v->set_arity(0, 0);
  v->type = pe_long;
  v->synthetic = true;
  s.globals.push_back(v);

  embeddedcode* ec = new embeddedcode;
  ec->code = "#define STAP_MONITOR_READ 8192\n"
             "static char _monitor_buf[STAP_MONITOR_READ];";
  s.embeds.push_back(ec);

  create_monitor_function(s);
  monitor_mode_read(s);
  monitor_mode_write(s);

  stringstream code;
  code << "probe begin {" << endl;
  code << "__monitor_module_start = jiffies()" << endl;
  code << "}" << endl;

  probe* p = parse_synthetic_probe(s, code, 0);
  if (!p)
    throw SEMANTIC_ERROR (_("can't create begin probe"), 0);

  vector<derived_probe*> dps;
  derive_probes (s, p, dps);

  derived_probe* dp = dps[0];
  s.probes.push_back (dp);
  dp->join_group (s);

  // Repopulate symbol info
  symresolution_info sym (s);
  sym.current_function = 0;
  sym.current_probe = dp;
  dp->body->visit (&sym);
}

int
semantic_pass (systemtap_session& s)
{
  int rc = 0;

  try
    {
      // FIXME: interactive mode, register_library_aliases handles
      // both aliases from library files *and* user scripts. It would
      // be nice to have them in separate lists and register them
      // separately.
      s.register_library_aliases();
      register_standard_tapsets(s);

      if (rc == 0) rc = semantic_pass_symbols (s);
      if (rc == 0) monitor_mode_init (s);
      if (rc == 0) rc = semantic_pass_conditions (s);
      if (rc == 0) rc = semantic_pass_optimize1 (s);
      if (rc == 0) rc = semantic_pass_types (s);
      if (rc == 0) rc = gen_dfa_table(s);
      if (rc == 0) add_global_var_display (s);
      if (rc == 0) rc = semantic_pass_optimize2 (s);
      if (rc == 0) rc = semantic_pass_vars (s);
      if (rc == 0) rc = semantic_pass_stats (s);
      if (rc == 0) embeddedcode_info_pass (s);
    }
  catch (const semantic_error& e)
    {
      s.print_error (e);
      rc ++;
    }

  bool no_primary_probes = true;
  for (unsigned i = 0; i < s.probes.size(); i++)
    if (s.is_primary_probe(s.probes[i]))
      no_primary_probes = false;

  if (s.num_errors() == 0 && no_primary_probes && !s.dump_mode)
    {
      s.print_error(SEMANTIC_ERROR(_("no probes found")));
      rc ++;
    }

  build_no_more (s);

  // PR11443
  // NB: listing mode only cares whether we have any probes,
  // so all previous error conditions are disregarded.
  if (s.dump_mode == systemtap_session::dump_matched_probes ||
      s.dump_mode == systemtap_session::dump_matched_probes_vars)
    rc = no_primary_probes;

  // If we're dumping functions, only error out if no functions were found
  if (s.dump_mode == systemtap_session::dump_functions)
    rc = s.functions.empty();

  return rc;
}


// ------------------------------------------------------------------------
// semantic processing: symbol resolution


symresolution_info::symresolution_info (systemtap_session& s, bool omniscient_unmangled):
  session (s), unmangled_p(omniscient_unmangled), current_function (0), current_probe (0)
{
}


void
symresolution_info::visit_block (block* e)
{
  for (unsigned i=0; i<e->statements.size(); i++)
    {
      try
	{
	  e->statements[i]->visit (this);
	}
      catch (const semantic_error& e)
	{
	  session.print_error (e);
        }
    }
}


void
symresolution_info::visit_foreach_loop (foreach_loop* e)
{
  for (unsigned i=0; i<e->indexes.size(); i++)
    e->indexes[i]->visit (this);
  for (unsigned i=0; i<e->array_slice.size(); i++)
    if (e->array_slice[i])
      e->array_slice[i]->visit(this);

  symbol *array = NULL;
  hist_op *hist = NULL;
  classify_indexable (e->base, array, hist);

  if (array)
    {
      if (!array->referent)
	{
	  vardecl* d = find_var (array->name, e->indexes.size (), array->tok);
	  if (d)
          {
	    array->referent = d;
            array->name = d->name;
          }
	  else
	    {
	      stringstream msg;
              msg << _F("unresolved arity-%zu global array %s, missing global declaration?",
                        e->indexes.size(), array->name.to_string().c_str());
	      throw SEMANTIC_ERROR (msg.str(), array->tok);
	    }
	}

      if (!e->array_slice.empty() && e->array_slice.size() != e->indexes.size())
        {
          stringstream msg;
          msg << _F("unresolved arity-%zu global array %s, missing global declaration?",
                    e->array_slice.size(), array->name.to_string().c_str());
          throw SEMANTIC_ERROR (msg.str(), array->tok);
        }
    }
  else
    {
      assert (hist);
      hist->visit (this);
    }

  if (e->value)
    e->value->visit (this);

  if (e->limit)
    e->limit->visit (this);

  e->block->visit (this);
}


struct
delete_statement_symresolution_info:
  public traversing_visitor
{
  symresolution_info *parent;

  delete_statement_symresolution_info (symresolution_info *p):
    parent(p)
  {}

  void visit_arrayindex (arrayindex* e)
  {
    parent->visit_arrayindex(e, true);
  }

  void visit_functioncall (functioncall* e)
  {
    parent->visit_functioncall (e);
  }

  void visit_symbol (symbol* e)
  {
    if (e->referent)
      return;

    vardecl* d = parent->find_var (e->name, -1, e->tok);
    if (d)
      e->referent = d;
    else
      throw SEMANTIC_ERROR (_("unresolved array in delete statement"), e->tok);
  }
};

void
symresolution_info::visit_delete_statement (delete_statement* s)
{
  delete_statement_symresolution_info di (this);
  s->value->visit (&di);
}


void
symresolution_info::visit_symbol (symbol* e)
{
  if (e->referent)
    return;

  vardecl* d = find_var (e->name, 0, e->tok);
  if (d)
  {
    e->referent = d;
    e->name = d->name;
  }
  else
    {
      // new local
      vardecl* v = new vardecl;
      v->unmangled_name = v->name = e->name;
      v->tok = e->tok;
      v->set_arity(0, e->tok);
      if (current_function)
        current_function->locals.push_back (v);
      else if (current_probe)
        current_probe->locals.push_back (v);
      else
        // must be probe-condition expression
        throw SEMANTIC_ERROR (_("probe condition must not reference undeclared global"), e->tok);
      e->referent = v;
    }
}


void
symresolution_info::visit_arrayindex (arrayindex* e)
{
  visit_arrayindex(e, false);
}

void
symresolution_info::visit_arrayindex (arrayindex* e, bool wildcard_ok)
{
  for (unsigned i=0; i<e->indexes.size(); i++)
    {
      // assuming that if NULL, it was originally a wildcard (*)
      if (e->indexes[i] == NULL)
        {
          if (!wildcard_ok)
            throw SEMANTIC_ERROR(_("wildcard not allowed in array index"), e->tok);
        }
      else
        e->indexes[i]->visit (this);
    }

  symbol *array = NULL;
  hist_op *hist = NULL;
  classify_indexable(e->base, array, hist);

  if (array)
    {
      if (array->referent)
	return;

      vardecl* d = find_var (array->name, e->indexes.size (), array->tok);
      if (d)
      {
	array->referent = d;
        array->name = d->name;
      }
      else
	{
	  stringstream msg;
          msg << _F("unresolved arity-%zu global array %s, missing global declaration?",
                    e->indexes.size(), array->name.to_string().c_str());
	  throw SEMANTIC_ERROR (msg.str(), e->tok);
	}
    }
  else
    {
      assert (hist);
      hist->visit (this);
    }
}


void
symresolution_info::visit_array_in (array_in* e)
{
  visit_arrayindex(e->operand, true);
}


void
symresolution_info::visit_functioncall (functioncall* e)
{
  // XXX: we could relax this, if we're going to examine the
  // vartracking data recursively.  See testsuite/semko/fortytwo.stp.
  if (! (current_function || current_probe))
    {
      // must be probe-condition expression
      throw SEMANTIC_ERROR (_("probe condition must not reference function"), e->tok);
    }

  for (unsigned i=0; i<e->args.size(); i++)
    e->args[i]->visit (this);

  if (!e->referents.empty())
    return;

  vector<functiondecl*> fds = find_functions (e->function, e->args.size (), e->tok);
  if (!fds.empty())
    {
      e->referents = fds;
      function_priority_order order;
      stable_sort(e->referents.begin(), e->referents.end(), order); // preserve declaration order
      e->function = e->referents[0]->name;
    }
  else
    {
      string sugs = levenshtein_suggest(e->function, collect_functions(), 5); // print 5 funcs
      throw SEMANTIC_ERROR(_F("unresolved function%s",
                              sugs.empty() ? "" : (_(" (similar: ") + sugs + ")").c_str()),
                           e->tok);
    }

  // In monitor mode, tapset functions used in the synthetic probe are not resolved and added
  // to the master list at the same time as the other functions so we must add them here to
  // allow the translator to generate the functions in the module.
  if (session.monitor && session.functions.find(e->function) == session.functions.end())
    session.functions[e->function] = fds[0]; // no overload
}

/*find_var will return an argument other than zero if the name matches the var
 * name ie, if the current local name matches the name passed to find_var*/
vardecl*
symresolution_info::find_var (interned_string name, int arity, const token* tok)
{
  if (current_function || current_probe)
    {
      // search locals
      vector<vardecl*>& locals = (current_function ?
                                  current_function->locals :
                                  current_probe->locals);


      for (unsigned i=0; i<locals.size(); i++)
        if (locals[i]->name == name)
          {
            locals[i]->set_arity (arity, tok);
            return locals[i];
          }
    }

  // search function formal parameters (for scalars)
  if (arity == 0 && current_function)
    for (unsigned i=0; i<current_function->formal_args.size(); i++)
      if (current_function->formal_args[i]->name == name)
	{
	  // NB: no need to check arity here: formal args always scalar
	  current_function->formal_args[i]->set_arity (0, tok);
	  return current_function->formal_args[i];
	}

  // search processed globals
  string gname, pname;
  if (unmangled_p)
    {
      gname = pname = string(name);
    }
  else
    {
      gname = "__global_" + string(name);
      pname = "__private_" + detox_path(tok->location.file->name) + string(name);
    }
  for (unsigned i=0; i<session.globals.size(); i++)
  {
    if ((session.globals[i]->name == name && startswith(name, "__global_")) ||
        (session.globals[i]->name == gname) ||
        (session.globals[i]->name == pname))
      {
        if (! session.suppress_warnings)
          {
            vardecl* v = session.globals[i];
	    stapfile* f = tok->location.file;
            // clog << "resolved " << *tok << " to global " << *v->tok << endl;
            if (v->tok && v->tok->location.file != f && !f->synthetic)
              {
                session.print_warning (_F("cross-file global variable reference to %s from",
                                          lex_cast(*v->tok).c_str()), tok);
              }
          }
        session.globals[i]->set_arity (arity, tok);
        return session.globals[i];
      }
  }

  // search library globals
  for (unsigned i=0; i<session.library_files.size(); i++)
    {
      stapfile* f = session.library_files[i];
      for (unsigned j=0; j<f->globals.size(); j++)
        {
          vardecl* g = f->globals[j];
          if (g->name == gname)
            {
	      g->set_arity (arity, tok);

              // put library into the queue if not already there
              if (find (session.files.begin(), session.files.end(), f)
                  == session.files.end())
                session.files.push_back (f);

              return g;
            }
        }
    }

  return 0;
}


vector<functiondecl*>
symresolution_info::find_functions (const string& name, unsigned arity, const token *tok)
{
  vector<functiondecl*> functions;
  functiondecl* last = 0; // used for error message

  // the common path

  // internal global functions bypassing the parser, such as __global_dwarf_tvar_[gs]et
  if ((session.functions.find(name) != session.functions.end()) && startswith(name, "__private_"))
    {
      functiondecl* fd = session.functions[name];
      assert (fd->name == name);
      if (fd->formal_args.size() == arity)
        functions.push_back(fd);
      else
        last = fd;
    }

  // functions scanned by the parser are overloaded
  unsigned alternatives = session.overload_count[name];
  for (unsigned alt = 0; alt < alternatives; alt++)
    {
      bool found = false; // multiple inclusion guard
      string gname = "__global_" + string(name) + "__overload_" + lex_cast(alt);
      string pname = "__private_" + detox_path(tok->location.file->name) + string(name) +
        "__overload_" + lex_cast(alt);

      // tapset or user script global functions coming from the parser
      if (!found && session.functions.find(gname) != session.functions.end())
        {
          functiondecl* fd = session.functions[gname];
          assert (fd->name == gname);
          if (fd->formal_args.size() == arity)
            {
              functions.push_back(fd);
              found = true;
            }
          else
            last = fd;
        }

      // tapset or user script private functions coming from the parser
      if (!found && session.functions.find(pname) != session.functions.end())
        {
          functiondecl* fd = session.functions[pname];
          assert (fd->name == pname);
          if (fd->formal_args.size() == arity)
            {
              functions.push_back(fd);
              found = true;
            }
          else
            last = fd;
        }

      // search library functions
      for (unsigned i=0; !found && i<session.library_files.size(); i++)
        {
          stapfile* f = session.library_files[i];
          for (unsigned j=0; !found && j<f->functions.size(); j++)
          {
            if ((f->functions[j]->name == gname) ||
                (f->functions[j]->name == pname))
              {
                if (f->functions[j]->formal_args.size() == arity)
                  {
                    // put library into the queue if not already there
                    if (0) // session.verbose_resolution
                      cerr << _F("      function %s is defined from %s",
                                 name.c_str(), f->name.c_str()) << endl;

                    if (find (session.files.begin(), session.files.end(), f)
                        == session.files.end())
                      session.files.push_back (f);
                    // else .. print different message?

                    functions.push_back(f->functions[j]);
                    found = true;
                  }
                else
                  last = f->functions[j];
              }
          }
        }
    }

  // suggest last found function with matching name
  if (last && functions.empty())
    {
      throw SEMANTIC_ERROR(_F("arity mismatch found (function '%s' takes %zu args)",
                              name.c_str(), last->formal_args.size()), tok, last->tok);
    }

  return functions;
}

set<string>
symresolution_info::collect_functions(void)
{
  set<string> funcs;

  for (map<string,functiondecl*>::const_iterator it = session.functions.begin();
       it != session.functions.end(); ++it)
    funcs.insert(it->second->unmangled_name);

  // search library functions
  for (unsigned i=0; i<session.library_files.size(); i++)
    {
      stapfile* f = session.library_files[i];
      for (unsigned j=0; j<f->functions.size(); j++)
        funcs.insert(f->functions[j]->unmangled_name);
    }

  return funcs;
}

// ------------------------------------------------------------------------
// optimization


// Do away with functiondecls that are never (transitively) called
// from probes.
void semantic_pass_opt1 (systemtap_session& s, bool& relaxed_p)
{
  functioncall_traversing_visitor ftv;
  for (unsigned i=0; i<s.probes.size(); i++)
    {
      s.probes[i]->body->visit (& ftv);
      if (s.probes[i]->sole_location()->condition)
        s.probes[i]->sole_location()->condition->visit (& ftv);
    }
  vector<functiondecl*> new_unused_functions;
  for (map<string,functiondecl*>::iterator it = s.functions.begin(); it != s.functions.end(); it++)
    {
      functiondecl* fd = it->second;
      if (ftv.seen.find(fd) == ftv.seen.end())
        {
          if (! fd->synthetic && s.is_user_file(fd->tok->location.file->name))
            s.print_warning (_F("Eliding unused function '%s'",
                                fd->unmangled_name.to_string().c_str()),
			     fd->tok);
          // s.functions.erase (it); // NB: can't, since we're already iterating upon it
          new_unused_functions.push_back (fd);
          relaxed_p = false;
        }
    }
  for (unsigned i=0; i<new_unused_functions.size(); i++)
    {
      map<string,functiondecl*>::iterator where = s.functions.find (new_unused_functions[i]->name);
      assert (where != s.functions.end());
      s.functions.erase (where);
      if (s.tapset_compile_coverage)
        s.unused_functions.push_back (new_unused_functions[i]);
    }
}


// ------------------------------------------------------------------------

// Do away with local & global variables that are never
// written nor read.
void semantic_pass_opt2 (systemtap_session& s, bool& relaxed_p, unsigned iterations)
{
  varuse_collecting_visitor vut(s);

  for (unsigned i=0; i<s.probes.size(); i++)
    {
      s.probes[i]->body->visit (& vut);

      if (s.probes[i]->sole_location()->condition)
        s.probes[i]->sole_location()->condition->visit (& vut);
    }

  // NB: Since varuse_collecting_visitor also traverses down
  // actually called functions, we don't need to explicitly
  // iterate over them.  Uncalled ones should have been pruned
  // in _opt1 above.
  //
  // for (unsigned i=0; i<s.functions.size(); i++)
  //   s.functions[i]->body->visit (& vut);

  // Now in vut.read/written, we have a mixture of all locals, globals

  for (unsigned i=0; i<s.probes.size(); i++)
    for (unsigned j=0; j<s.probes[i]->locals.size(); /* see below */)
      {
        vardecl* l = s.probes[i]->locals[j];

        // skip over "special" locals
        if (l->synthetic) { j++; continue; }

        if (vut.read.find (l) == vut.read.end() &&
            vut.written.find (l) == vut.written.end())
          {
            if (s.is_user_file(l->tok->location.file->name))
              s.print_warning (_F("Eliding unused variable '%s'",
                                  l->unmangled_name.to_string().c_str()),
			       l->tok);
	    if (s.tapset_compile_coverage) {
	      s.probes[i]->unused_locals.push_back
		      (s.probes[i]->locals[j]);
	    }
            s.probes[i]->locals.erase(s.probes[i]->locals.begin() + j);
            relaxed_p = false;
            // don't increment j
          }
        else
          {
            if (vut.written.find (l) == vut.written.end())
              if (iterations == 0 && ! s.suppress_warnings)
                {
                  set<string> vars;
                  vector<vardecl*>::iterator it;
                  for (it = s.probes[i]->locals.begin(); it != s.probes[i]->locals.end(); it++)
                    vars.insert((*it)->unmangled_name);
                  for (it = s.globals.begin(); it != s.globals.end(); it++)
                    vars.insert((*it)->unmangled_name);

                  vars.erase(l->name);
                  string sugs = levenshtein_suggest(l->name, vars, 5); // suggest top 5 vars
                  s.print_warning (_F("never-assigned local variable '%s'%s",
                                      l->unmangled_name.to_string().c_str(),
				      (sugs.empty() ? "" :
				       (_(" (similar: ") + sugs + ")")).c_str()), l->tok);
                }
            j++;
          }
      }

  for (map<string,functiondecl*>::iterator it = s.functions.begin(); it != s.functions.end(); it++)
    {
      functiondecl *fd = it->second;
      for (unsigned j=0; j<fd->locals.size(); /* see below */)
        {
          vardecl* l = fd->locals[j];
          if (vut.read.find (l) == vut.read.end() &&
              vut.written.find (l) == vut.written.end())
            {
              if (s.is_user_file(l->tok->location.file->name))
                s.print_warning (_F("Eliding unused variable '%s'",
                                    l->unmangled_name.to_string().c_str()),
				 l->tok);
              if (s.tapset_compile_coverage) {
                fd->unused_locals.push_back (fd->locals[j]);
              }
              fd->locals.erase(fd->locals.begin() + j);
              relaxed_p = false;
              // don't increment j
            }
          else
            {
              if (vut.written.find (l) == vut.written.end())
                if (iterations == 0 && ! s.suppress_warnings)
                  {
                    set<string> vars;
                    vector<vardecl*>::iterator it;
                    for (it = fd->formal_args.begin() ;
                         it != fd->formal_args.end(); it++)
                        vars.insert((*it)->unmangled_name);
                    for (it = fd->locals.begin(); it != fd->locals.end(); it++)
                        vars.insert((*it)->unmangled_name);
                    for (it = s.globals.begin(); it != s.globals.end(); it++)
                        vars.insert((*it)->unmangled_name);

                    vars.erase(l->name);
                    string sugs = levenshtein_suggest(l->name, vars, 5); // suggest top 5 vars
                    s.print_warning (_F("never-assigned local variable '%s'%s",
                                        l->unmangled_name.to_string().c_str(),
					(sugs.empty() ? "" :
					 (_(" (similar: ") + sugs + ")")).c_str()), l->tok);
                  }

              j++;
            }
        }
    }
  for (unsigned i=0; i<s.globals.size(); /* see below */)
    {
      vardecl* l = s.globals[i];
      if (vut.read.find (l) == vut.read.end() &&
          vut.written.find (l) == vut.written.end())
        {
          if (s.is_user_file(l->tok->location.file->name)) 
            s.print_warning (_F("Eliding unused variable '%s'",
                                l->unmangled_name.to_string().c_str()),
			     l->tok);
	  if (s.tapset_compile_coverage) {
	    s.unused_globals.push_back(s.globals[i]);
	  }
	  s.globals.erase(s.globals.begin() + i);
	  relaxed_p = false;
	  // don't increment i
        }
      else
        {
          if (vut.written.find (l) == vut.written.end() && ! l->init) // no initializer
            if (iterations == 0 && ! s.suppress_warnings)
              {
                set<string> vars;
                vector<vardecl*>::iterator it;
                for (it = s.globals.begin(); it != s.globals.end(); it++)
                  if (l->name != (*it)->unmangled_name)
                    vars.insert((*it)->unmangled_name);

                string sugs = levenshtein_suggest(l->name, vars, 5); // suggest top 5 vars
                s.print_warning (_F("never-assigned global variable '%s'%s",
                                    l->unmangled_name.to_string().c_str(),
				    (sugs.empty() ? "" :
				     (_(" (similar: ") + sugs + ")")).c_str()),
				 l->tok);
              }

          i++;
        }
    }
}


// ------------------------------------------------------------------------

struct dead_assignment_remover: public update_visitor
{
  systemtap_session& session;
  bool& relaxed_p;
  const varuse_collecting_visitor& vut;

  dead_assignment_remover(systemtap_session& s, bool& r,
                          const varuse_collecting_visitor& v):
    session(s), relaxed_p(r), vut(v) {}

  void visit_assignment (assignment* e);
  void visit_try_block (try_block *s);
};


// symbol_fetcher augmented to allow target-symbol types, but NULLed.
struct assignment_symbol_fetcher
  : public symbol_fetcher
{
  assignment_symbol_fetcher (symbol *&sym): symbol_fetcher(sym)
  {}

  void visit_target_symbol (target_symbol*)
  {
    sym = NULL;
  }

  void visit_atvar_op (atvar_op*)
  {
    sym = NULL;
  }

  void visit_cast_op (cast_op*)
  {
    sym = NULL;
  }

  void visit_autocast_op (autocast_op*)
  {
    sym = NULL;
  }

  void throwone (const token* t)
  {
    if (t->type == tok_operator && t->content == ".")
      // guess someone misused . in $foo->bar.baz expression
      // XXX why are we only checking this in lvalues?
      throw SEMANTIC_ERROR (_("Expecting lvalue expression, try -> instead"), t);
    else
      throw SEMANTIC_ERROR (_("Expecting lvalue expression"), t);
  }
};

symbol *
get_assignment_symbol_within_expression (expression *e)
{
  symbol *sym = NULL;
  assignment_symbol_fetcher fetcher(sym);
  e->visit (&fetcher);
  return sym; // NB: may be null!
}


void
dead_assignment_remover::visit_assignment (assignment* e)
{
  replace (e->left);
  replace (e->right);

  symbol* left = get_assignment_symbol_within_expression (e->left);
  if (left) // not unresolved $target, so intended sideeffect may be elided
    {
      vardecl* leftvar = left->referent;
      if (vut.read.find(leftvar) == vut.read.end()) // var never read?
        {
          // NB: Not so fast!  The left side could be an array whose
          // index expressions may have side-effects.  This would be
          // OK if we could replace the array assignment with a
          // statement-expression containing all the index expressions
          // and the rvalue... but we can't.
	  // Another possibility is that we have an unread global variable
	  // which are kept for probe end value display.

	  bool is_global = false;
	  vector<vardecl*>::iterator it;
	  for (it = session.globals.begin(); it != session.globals.end(); it++)
	    if (leftvar->name == (*it)->name)
	      {
		is_global = true;
		break;
	      }

          varuse_collecting_visitor lvut(session);
          e->left->visit (& lvut);
          if (lvut.side_effect_free () && !is_global // XXX: use _wrt() once we track focal_vars
              && !leftvar->synthetic) // don't elide assignment to synthetic $context variables
            {
              /* PR 1119: NB: This is not necessary here.  A write-only
                 variable will also be elided soon at the next _opt2 iteration.
              if (e->left->tok->location.file->name == session.user_file->name) // !tapset
                session.print_warning("eliding write-only ", *e->left->tok);
              else
              */
              if (session.is_user_file(e->left->tok->location.file->name)) 
                session.print_warning(_F("Eliding assignment to '%s'",
                                         leftvar->unmangled_name.to_string().c_str()), e->tok);
              provide (e->right); // goodbye assignment*
              relaxed_p = false;
              return;
            }
        }
    }
  provide (e);
}


void
dead_assignment_remover::visit_try_block (try_block *s)
{
  replace (s->try_block);
  if (s->catch_error_var)
    {
      vardecl* errvar = s->catch_error_var->referent;
      if (vut.read.find(errvar) == vut.read.end()) // never read?
        {
          if (session.verbose>2)
            clog << _F("Eliding unused error string catcher %s at %s",
		       errvar->unmangled_name.to_string().c_str(),
		       lex_cast(*s->tok).c_str()) << endl;
          s->catch_error_var = 0;
        }
    }
  replace (s->catch_block);
  provide (s);
}


// Let's remove assignments to variables that are never read.  We
// rewrite "(foo = expr)" as "(expr)".  This makes foo a candidate to
// be optimized away as an unused variable, and expr a candidate to be
// removed as a side-effect-free statement expression.  Wahoo!
void semantic_pass_opt3 (systemtap_session& s, bool& relaxed_p)
{
  // Recompute the varuse data, which will probably match the opt2
  // copy of the computation, except for those totally unused
  // variables that opt2 removed.
  varuse_collecting_visitor vut(s);
  for (unsigned i=0; i<s.probes.size(); i++)
    s.probes[i]->body->visit (& vut); // includes reachable functions too

  dead_assignment_remover dar (s, relaxed_p, vut);
  // This instance may be reused for multiple probe/function body trims.

  for (unsigned i=0; i<s.probes.size(); i++)
    dar.replace (s.probes[i]->body);
  for (map<string,functiondecl*>::iterator it = s.functions.begin();
       it != s.functions.end(); it++)
    dar.replace (it->second->body);
  // The rewrite operation is performed within the visitor.

  // XXX: we could also zap write-only globals here
}


// ------------------------------------------------------------------------

struct dead_stmtexpr_remover: public update_visitor
{
  systemtap_session& session;
  bool& relaxed_p;
  set<vardecl*> focal_vars; // vars considered subject to side-effects

  dead_stmtexpr_remover(systemtap_session& s, bool& r):
    session(s), relaxed_p(r) {}

  void visit_block (block *s);
  void visit_try_block (try_block *s);
  void visit_null_statement (null_statement *s);
  void visit_if_statement (if_statement* s);
  void visit_foreach_loop (foreach_loop *s);
  void visit_for_loop (for_loop *s);
  // XXX: and other places where stmt_expr's might be nested

  void visit_expr_statement (expr_statement *s);
};


void
dead_stmtexpr_remover::visit_null_statement (null_statement *s)
{
  // easy!
  if (session.verbose>2)
    clog << _("Eliding side-effect-free null statement ") << *s->tok << endl;
  s = 0;
  provide (s);
}


void
dead_stmtexpr_remover::visit_block (block *s)
{
  vector<statement*> new_stmts;
  for (unsigned i=0; i<s->statements.size(); i++ )
    {
      statement* new_stmt = require (s->statements[i], true);
      if (new_stmt != 0)
        {
          // flatten nested blocks into this one
          block *b = dynamic_cast<block *>(new_stmt);
          if (b)
            {
              if (session.verbose>2)
                clog << _("Flattening nested block ") << *b->tok << endl;
              new_stmts.insert(new_stmts.end(),
                  b->statements.begin(), b->statements.end());
              relaxed_p = false;
            }
          else
            new_stmts.push_back (new_stmt);
        }
    }
  if (new_stmts.size() == 0)
    {
      if (session.verbose>2)
        clog << _("Eliding side-effect-free empty block ") << *s->tok << endl;
      s = 0;
    }
  else if (new_stmts.size() == 1)
    {
      if (session.verbose>2)
        clog << _("Eliding side-effect-free singleton block ") << *s->tok << endl;
      provide (new_stmts[0]);
      return;
    }
  else
    s->statements = new_stmts;
  provide (s);
}


void
dead_stmtexpr_remover::visit_try_block (try_block *s)
{
  replace (s->try_block, true);
  replace (s->catch_block, true); // null catch{} is ok and useful
  if (s->try_block == 0)
    {
      if (session.verbose>2)
        clog << _("Eliding empty try {} block ") << *s->tok << endl;
      s = 0;
    }
  provide (s);
}


void
dead_stmtexpr_remover::visit_if_statement (if_statement *s)
{
  replace (s->thenblock, true);
  replace (s->elseblock, true);

  if (s->thenblock == 0)
    {
      if (s->elseblock == 0)
        {
          // We may be able to elide this statement, if the condition
          // expression is side-effect-free.
          varuse_collecting_visitor vct(session);
          s->condition->visit(& vct);
          if (vct.side_effect_free ())
            {
              if (session.verbose>2)
                clog << _("Eliding side-effect-free if statement ")
                     << *s->tok << endl;
              s = 0; // yeah, baby
            }
          else
            {
              // We can still turn it into a simple expr_statement though...
              if (session.verbose>2)
                clog << _("Creating simple evaluation from if statement ")
                     << *s->tok << endl;
              expr_statement *es = new expr_statement;
              es->value = s->condition;
              es->tok = es->value->tok;
              provide (es);
              return;
            }
        }
      else
        {
          // For an else without a then, we can invert the condition logic to
          // avoid having a null statement in the thenblock
          if (session.verbose>2)
            clog << _("Inverting the condition of if statement ")
                 << *s->tok << endl;
          unary_expression *ue = new unary_expression;
          ue->operand = s->condition;
          ue->tok = ue->operand->tok;
          ue->op = "!";
          s->condition = ue;
          s->thenblock = s->elseblock;
          s->elseblock = 0;
        }
    }
  provide (s);
}

void
dead_stmtexpr_remover::visit_foreach_loop (foreach_loop *s)
{
  replace (s->block, true);

  if (s->block == 0)
    {
      // XXX what if s->limit has side effects?
      // XXX what about s->indexes or s->value used outside the loop?
      if(session.verbose > 2)
        clog << _("Eliding side-effect-free foreach statement ") << *s->tok << endl;
      s = 0; // yeah, baby
    }
  provide (s);
}

void
dead_stmtexpr_remover::visit_for_loop (for_loop *s)
{
  replace (s->block, true);

  if (s->block == 0)
    {
      // We may be able to elide this statement, if the condition
      // expression is side-effect-free.
      varuse_collecting_visitor vct(session);
      if (s->init) s->init->visit(& vct);
      s->cond->visit(& vct);
      if (s->incr) s->incr->visit(& vct);
      if (vct.side_effect_free ())
        {
          if (session.verbose>2)
            clog << _("Eliding side-effect-free for statement ") << *s->tok << endl;
          s = 0; // yeah, baby
        }
      else
        {
          // Can't elide this whole statement; put a null in there.
          s->block = new null_statement(s->tok);
        }
    }
  provide (s);
}



void
dead_stmtexpr_remover::visit_expr_statement (expr_statement *s)
{
  // Run a varuse query against the operand expression.  If it has no
  // side-effects, replace the entire statement expression by a null
  // statement with the provide() call.
  //
  // Unlike many other visitors, we do *not* traverse this outermost
  // one into the expression subtrees.  There is no need - no
  // expr_statement nodes will be found there.  (Function bodies
  // need to be visited explicitly by our caller.)
  //
  // NB.  While we don't share nodes in the parse tree, let's not
  // deallocate *s anyway, just in case...

  varuse_collecting_visitor vut(session);
  s->value->visit (& vut);

  if (vut.side_effect_free_wrt (focal_vars))
    {
      /* PR 1119: NB: this message is not a good idea here.  It can
         name some arbitrary RHS expression of an assignment.
      if (s->value->tok->location.file->name == session.user_file->name) // not tapset
        session.print_warning("eliding never-assigned ", *s->value->tok);
      else
      */
      if (session.is_user_file(s->value->tok->location.file->name))
        session.print_warning("Eliding side-effect-free expression ", s->tok);

      // NB: this 0 pointer is invalid to leave around for any length of
      // time, but the parent parse tree objects above handle it.
      s = 0;
      relaxed_p = false;
    }
  provide (s);
}


void semantic_pass_opt4 (systemtap_session& s, bool& relaxed_p)
{
  // Finally, let's remove some statement-expressions that have no
  // side-effect.  These should be exactly those whose private varuse
  // visitors come back with an empty "written" and "embedded" lists.

  dead_stmtexpr_remover duv (s, relaxed_p);
  // This instance may be reused for multiple probe/function body trims.

  for (unsigned i=0; i<s.probes.size(); i++)
    {
      assert_no_interrupts();

      derived_probe* p = s.probes[i];

      duv.focal_vars.clear ();
      duv.focal_vars.insert (s.globals.begin(),
                             s.globals.end());
      duv.focal_vars.insert (p->locals.begin(),
                             p->locals.end());

      duv.replace (p->body, true);
      if (p->body == 0)
        {
          if (! s.timing && // PR10070
              !(p->base->tok->location.file->synthetic)) // don't warn for synthetic probes
            s.print_warning (_F("side-effect-free probe '%s'",
                                p->name().c_str()), p->tok);

          p->body = new null_statement(p->tok);

          // XXX: possible duplicate warnings; see below
        }
    }
  for (map<string,functiondecl*>::iterator it = s.functions.begin(); it != s.functions.end(); it++)
    {
      assert_no_interrupts();

      functiondecl* fn = it->second;
      duv.focal_vars.clear ();
      duv.focal_vars.insert (fn->locals.begin(),
                             fn->locals.end());
      duv.focal_vars.insert (fn->formal_args.begin(),
                             fn->formal_args.end());
      duv.focal_vars.insert (s.globals.begin(),
                             s.globals.end());

      duv.replace (fn->body, true);
      if (fn->body == 0)
        {
          s.print_warning (_F("side-effect-free function '%s'",
                              fn->unmangled_name.to_string().c_str()),
			   fn->tok);

          fn->body = new null_statement(fn->tok);

          // XXX: the next iteration of the outer optimization loop may
          // take this new null_statement away again, and thus give us a
          // fresh warning.  It would be better if this fixup was performed
          // only after the relaxation iterations.
          // XXX: or else see bug #6469.
        }
    }
}


// ------------------------------------------------------------------------

// The goal of this visitor is to reduce top-level expressions in void context
// into separate statements that evaluate each subcomponent of the expression.
// The dead-statement-remover can later remove some parts if they have no side
// effects.
//
// All expressions must be overridden here so we never visit their subexpressions
// accidentally.  Thus, the only visited expressions should be value of an
// expr_statement.
//
// For an expression to replace its expr_statement with something else, it will
// let the new statement provide(), and then provide(0) for itself.  The
// expr_statement will take this as a sign that it's been replaced.
struct void_statement_reducer: public update_visitor
{
  systemtap_session& session;
  bool& relaxed_p;
  set<vardecl*> focal_vars; // vars considered subject to side-effects

  void_statement_reducer(systemtap_session& s, bool& r):
    session(s), relaxed_p(r) {}

  void visit_expr_statement (expr_statement* s);

  // expressions in conditional / loop controls are definitely a side effect,
  // but still recurse into the child statements
  void visit_if_statement (if_statement* s);
  void visit_for_loop (for_loop* s);
  void visit_foreach_loop (foreach_loop* s);

  // these expressions get rewritten into their statement equivalents
  void visit_logical_or_expr (logical_or_expr* e);
  void visit_logical_and_expr (logical_and_expr* e);
  void visit_ternary_expression (ternary_expression* e);

  // all of these can (usually) be reduced into simpler statements
  void visit_binary_expression (binary_expression* e);
  void visit_unary_expression (unary_expression* e);
  void visit_regex_query (regex_query* e); // XXX depends on subexpr extraction
  void visit_comparison (comparison* e);
  void visit_concatenation (concatenation* e);
  void visit_functioncall (functioncall* e);
  void visit_print_format (print_format* e);
  void visit_target_symbol (target_symbol* e);
  void visit_atvar_op (atvar_op* e);
  void visit_cast_op (cast_op* e);
  void visit_autocast_op (autocast_op* e);
  void visit_defined_op (defined_op* e);

  // these are a bit hairy to grok due to the intricacies of indexables and
  // stats, so I'm chickening out and skipping them...
  void visit_array_in (array_in* e) { provide (e); }
  void visit_arrayindex (arrayindex* e) { provide (e); }
  void visit_stat_op (stat_op* e) { provide (e); }
  void visit_hist_op (hist_op* e) { provide (e); }

  // these can't be reduced because they always have an effect
  void visit_return_statement (return_statement* s) { provide (s); }
  void visit_delete_statement (delete_statement* s) { provide (s); }
  void visit_pre_crement (pre_crement* e) { provide (e); }
  void visit_post_crement (post_crement* e) { provide (e); }
  void visit_assignment (assignment* e) { provide (e); }

private:
  void reduce_target_symbol (target_symbol* e, expression* operand=NULL);
};


void
void_statement_reducer::visit_expr_statement (expr_statement* s)
{
  replace (s->value, true);

  // if the expression provides 0, that's our signal that a new
  // statement has been provided, so we shouldn't provide this one.
  if (s->value != 0)
    provide(s);
}

void
void_statement_reducer::visit_if_statement (if_statement* s)
{
  // s->condition is never void
  replace (s->thenblock);
  replace (s->elseblock);
  provide (s);
}

void
void_statement_reducer::visit_for_loop (for_loop* s)
{
  // s->init/cond/incr are never void
  replace (s->block);
  provide (s);
}

void
void_statement_reducer::visit_foreach_loop (foreach_loop* s)
{
  // s->indexes/base/value/limit are never void
  replace (s->block);
  provide (s);
}

void
void_statement_reducer::visit_logical_or_expr (logical_or_expr* e)
{
  // In void context, the evaluation of "a || b" is exactly like
  // "if (!a) b", so let's do that instead.

  if (session.verbose>2)
    clog << _("Creating if statement from unused logical-or ")
         << *e->tok << endl;

  if_statement *is = new if_statement;
  is->tok = e->tok;
  is->elseblock = 0;

  unary_expression *ue = new unary_expression;
  ue->operand = e->left;
  ue->tok = e->tok;
  ue->op = "!";
  is->condition = ue;

  expr_statement *es = new expr_statement;
  es->value = e->right;
  es->tok = es->value->tok;
  is->thenblock = es;

  is->visit(this);
  relaxed_p = false;
  e = 0;
  provide (e);
}

void
void_statement_reducer::visit_logical_and_expr (logical_and_expr* e)
{
  // In void context, the evaluation of "a && b" is exactly like
  // "if (a) b", so let's do that instead.

  if (session.verbose>2)
    clog << _("Creating if statement from unused logical-and ")
         << *e->tok << endl;

  if_statement *is = new if_statement;
  is->tok = e->tok;
  is->elseblock = 0;
  is->condition = e->left;

  expr_statement *es = new expr_statement;
  es->value = e->right;
  es->tok = es->value->tok;
  is->thenblock = es;

  is->visit(this);
  relaxed_p = false;
  e = 0;
  provide (e);
}

void
void_statement_reducer::visit_ternary_expression (ternary_expression* e)
{
  // In void context, the evaluation of "a ? b : c" is exactly like
  // "if (a) b else c", so let's do that instead.

  if (session.verbose>2)
    clog << _("Creating if statement from unused ternary expression ")
         << *e->tok << endl;

  if_statement *is = new if_statement;
  is->tok = e->tok;
  is->condition = e->cond;

  expr_statement *es = new expr_statement;
  es->value = e->truevalue;
  es->tok = es->value->tok;
  is->thenblock = es;

  es = new expr_statement;
  es->value = e->falsevalue;
  es->tok = es->value->tok;
  is->elseblock = es;

  is->visit(this);
  relaxed_p = false;
  e = 0;
  provide (e);
}

void
void_statement_reducer::visit_binary_expression (binary_expression* e)
{
  // When the result of a binary operation isn't needed, it's just as good to
  // evaluate the operands as sequential statements in a block.

  if (session.verbose>2)
    clog << _("Eliding unused binary ") << *e->tok << endl;

  block *b = new block;
  b->tok = e->tok;

  expr_statement *es = new expr_statement;
  es->value = e->left;
  es->tok = es->value->tok;
  b->statements.push_back(es);

  es = new expr_statement;
  es->value = e->right;
  es->tok = es->value->tok;
  b->statements.push_back(es);

  b->visit(this);
  relaxed_p = false;
  e = 0;
  provide (e);
}

void
void_statement_reducer::visit_unary_expression (unary_expression* e)
{
  // When the result of a unary operation isn't needed, it's just as good to
  // evaluate the operand directly

  if (session.verbose>2)
    clog << _("Eliding unused unary ") << *e->tok << endl;

  relaxed_p = false;
  e->operand->visit(this);
}

void
void_statement_reducer::visit_regex_query (regex_query* e)
{
  // TODOXXX After subexpression extraction is implemented,
  // regular expression matches *may* have side-effects in
  // terms of producing matched subexpressions, e.g.:
  //
  //   str =~ "pat"; println(matched(0));
  //
  // It's debatable if we want to actually allow this, though.

  // Treat e as a unary expression on the left operand -- since the
  // right hand side must be a literal (as verified by the parser),
  // evaluating it never has side effects.

  if (session.verbose>2)
    clog << _("Eliding regex query ") << *e->tok << endl;

  relaxed_p = false;
  e->left->visit(this);
}

void
void_statement_reducer::visit_comparison (comparison* e)
{
  visit_binary_expression(e);
}

void
void_statement_reducer::visit_concatenation (concatenation* e)
{
  visit_binary_expression(e);
}

void
void_statement_reducer::visit_functioncall (functioncall* e)
{
  // If a function call is pure and its result ignored, we can elide the call
  // and just evaluate the arguments in sequence

  if (e->args.empty())
    {
      provide (e);
      return;
    }

  bool side_effect_free = true;
  for (unsigned i = 0; i < e->referents.size(); i++)
    {
      varuse_collecting_visitor vut(session);
      vut.seen.insert (e->referents[i]);
      vut.current_function = e->referents[i];
      e->referents[i]->body->visit (& vut);
      if (!vut.side_effect_free_wrt(focal_vars))
        {
          side_effect_free = false;
          break;
        }
    }

  if (!side_effect_free)
    {
      provide (e);
      return;
    }

  if (session.verbose>2)
    clog << _("Eliding side-effect-free function call ") << *e->tok << endl;

  block *b = new block;
  b->tok = e->tok;

  for (unsigned i=0; i<e->args.size(); i++ )
    {
      expr_statement *es = new expr_statement;
      es->value = e->args[i];
      es->tok = es->value->tok;
      b->statements.push_back(es);
    }

  b->visit(this);
  relaxed_p = false;
  e = 0;
  provide (e);
}

void
void_statement_reducer::visit_print_format (print_format* e)
{
  // When an sprint's return value is ignored, we can simply evaluate the
  // arguments in sequence

  if (e->print_to_stream || !e->args.size())
    {
      provide (e);
      return;
    }

  if (session.verbose>2)
    clog << _("Eliding unused print ") << *e->tok << endl;

  block *b = new block;
  b->tok = e->tok;

  for (unsigned i=0; i<e->args.size(); i++ )
    {
      expr_statement *es = new expr_statement;
      es->value = e->args[i];
      es->tok = es->value->tok;
      b->statements.push_back(es);
    }

  b->visit(this);
  relaxed_p = false;
  e = 0;
  provide (e);
}

void
void_statement_reducer::reduce_target_symbol (target_symbol* e,
                                              expression* operand)
{
  // When the result of any target_symbol isn't needed, it's just as good to
  // evaluate the operand and any array indexes directly

  block *b = new block;
  b->tok = e->tok;

  if (operand)
    {
      expr_statement *es = new expr_statement;
      es->value = operand;
      es->tok = es->value->tok;
      b->statements.push_back(es);
    }

  for (unsigned i=0; i<e->components.size(); i++ )
    {
      if (e->components[i].type != target_symbol::comp_expression_array_index)
        continue;

      expr_statement *es = new expr_statement;
      es->value = e->components[i].expr_index;
      es->tok = es->value->tok;
      b->statements.push_back(es);
    }

  b->visit(this);
  relaxed_p = false;
  e = 0;
  provide (e);
}

void
void_statement_reducer::visit_atvar_op (atvar_op* e)
{
  if (session.verbose>2)
    clog << _("Eliding unused target symbol ") << *e->tok << endl;
  reduce_target_symbol (e);
}

void
void_statement_reducer::visit_target_symbol (target_symbol* e)
{
  if (session.verbose>2)
    clog << _("Eliding unused target symbol ") << *e->tok << endl;
  reduce_target_symbol (e);
}

void
void_statement_reducer::visit_cast_op (cast_op* e)
{
  if (session.verbose>2)
    clog << _("Eliding unused typecast ") << *e->tok << endl;
  reduce_target_symbol (e, e->operand);
}

void
void_statement_reducer::visit_autocast_op (autocast_op* e)
{
  if (session.verbose>2)
    clog << _("Eliding unused autocast ") << *e->tok << endl;
  reduce_target_symbol (e, e->operand);
}


void
void_statement_reducer::visit_defined_op (defined_op* e)
{
  // When the result of a @defined operation isn't needed, just elide
  // it entirely.  Its operand $expression must already be
  // side-effect-free.

  if (session.verbose>2)
    clog << _("Eliding unused check ") << *e->tok << endl;

  relaxed_p = false;
  e = 0;
  provide (e);
}



void semantic_pass_opt5 (systemtap_session& s, bool& relaxed_p)
{
  // Let's simplify statements with unused computed values.

  void_statement_reducer vuv (s, relaxed_p);
  // This instance may be reused for multiple probe/function body trims.

  vuv.focal_vars.insert (s.globals.begin(), s.globals.end());

  for (unsigned i=0; i<s.probes.size(); i++)
    vuv.replace (s.probes[i]->body);
  for (map<string,functiondecl*>::iterator it = s.functions.begin();
       it != s.functions.end(); it++)
    vuv.replace (it->second->body);
}


struct const_folder: public update_visitor
{
  systemtap_session& session;
  bool& relaxed_p;

  const_folder(systemtap_session& s, bool& r):
    session(s), relaxed_p(r), last_number(0), last_string(0) {}

  literal_number* last_number;
  literal_number* get_number(expression*& e);
  void visit_literal_number (literal_number* e);

  literal_string* last_string;
  literal_string* get_string(expression*& e);
  void visit_literal_string (literal_string* e);

  void get_literal(expression*& e, literal_number*& n, literal_string*& s);

  void visit_if_statement (if_statement* s);
  void visit_for_loop (for_loop* s);
  void visit_foreach_loop (foreach_loop* s);
  void visit_binary_expression (binary_expression* e);
  void visit_unary_expression (unary_expression* e);
  void visit_logical_or_expr (logical_or_expr* e);
  void visit_logical_and_expr (logical_and_expr* e);
  // void visit_regex_query (regex_query* e); // XXX: would require executing dfa at compile-time
  void visit_comparison (comparison* e);
  void visit_concatenation (concatenation* e);
  void visit_ternary_expression (ternary_expression* e);
  void visit_defined_op (defined_op* e);
  void visit_target_symbol (target_symbol* e);
};

void
const_folder::get_literal(expression*& e,
                          literal_number*& n,
                          literal_string*& s)
{
  replace (e);
  n = (e == last_number) ? last_number : NULL;
  s = (e == last_string) ? last_string : NULL;
}

literal_number*
const_folder::get_number(expression*& e)
{
  replace (e);
  return (e == last_number) ? last_number : NULL;
}

void
const_folder::visit_literal_number (literal_number* e)
{
  last_number = e;
  provide (e);
}

literal_string*
const_folder::get_string(expression*& e)
{
  replace (e);
  return (e == last_string) ? last_string : NULL;
}

void
const_folder::visit_literal_string (literal_string* e)
{
  last_string = e;
  provide (e);
}

void
const_folder::visit_if_statement (if_statement* s)
{
  literal_number* cond = get_number (s->condition);
  if (!cond)
    {
      replace (s->thenblock);
      replace (s->elseblock);
      provide (s);
    }
  else
    {
      if (session.verbose>2)
        clog << _F("Collapsing constant-%" PRIi64 " if-statement %s",
                   cond->value, lex_cast(*s->tok).c_str()) << endl;
      relaxed_p = false;

      statement* n = cond->value ? s->thenblock : s->elseblock;
      if (n)
        n->visit (this);
      else
        provide (new null_statement (s->tok));
    }
}

void
const_folder::visit_for_loop (for_loop* s)
{
  literal_number* cond = get_number (s->cond);
  if (!cond || cond->value)
    {
      replace (s->init);
      replace (s->incr);
      replace (s->block);
      provide (s);
    }
  else
    {
      if (session.verbose>2)
        clog << _("Collapsing constantly-false for-loop ") << *s->tok << endl;
      relaxed_p = false;

      if (s->init)
        s->init->visit (this);
      else
        provide (new null_statement (s->tok));
    }
}

void
const_folder::visit_foreach_loop (foreach_loop* s)
{
  literal_number* limit = get_number (s->limit);
  if (!limit || limit->value > 0)
    {
      for (unsigned i = 0; i < s->indexes.size(); ++i)
        replace (s->indexes[i]);
      replace (s->base);
      replace (s->value);
      replace (s->block);
      provide (s);
    }
  else
    {
      if (session.verbose>2)
        clog << _("Collapsing constantly-limited foreach-loop ") << *s->tok << endl;
      relaxed_p = false;

      provide (new null_statement (s->tok));
    }
}

void
const_folder::visit_binary_expression (binary_expression* e)
{
  int64_t value;
  literal_number* left = get_number (e->left);
  literal_number* right = get_number (e->right);

  if (right && !right->value && (e->op == "/" || e->op == "%"))
    {
      // Give divide-by-zero a chance to be optimized out elsewhere,
      // and if not it will be a runtime error anyway...
      provide (e);
      return;
    }

  if (left && right)
    {
      if (e->op == "+")
        value = left->value + right->value;
      else if (e->op == "-")
        value = left->value - right->value;
      else if (e->op == "*")
        value = left->value * right->value;
      else if (e->op == "&")
        value = left->value & right->value;
      else if (e->op == "|")
        value = left->value | right->value;
      else if (e->op == "^")
        value = left->value ^ right->value;
      else if (e->op == ">>")
        value = left->value >> max(min(right->value, (int64_t)64), (int64_t)0);
      else if (e->op == "<<")
        value = left->value << max(min(right->value, (int64_t)64), (int64_t)0);
      else if (e->op == "/")
        value = (left->value == LLONG_MIN && right->value == -1) ? LLONG_MIN :
                left->value / right->value;
      else if (e->op == "%")
        value = (left->value == LLONG_MIN && right->value == -1) ? 0 :
                left->value % right->value;
      else
        throw SEMANTIC_ERROR (_("unsupported binary operator ") + (string)e->op);
    }

  else if ((left && ((left->value == 0 && (e->op == "*" || e->op == "&" ||
                                           e->op == ">>" || e->op == "<<" )) ||
                     (left->value ==-1 && (e->op == "|" || e->op == ">>"))))
           ||
           (right && ((right->value == 0 && (e->op == "*" || e->op == "&")) ||
                      (right->value == 1 && (e->op == "%")) ||
                      (right->value ==-1 && (e->op == "%" || e->op == "|")))))
    {
      expression* other = left ? e->right : e->left;
      varuse_collecting_visitor vu(session);
      other->visit(&vu);
      if (!vu.side_effect_free())
        {
          provide (e);
          return;
        }

      // we'll pass on type=pe_long inference to the expression
      if (other->type == pe_unknown)
        other->type = pe_long;
      else if (other->type != pe_long)
        {
          // this mismatch was not caught in the initial type resolution pass,
          // generate a mismatch (left doesn't match right) error
          typeresolution_info ti(session);
          ti.assert_resolvability = true; // need this to get it throw errors
          ti.mismatch_complexity = 1; // also needed to throw errors
          ti.mismatch(e);
        }

      if (left)
        value = left->value;
      else if (e->op == "%")
        value = 0;
      else
        value = right->value;
    }

  else if ((left && ((left->value == 0 && (e->op == "+" || e->op == "|" ||
                                           e->op == "^")) ||
                     (left->value == 1 && (e->op == "*")) ||
                     (left->value ==-1 && (e->op == "&"))))
           ||
           (right && ((right->value == 0 && (e->op == "+" || e->op == "-" ||
                                             e->op == "|" || e->op == "^")) ||
                      (right->value == 1 && (e->op == "*" || e->op == "/")) ||
                      (right->value ==-1 && (e->op == "&")) ||
                      (right->value <= 0 && (e->op == ">>" || e->op == "<<")))))
    {
      if (session.verbose>2)
        clog << _("Collapsing constant-identity binary operator ") << *e->tok << endl;
      relaxed_p = false;

      // we'll pass on type=pe_long inference to the expression
      expression* other = left ? e->right : e->left;
      if (other->type == pe_unknown)
        other->type = pe_long;
      else if (other->type != pe_long)
        {
          // this mismatch was not caught in the initial type resolution pass,
          // generate a mismatch (left doesn't match right) error
          typeresolution_info ti(session);
          ti.assert_resolvability = true; // need this to get it throw errors
          ti.mismatch_complexity = 1; // also needed to throw errors
          ti.mismatch(e);
        }

      provide (other);
      return;
    }

  else
    {
      provide (e);
      return;
    }

  if (session.verbose>2)
    clog << _F("Collapsing constant-%" PRIi64 " binary operator %s",
               value, lex_cast(*e->tok).c_str()) << endl;
  relaxed_p = false;

  literal_number* n = new literal_number(value);
  n->tok = e->tok;
  n->visit (this);
}

void
const_folder::visit_unary_expression (unary_expression* e)
{
  literal_number* operand = get_number (e->operand);
  if (!operand)
    provide (e);
  else
    {
      if (session.verbose>2)
        clog << _("Collapsing constant unary ") << *e->tok << endl;
      relaxed_p = false;

      literal_number* n = new literal_number (*operand);
      n->tok = e->tok;
      if (e->op == "+")
        ; // nothing to do
      else if (e->op == "-")
        n->value = -n->value;
      else if (e->op == "!")
        n->value = !n->value;
      else if (e->op == "~")
        n->value = ~n->value;
      else
        throw SEMANTIC_ERROR (_("unsupported unary operator ") + (string)e->op);
      n->visit (this);
    }
}

void
const_folder::visit_logical_or_expr (logical_or_expr* e)
{
  int64_t value;
  literal_number* left = get_number (e->left);
  literal_number* right = get_number (e->right);

  if (left && right)
    value = left->value || right->value;

  else if ((left && left->value) || (right && right->value))
    {
      // If the const is on the left, we get to short-circuit the right
      // immediately.  Otherwise, we can only eliminate the LHS if it's pure.
      if (right)
        {
          varuse_collecting_visitor vu(session);
          e->left->visit(&vu);
          if (!vu.side_effect_free())
            {
              provide (e);
              return;
            }
        }

      value = 1;
    }

  // We might also get rid of useless "0||x" and "x||0", except it does
  // normalize x to 0 or 1.  We could change it to "!!x", but it's not clear
  // that this would gain us much.

  else
    {
      provide (e);
      return;
    }

  if (session.verbose>2)
    clog << _("Collapsing constant logical-OR ") << *e->tok << endl;
  relaxed_p = false;

  literal_number* n = new literal_number(value);
  n->tok = e->tok;
  n->visit (this);
}

void
const_folder::visit_logical_and_expr (logical_and_expr* e)
{
  int64_t value;
  literal_number* left = get_number (e->left);
  literal_number* right = get_number (e->right);

  if (left && right)
    value = left->value && right->value;

  else if ((left && !left->value) || (right && !right->value))
    {
      // If the const is on the left, we get to short-circuit the right
      // immediately.  Otherwise, we can only eliminate the LHS if it's pure.
      if (right)
        {
          varuse_collecting_visitor vu(session);
          e->left->visit(&vu);
          if (!vu.side_effect_free())
            {
              provide (e);
              return;
            }
        }

      value = 0;
    }

  // We might also get rid of useless "1&&x" and "x&&1", except it does
  // normalize x to 0 or 1.  We could change it to "!!x", but it's not clear
  // that this would gain us much.

  else
    {
      provide (e);
      return;
    }

  if (session.verbose>2)
    clog << _("Collapsing constant logical-AND ") << *e->tok << endl;
  relaxed_p = false;

  literal_number* n = new literal_number(value);
  n->tok = e->tok;
  n->visit (this);
}

void
const_folder::visit_comparison (comparison* e)
{
  int comp;

  literal_number *left_num, *right_num;
  literal_string *left_str, *right_str;
  get_literal(e->left, left_num, left_str);
  get_literal(e->right, right_num, right_str);

  if (left_str && right_str)
    comp = left_str->value.compare(right_str->value);

  else if (left_num && right_num)
    comp = left_num->value < right_num->value ? -1 :
           left_num->value > right_num->value ? 1 : 0;

  else if ((left_num && ((left_num->value == LLONG_MIN &&
                          (e->op == "<=" || e->op == ">")) ||
                         (left_num->value == LLONG_MAX &&
                          (e->op == ">=" || e->op == "<"))))
           ||
           (right_num && ((right_num->value == LLONG_MIN &&
                            (e->op == ">=" || e->op == "<")) ||
                           (right_num->value == LLONG_MAX &&
                            (e->op == "<=" || e->op == ">")))))
    {
      expression* other = left_num ? e->right : e->left;
      varuse_collecting_visitor vu(session);
      other->visit(&vu);
      if (!vu.side_effect_free())
        provide (e);
      else
        {
          if (session.verbose>2)
            clog << _("Collapsing constant-boundary comparison ") << *e->tok << endl;
          relaxed_p = false;

          // ops <= and >= are true, < and > are false
          literal_number* n = new literal_number( e->op.length() == 2 );
          n->tok = e->tok;
          n->visit (this);
        }
      return;
    }

  else
    {
      provide (e);
      return;
    }

  if (session.verbose>2)
    clog << _("Collapsing constant comparison ") << *e->tok << endl;
  relaxed_p = false;

  int64_t value;
  if (e->op == "==")
    value = comp == 0;
  else if (e->op == "!=")
    value = comp != 0;
  else if (e->op == "<")
    value = comp < 0;
  else if (e->op == ">")
    value = comp > 0;
  else if (e->op == "<=")
    value = comp <= 0;
  else if (e->op == ">=")
    value = comp >= 0;
  else
    throw SEMANTIC_ERROR (_("unsupported comparison operator ") + (string)e->op);

  literal_number* n = new literal_number(value);
  n->tok = e->tok;
  n->visit (this);
}

void
const_folder::visit_concatenation (concatenation* e)
{
  literal_string* left = get_string (e->left);
  literal_string* right = get_string (e->right);

  if (left && right)
    {
      if (session.verbose>2)
        clog << _("Collapsing constant concatenation ") << *e->tok << endl;
      relaxed_p = false;

      literal_string* n = new literal_string (*left);
      n->tok = e->tok;
      n->value = (string)n->value + (string)right->value;
      n->visit (this);
    }
  else if ((left && left->value.empty()) ||
           (right && right->value.empty()))
    {
      if (session.verbose>2)
        clog << _("Collapsing identity concatenation ") << *e->tok << endl;
      relaxed_p = false;
      provide(left ? e->right : e->left);
    }
  else
    provide (e);
}

void
const_folder::visit_ternary_expression (ternary_expression* e)
{
  literal_number* cond = get_number (e->cond);
  if (!cond)
    {
      replace (e->truevalue);
      replace (e->falsevalue);
      provide (e);
    }
  else
    {
      if (session.verbose>2)
        clog << _F("Collapsing constant-%" PRIi64 " ternary %s",
                   cond->value, lex_cast(*e->tok).c_str()) << endl;
      relaxed_p = false;

      expression* n = cond->value ? e->truevalue : e->falsevalue;
      n->visit (this);
    }
}

void
const_folder::visit_defined_op (defined_op* e)
{
  // If a @defined makes it this far, then it is, de facto, undefined.

  if (session.verbose>2)
    clog << _("Collapsing untouched @defined check ") << *e->tok << endl;
  relaxed_p = false;

  literal_number* n = new literal_number (0);
  n->tok = e->tok;
  n->visit (this);
}

void
const_folder::visit_target_symbol (target_symbol* e)
{
  if (session.skip_badvars)
    {
      // Upon user request for ignoring context, the symbol is replaced
      // with a literal 0 and a warning message displayed
      // XXX this ignores possible side-effects, e.g. in array indexes
      literal_number* ln_zero = new literal_number (0);
      ln_zero->tok = e->tok;
      provide (ln_zero);
      session.print_warning (_("Bad $context variable being substituted with literal 0"),
                               e->tok);
      relaxed_p = false;
    }
  else
    update_visitor::visit_target_symbol (e);
}

static int initial_typeres_pass(systemtap_session& s);
static int semantic_pass_const_fold (systemtap_session& s, bool& relaxed_p)
{
  // attempt an initial type resolution pass to see if there are any type
  // mismatches before we starting whisking away vars that get switched out
  // with a const.

  // return if the initial type resolution pass reported errors (type mismatches)
  int rc = initial_typeres_pass(s);
  if (rc)
    {
      relaxed_p = true;
      return rc;
    }

  // Let's simplify statements with constant values.
  const_folder cf (s, relaxed_p);
  // This instance may be reused for multiple probe/function body trims.

  for (unsigned i=0; i<s.probes.size(); i++)
    cf.replace (s.probes[i]->body);
  for (map<string,functiondecl*>::iterator it = s.functions.begin();
       it != s.functions.end(); it++)
    cf.replace (it->second->body);
  return 0;
}


struct dead_control_remover: public traversing_visitor
{
  systemtap_session& session;
  bool& relaxed_p;
  statement* control;

  dead_control_remover(systemtap_session& s, bool& r):
    session(s), relaxed_p(r), control(NULL) {}

  void visit_block (block *b);

  // When a block contains any of these, the following statements are dead.
  void visit_return_statement (return_statement* s) { control = s; }
  void visit_next_statement (next_statement* s) { control = s; }
  void visit_break_statement (break_statement* s) { control = s; }
  void visit_continue_statement (continue_statement* s) { control = s; }
};


void dead_control_remover::visit_block (block* b)
{
  vector<statement*>& vs = b->statements;
  if (vs.size() == 0) /* else (size_t) size()-1 => very big */
    return;
  for (size_t i = 0; i < vs.size() - 1; ++i)
    {
      vs[i]->visit (this);
      if (vs[i] == control)
        {
          session.print_warning(_("statement will never be reached"),
                                vs[i + 1]->tok);
          vs.erase(vs.begin() + i + 1, vs.end());
          relaxed_p = false;
          break;
        }
    }
}


static void semantic_pass_dead_control (systemtap_session& s, bool& relaxed_p)
{
  // Let's remove code that follow unconditional control statements

  dead_control_remover dc (s, relaxed_p);

  for (unsigned i=0; i<s.probes.size(); i++)
    s.probes[i]->body->visit(&dc);

  for (map<string,functiondecl*>::iterator it = s.functions.begin();
       it != s.functions.end(); it++)
    it->second->body->visit(&dc);
}


struct duplicate_function_remover: public functioncall_traversing_visitor
{
  systemtap_session& s;
  map<functiondecl*, functiondecl*>& duplicate_function_map;

  duplicate_function_remover(systemtap_session& sess,
			     map<functiondecl*, functiondecl*>&dfm):
    s(sess), duplicate_function_map(dfm) {};

  void visit_functioncall (functioncall* e);
};

void
duplicate_function_remover::visit_functioncall (functioncall *e)
{
  functioncall_traversing_visitor::visit_functioncall (e);

  // If any of the current function call references points to a function that
  // is a duplicate, replace it.
  for (unsigned i = 0; i < e->referents.size(); i++)
    {
      functiondecl* referent = e->referents[i];
      if (duplicate_function_map.count(referent) != 0)
        {
          if (s.verbose>2)
              clog << _F("Changing %s reference to %s reference\n",
                         referent->unmangled_name.to_string().c_str(),
                         duplicate_function_map[referent]->unmangled_name.to_string().c_str());
          e->tok = duplicate_function_map[referent]->tok;
          e->function = duplicate_function_map[referent]->name;
          e->referents[i] = duplicate_function_map[referent];
        }
    }
}

static string
get_functionsig (functiondecl* f)
{
  ostringstream s;

  // Get the "name:args body" of the function in s.  We have to
  // include the args since the function 'x1(a, b)' is different than
  // the function 'x2(b, a)' even if the bodies of the two functions
  // are exactly the same.
  f->printsig(s);
  f->body->print(s);

  // printsig puts f->name + ':' on the front.  Remove this
  // (otherwise, functions would never compare equal).
  string str = s.str().erase(0, f->unmangled_name.size() + 1);

  // Return the function signature.
  return str;
}

void semantic_pass_opt6 (systemtap_session& s, bool& relaxed_p)
{
  // Walk through all the functions, looking for duplicates.
  map<string, functiondecl*> functionsig_map;
  map<functiondecl*, functiondecl*> duplicate_function_map;


  vector<functiondecl*> newly_zapped_functions;
  for (map<string,functiondecl*>::iterator it = s.functions.begin(); it != s.functions.end(); it++)
    {
      functiondecl *fd = it->second;
      string functionsig = get_functionsig(fd);

      if (functionsig_map.count(functionsig) == 0)
	{
	  // This function is unique.  Remember it.
	  functionsig_map[functionsig] = fd;
	}
      else
        {
	  // This function is a duplicate.
	  duplicate_function_map[fd] = functionsig_map[functionsig];
          newly_zapped_functions.push_back (fd);
	  relaxed_p = false;
	}
    }
  for (unsigned i=0; i<newly_zapped_functions.size(); i++)
    {
      map<string,functiondecl*>::iterator where = s.functions.find (newly_zapped_functions[i]->name);
      assert (where != s.functions.end());
      s.functions.erase (where);
    }


  // If we have duplicate functions, traverse down the tree, replacing
  // the appropriate function calls.
  // duplicate_function_remover::visit_functioncall() handles the
  // details of replacing the function calls.
  if (duplicate_function_map.size() != 0)
    {
      duplicate_function_remover dfr (s, duplicate_function_map);

      for (unsigned i=0; i < s.probes.size(); i++)
	s.probes[i]->body->visit(&dfr);
    }
}

struct stable_analysis: public embedded_tags_visitor
{
  bool stable;
  stable_analysis(): embedded_tags_visitor(true), stable(false) {};

  void visit_embeddedcode (embeddedcode* s);
  void visit_functioncall (functioncall* e);
};

void stable_analysis::visit_embeddedcode (embeddedcode* s)
{
  embedded_tags_visitor::visit_embeddedcode(s);
  if (tagged_p("/* stable */"))
    stable = true;
  if (stable && !tagged_p("/* pure */"))
    throw SEMANTIC_ERROR(_("stable function must also be /* pure */"),
        s->tok);
}

void stable_analysis::visit_functioncall (functioncall*)
{
}

// Examines entire subtree for any stable functioncalls.
struct stable_finder: public traversing_visitor
{
  bool stable;
  set<string>& stable_fcs;
  stable_finder(set<string>&s): stable(false), stable_fcs(s) {};
  void visit_functioncall (functioncall* e);
};

void stable_finder::visit_functioncall (functioncall* e)
{
  if (stable_fcs.find(e->function) != stable_fcs.end())
    stable = true;
  traversing_visitor::visit_functioncall(e);
}

// Examines current level of block for stable functioncalls.
// Does not descend into sublevels.
struct level_check: public traversing_visitor
{
  bool stable;
  set<string>& stable_fcs;
  level_check(set<string>& s): stable(false), stable_fcs(s) {};

  void visit_block (block* s);
  void visit_try_block (try_block *s);
  void visit_if_statement (if_statement* s);
  void visit_for_loop (for_loop* s);
  void visit_foreach_loop (foreach_loop* s);
  void visit_functioncall (functioncall* s);
};

void level_check::visit_block (block*)
{
}

void level_check::visit_try_block (try_block* s)
{
  if (s->catch_error_var)
    s->catch_error_var->visit(this);
}

void level_check::visit_if_statement (if_statement* s)
{
  s->condition->visit(this);
}

void level_check::visit_for_loop (for_loop* s)
{
  if (s->init) s->init->visit(this);
  s->cond->visit(this);
  if (s->incr) s->incr->visit(this);
}

void level_check::visit_foreach_loop (foreach_loop* s)
{
  s->base->visit(this);

  for (unsigned i=0; i<s->indexes.size(); i++)
    s->indexes[i]->visit(this);

  if (s->value)
    s->value->visit(this);

  if (s->limit)
    s->limit->visit(this);
}

void level_check::visit_functioncall (functioncall* e)
{
  if (stable_fcs.find(e->function) != stable_fcs.end())
    stable = true;
  traversing_visitor::visit_functioncall(e);
}

struct stable_functioncall_visitor: public update_visitor
{
  systemtap_session& session;
  functiondecl* current_function;
  derived_probe* current_probe;
  set<string>& stable_fcs;
  set<string> scope_vars;
  map<string,vardecl*> new_vars;
  vector<pair<expr_statement*,block*> > new_stmts;
  unsigned loop_depth;
  block* top_scope;
  block* curr_scope;
  stable_functioncall_visitor(systemtap_session& s, set<string>& sfc):
    session(s), current_function(0), current_probe(0), stable_fcs(sfc),
    loop_depth(0), top_scope(0), curr_scope(0) {};

  statement* convert_stmt(statement* s);
  void visit_block (block* s);
  void visit_try_block (try_block* s);
  void visit_if_statement (if_statement* s);
  void visit_for_loop (for_loop* s);
  void visit_foreach_loop (foreach_loop* s);
  void visit_functioncall (functioncall* e);
};

statement* stable_functioncall_visitor::convert_stmt (statement* s)
{
  if (top_scope == 0 &&
     (dynamic_cast<for_loop*>(s) || dynamic_cast<foreach_loop*>(s)))
    {
      stable_finder sf(stable_fcs);
      s->visit(&sf);
      if (sf.stable)
        {
          block* b = new block;
          b->tok = s->tok;
          b->statements.push_back(s);
          return b;
        }
    }
  else if (top_scope == 0 && !dynamic_cast<block*>(s))
    {
      level_check lc(stable_fcs);
      s->visit(&lc);
      if (lc.stable)
        {
          block* b = new block;
          b->tok = s->tok;
          b->statements.push_back(s);
          return b;
        }
    }

  return s;
}

void stable_functioncall_visitor::visit_block (block* s)
{
  block* prev_top_scope = top_scope;
  block* prev_scope = curr_scope;
  if (loop_depth == 0)
    top_scope = s;
  curr_scope = s;
  set<string> current_vars = scope_vars;

  update_visitor::visit_block(s);

  if (loop_depth == 0)
    top_scope = prev_top_scope;
  curr_scope = prev_scope;
  scope_vars = current_vars;
}

void stable_functioncall_visitor::visit_try_block (try_block* s)
{
  if (s->try_block)
    s->try_block = convert_stmt(s->try_block);
  replace(s->try_block);
  replace(s->catch_error_var);
  if (s->catch_block)
    s->catch_block = convert_stmt(s->catch_block);
  replace(s->catch_block);
  provide(s);
}

void stable_functioncall_visitor::visit_if_statement (if_statement* s)
{
  block* prev_top_scope = top_scope;

  if (loop_depth == 0)
    top_scope = 0;
  replace(s->condition);
  s->thenblock = convert_stmt(s->thenblock);
  replace(s->thenblock);
  if (loop_depth == 0)
    top_scope = 0;
  if (s->elseblock)
    s->elseblock = convert_stmt(s->elseblock);
  replace(s->elseblock);
  provide(s);

  top_scope = prev_top_scope;
}

void stable_functioncall_visitor::visit_for_loop (for_loop* s)
{
  replace(s->init);
  replace(s->cond);
  replace(s->incr);
  loop_depth++;
  s->block = convert_stmt(s->block);
  replace(s->block);
  loop_depth--;
  provide(s);
}

void stable_functioncall_visitor::visit_foreach_loop (foreach_loop* s)
{
  for (unsigned i = 0; i < s->indexes.size(); ++i)
    replace(s->indexes[i]);
  replace(s->base);
  replace(s->value);
  replace(s->limit);
  loop_depth++;
  s->block = convert_stmt(s->block);
  replace(s->block);
  loop_depth--;
  provide(s);
}

void stable_functioncall_visitor::visit_functioncall (functioncall* e)
{
  for (unsigned i = 0; i < e->args.size(); ++i)
    replace (e->args[i]);

  if (stable_fcs.find(e->function) != stable_fcs.end())
    {
      string name("__stable_");
      name.append(e->function).append("_value");

      // Variable potentially not in scope since it is in a sibling block
      if (scope_vars.find(e->function) == scope_vars.end())
        {
          if (new_vars.find(e->function) == new_vars.end())
            {
              // New variable declaration to store result of function call
              vardecl* v = new vardecl;
              v->unmangled_name = v->name = name;
              v->tok = e->tok;
              v->set_arity(0, e->tok);
              v->type = e->type;
              if (current_function)
                current_function->locals.push_back(v);
              else
                current_probe->locals.push_back(v);
              new_vars[e->function] = v;
            }

          symbol* sym = new symbol;
          sym->name = name;
          sym->tok = e->tok;
          sym->referent = new_vars[e->function];
          sym->type = e->type;

          functioncall* fc = new functioncall;
          fc->tok = e->tok;
          fc->function = e->function;
          fc->referents = e->referents;
          fc->type = e->type;

          assignment* a = new assignment;
          a->tok = e->tok;
          a->op = "=";
          a->left = sym;
          a->right = fc;
          a->type = e->type;

          expr_statement* es = new expr_statement;
          es->tok = e->tok;
          es->value = a;

          // Store location of the block to put new declaration.
          if (loop_depth != 0)
            {
              assert(top_scope);
              new_stmts.push_back(make_pair(es,top_scope));
            }
          else 
            {
              assert(curr_scope);
              new_stmts.push_back(make_pair(es,curr_scope));
            }

          scope_vars.insert(e->function);

          provide(sym);
        }
      else
        {
          symbol* sym = new symbol;
          sym->name = name;
          sym->tok = e->tok;
          sym->referent = new_vars[e->function];
          sym->type = e->type;
          provide(sym);
        }
      return;
    }

  provide(e);
}

// Cache stable embedded-c functioncall results and replace
// all calls with same name using that value to reduce duplicate
// functioncall overhead. Functioncalls are pulled out of any
// top-level loops and put into if/try blocks.
void semantic_pass_opt7(systemtap_session& s)
{
  set<string> stable_fcs;
  for (map<string,functiondecl*>::iterator it = s.functions.begin();
       it != s.functions.end(); ++it)
    {
      functiondecl* fn = (*it).second;
      stable_analysis sa;
      fn->body->visit(&sa);
      if (sa.stable && fn->formal_args.size() == 0)
        stable_fcs.insert(fn->name);
    }

  for (vector<derived_probe*>::iterator it = s.probes.begin();
       it != s.probes.end(); ++it)
    {
      stable_functioncall_visitor t(s, stable_fcs);
      t.current_probe = *it;
      (*it)->body = t.convert_stmt((*it)->body);
      t.replace((*it)->body);

      for (vector<pair<expr_statement*,block*> >::iterator st = t.new_stmts.begin();
           st != t.new_stmts.end(); ++st)
        st->second->statements.insert(st->second->statements.begin(), st->first);
    }

  for (map<string,functiondecl*>::iterator it = s.functions.begin();
       it != s.functions.end(); ++it)
    {
      functiondecl* fn = (*it).second;
      stable_functioncall_visitor t(s, stable_fcs);
      t.current_function = fn;
      fn->body = t.convert_stmt(fn->body);
      t.replace(fn->body);

      for (vector<pair<expr_statement*,block*> >::iterator st = t.new_stmts.begin();
           st != t.new_stmts.end(); ++st)
        st->second->statements.insert(st->second->statements.begin(), st->first);
    }
}

static int
semantic_pass_optimize1 (systemtap_session& s)
{
  // In this pass, we attempt to rewrite probe/function bodies to
  // eliminate some blatantly unnecessary code.  This is run before
  // type inference, but after symbol resolution and derived_probe
  // creation.  We run an outer "relaxation" loop that repeats the
  // optimizations until none of them find anything to remove.

  int rc = 0;

  // Save the old value of suppress_warnings, as we will be changing
  // it below.
  save_and_restore<bool> suppress_warnings(& s.suppress_warnings);

  bool relaxed_p = false;
  unsigned iterations = 0;
  while (! relaxed_p)
    {
      assert_no_interrupts();

      relaxed_p = true; // until proven otherwise

      // If the verbosity is high enough, always print warnings (overrides -w),
      // or if not, always suppress warnings for every itteration after the first.
      if(s.verbose > 2)
        s.suppress_warnings = false;
      else if (iterations > 0)
        s.suppress_warnings = true;

      if (!s.unoptimized)
        {
          semantic_pass_opt1 (s, relaxed_p);
          semantic_pass_opt2 (s, relaxed_p, iterations); // produce some warnings only on iteration=0
          semantic_pass_opt3 (s, relaxed_p);
          semantic_pass_opt4 (s, relaxed_p);
          semantic_pass_opt5 (s, relaxed_p);
        }

      // For listing mode, we need const-folding regardless of optimization so
      // that @defined expressions can be properly resolved.  PR11360
      // We also want it in case variables are used in if/case expressions,
      // so enable always.  PR11366
      // rc is incremented if there is an error that got reported.
      rc += semantic_pass_const_fold (s, relaxed_p);

      if (!s.unoptimized)
        semantic_pass_dead_control (s, relaxed_p);

      iterations ++;
    }

  return rc;
}


static int
semantic_pass_optimize2 (systemtap_session& s)
{
  // This is run after type inference.  We run an outer "relaxation"
  // loop that repeats the optimizations until none of them find
  // anything to remove.

  int rc = 0;

  // Save the old value of suppress_warnings, as we will be changing
  // it below.
  save_and_restore<bool> suppress_warnings(& s.suppress_warnings);

  bool relaxed_p = false;
  unsigned iterations = 0;
  while (! relaxed_p)
    {
      assert_no_interrupts();
      relaxed_p = true; // until proven otherwise

      // If the verbosity is high enough, always print warnings (overrides -w),
      // or if not, always suppress warnings for every itteration after the first.
      if(s.verbose > 2)
        s.suppress_warnings = false;
      else if (iterations > 0)
        s.suppress_warnings = true;

      if (!s.unoptimized)
        semantic_pass_opt6 (s, relaxed_p);

      iterations++;
    }

  if (!s.unoptimized)
    semantic_pass_opt7(s);

  return rc;
}



// ------------------------------------------------------------------------
// type resolution

struct autocast_expanding_visitor: public var_expanding_visitor
{
  typeresolution_info& ti;
  autocast_expanding_visitor (typeresolution_info& ti): ti(ti) {}

  void resolve_functioncall (functioncall* fc)
    {
      // This is a very limited version of semantic_pass_symbols, but we're
      // late in the game at this point.  We won't get a chance to optimize,
      // but for now the only functions we expect are kernel/user_string from
      // pretty-printing, which don't need optimization.

      systemtap_session& s = ti.session;
      size_t nfiles = s.files.size();

      symresolution_info sym (s);
      sym.current_function = ti.current_function;
      sym.current_probe = ti.current_probe;
      fc->visit (&sym);

      // NB: synthetic functions get tacked onto the origin file, so we won't
      // see them growing s.files[].  Traverse it directly.
      for (unsigned i = 0; i < fc->referents.size(); i++)
        {
          functiondecl* fd = fc->referents[i];
          sym.current_function = fd;
          sym.current_probe = 0;
          fd->body->visit (&sym);
        }

      while (nfiles < s.files.size())
        {
          stapfile* dome = s.files[nfiles++];
          for (size_t i = 0; i < dome->functions.size(); ++i)
            {
              functiondecl* fd = dome->functions[i];
              sym.current_function = fd;
              sym.current_probe = 0;
              fd->body->visit (&sym);
              // NB: not adding to s.functions just yet...
            }
        }

      // Add only the direct functions we need.
      functioncall_traversing_visitor ftv;
      fc->visit (&ftv);
      for (set<functiondecl*>::iterator it = ftv.seen.begin();
           it != ftv.seen.end(); ++it)
        {
          functiondecl* fd = *it;
          pair<map<string,functiondecl*>::iterator,bool> inserted =
            s.functions.insert (make_pair (fd->name, fd));
          if (!inserted.second && inserted.first->second != fd)
            throw SEMANTIC_ERROR
              (_F("resolved function '%s' conflicts with an existing function",
                  fd->unmangled_name.to_string().c_str()), fc->tok);
        }
    }

  void visit_autocast_op (autocast_op* e)
    {
      const bool lvalue = is_active_lvalue (e);
      const exp_type_ptr& details = e->operand->type_details;
      if (details && !e->saved_conversion_error)
        {
          functioncall* fc = details->expand (e, lvalue);
          if (fc)
            {
              ti.num_newly_resolved++;

              resolve_functioncall (fc);

              if (lvalue)
                provide_lvalue_call (fc);

              fc->visit (this);
              return;
            }
        }
      var_expanding_visitor::visit_autocast_op (e);
    }
};


struct initial_typeresolution_info : public typeresolution_info
{
  initial_typeresolution_info (systemtap_session& s): typeresolution_info(s)
  {}

  // these expressions are not supposed to make its way to the typeresolution
  // pass. they probably get substituted/replaced, but since this is an initial pass
  // and not all substitutions are done, replace the functions that throw errors.
  void visit_target_symbol (target_symbol*) {}
  void visit_atvar_op (atvar_op*) {}
  void visit_defined_op (defined_op*) {}
  void visit_entry_op (entry_op*) {}
  void visit_cast_op (cast_op*) {}
};

static int initial_typeres_pass(systemtap_session& s)
{
  // minimal type resolution based off of semantic_pass_types(), without
  // checking for complete type resolutions or autocast expanding
  initial_typeresolution_info ti(s);

  // Globals never have detailed types.
  // If we null them now, then all remaining vardecls can be detailed.
  for (unsigned j=0; j<s.globals.size(); j++)
    {
      vardecl* gd = s.globals[j];
      if (!gd->type_details)
        gd->type_details = ti.null_type;
    }

  ti.assert_resolvability = false;
  while (1)
    {
      assert_no_interrupts();

      ti.num_newly_resolved = 0;
      ti.num_still_unresolved = 0;
      ti.num_available_autocasts = 0;

      for (map<string,functiondecl*>::iterator it = s.functions.begin();
                                               it != s.functions.end(); it++)
        {
          assert_no_interrupts();

          functiondecl* fd = it->second;
          ti.current_probe = 0;
          ti.current_function = fd;
          ti.t = pe_unknown;
          fd->body->visit (& ti);
        }

      for (unsigned j=0; j<s.probes.size(); j++)
        {
          assert_no_interrupts();

          derived_probe* pn = s.probes[j];
          ti.current_function = 0;
          ti.current_probe = pn;
          ti.t = pe_unknown;
          pn->body->visit (& ti);

          probe_point* pp = pn->sole_location();
          if (pp->condition)
            {
              ti.current_function = 0;
              ti.current_probe = 0;
              ti.t = pe_long; // NB: expected type
              pp->condition->visit (& ti);
            }
        }
      if (ti.num_newly_resolved == 0) // converged
        {
          // take into account that if there are mismatches, we'd want to know
          // about them incase they get whisked away, later in this process
          if (!ti.assert_resolvability && ti.mismatch_complexity > 0) // found a mismatch!!
            {
              ti.assert_resolvability = true; // report errors
              if (s.verbose > 0)
                ti.mismatch_complexity = 1; // print out mismatched but not unresolved type mismatches
            }
          else
            break;
        }
      else
        ti.mismatch_complexity = 0;
    }

  return s.num_errors();
}

static int
semantic_pass_types (systemtap_session& s)
{
  int rc = 0;

  // next pass: type inference
  unsigned iterations = 0;
  typeresolution_info ti (s);

  // Globals never have detailed types.
  // If we null them now, then all remaining vardecls can be detailed.
  for (unsigned j=0; j<s.globals.size(); j++)
    {
      vardecl* gd = s.globals[j];
      if (!gd->type_details)
        gd->type_details = ti.null_type;
    }

  ti.assert_resolvability = false;
  while (1)
    {
      assert_no_interrupts();

      iterations ++;
      ti.num_newly_resolved = 0;
      ti.num_still_unresolved = 0;
      ti.num_available_autocasts = 0;

      for (map<string,functiondecl*>::iterator it = s.functions.begin();
                                               it != s.functions.end(); it++)
        try
          {
            assert_no_interrupts();
            
            functiondecl* fd = it->second;
            ti.current_probe = 0;
            ti.current_function = fd;
            ti.t = pe_unknown;
            fd->body->visit (& ti);
            // NB: we don't have to assert a known type for
            // functions here, to permit a "void" function.
            // The translator phase will omit the "retvalue".
            //
            // if (fd->type == pe_unknown)
            //   ti.unresolved (fd->tok);
            for (unsigned i=0; i < fd->locals.size(); ++i)
              ti.check_local (fd->locals[i]);
            
            // Check and run the autocast expanding visitor.
            if (ti.num_available_autocasts > 0)
              {
                autocast_expanding_visitor aev (ti);
                aev.replace (fd->body);
                ti.num_available_autocasts = 0;
              }
          }
        catch (const semantic_error& e)
          {
            throw SEMANTIC_ERROR(_F("while processing function %s",
                                    it->second->unmangled_name.to_string().c_str())).set_chain(e);
          }
      
      for (unsigned j=0; j<s.probes.size(); j++)
        try
          {
            assert_no_interrupts();
            
            derived_probe* pn = s.probes[j];
            ti.current_function = 0;
            ti.current_probe = pn;
            ti.t = pe_unknown;
            pn->body->visit (& ti);
            for (unsigned i=0; i < pn->locals.size(); ++i)
              ti.check_local (pn->locals[i]);
            
            // Check and run the autocast expanding visitor.
            if (ti.num_available_autocasts > 0)
              {
                autocast_expanding_visitor aev (ti);
                aev.replace (pn->body);
                ti.num_available_autocasts = 0;
              }
            
            probe_point* pp = pn->sole_location();
            if (pp->condition)
              {
                ti.current_function = 0;
                ti.current_probe = 0;
                ti.t = pe_long; // NB: expected type
                pp->condition->visit (& ti);
              }
          }
        catch (const semantic_error& e)
          {
            throw SEMANTIC_ERROR(_F("while processing probe %s",
                                    s.probes[j]->derived_locations(false).c_str())).set_chain(e);
          }
  
      for (unsigned j=0; j<s.globals.size(); j++)
        {
          vardecl* gd = s.globals[j];
          if (gd->type == pe_unknown)
            ti.unresolved (gd->tok);
          if(gd->arity == 0 && gd->wrap == true)
            {
              throw SEMANTIC_ERROR(_("wrapping not supported for scalars"), gd->tok);
            }
        }

      if (ti.num_newly_resolved == 0) // converged
        {
          if (ti.num_still_unresolved == 0)
            break; // successfully
          else if (! ti.assert_resolvability)
            {
              ti.assert_resolvability = true; // last pass, with error msgs
              if (s.verbose > 0)
                ti.mismatch_complexity = 0; // print every kind of mismatch
            }
          else
            { // unsuccessful conclusion
              rc ++;
              break;
            }
        }
      else
        ti.mismatch_complexity = 0; // reset for next pass
    }

  return rc + s.num_errors();
}


struct exp_type_null : public exp_type_details
{
  uintptr_t id () const { return 0; }
  bool expandable() const { return false; }
  functioncall *expand(autocast_op*, bool) { return NULL; }
};

typeresolution_info::typeresolution_info (systemtap_session& s):
  session(s), num_newly_resolved(0), num_still_unresolved(0),
  num_available_autocasts(0),
  assert_resolvability(false), mismatch_complexity(0),
  current_function(0), current_probe(0), t(pe_unknown),
  null_type(new exp_type_null())
{
}


void
typeresolution_info::visit_literal_number (literal_number* e)
{
  assert (e->type == pe_long);
  if ((t == e->type) || (t == pe_unknown))
    return;

  mismatch (e->tok, t, e->type);
}


void
typeresolution_info::visit_literal_string (literal_string* e)
{
  assert (e->type == pe_string);
  if ((t == e->type) || (t == pe_unknown))
    return;

  mismatch (e->tok, t, e->type);
}


void
typeresolution_info::visit_logical_or_expr (logical_or_expr *e)
{
  visit_binary_expression (e);
}


void
typeresolution_info::visit_logical_and_expr (logical_and_expr *e)
{
  visit_binary_expression (e);
}

void
typeresolution_info::visit_regex_query (regex_query *e)
{
  // NB: result of regex query is an integer!
  if (t == pe_stats || t == pe_string)
    invalid (e->tok, t);

  t = pe_string;
  e->left->visit (this);
  t = pe_string;
  e->right->visit (this); // parser ensures this is a literal known at compile time

  if (e->type == pe_unknown)
    {
      e->type = pe_long;
      resolved (e->tok, e->type);
    }
}


void
typeresolution_info::visit_comparison (comparison *e)
{
  // NB: result of any comparison is an integer!
  if (t == pe_stats || t == pe_string)
    invalid (e->tok, t);

  t = (e->right->type != pe_unknown) ? e->right->type : pe_unknown;
  e->left->visit (this);
  t = (e->left->type != pe_unknown) ? e->left->type : pe_unknown;
  e->right->visit (this);

  if (e->left->type != pe_unknown &&
      e->right->type != pe_unknown &&
      e->left->type != e->right->type)
    mismatch (e);

  if (e->type == pe_unknown)
    {
      e->type = pe_long;
      resolved (e->tok, e->type);
    }
}


void
typeresolution_info::visit_concatenation (concatenation *e)
{
  if (t != pe_unknown && t != pe_string)
    invalid (e->tok, t);

  t = pe_string;
  e->left->visit (this);
  t = pe_string;
  e->right->visit (this);

  if (e->type == pe_unknown)
    {
      e->type = pe_string;
      resolved (e->tok, e->type);
    }
}


void
typeresolution_info::visit_assignment (assignment *e)
{
  if (t == pe_stats)
    invalid (e->tok, t);

  if (e->op == "<<<") // stats aggregation
    {
      if (t == pe_string)
        invalid (e->tok, t);

      t = pe_stats;
      e->left->visit (this);
      t = pe_long;
      e->right->visit (this);
      if (e->type == pe_unknown ||
	  e->type == pe_stats)
        {
          e->type = pe_long;
          resolved (e->tok, e->type);
        }
    }

  else if (e->left->type == pe_stats)
    invalid (e->left->tok, e->left->type);

  else if (e->right->type == pe_stats)
    invalid (e->right->tok, e->right->type);

  else if (e->op == "+=" || // numeric only
           e->op == "-=" ||
           e->op == "*=" ||
           e->op == "/=" ||
           e->op == "%=" ||
           e->op == "&=" ||
           e->op == "^=" ||
           e->op == "|=" ||
           e->op == "<<=" ||
           e->op == ">>=" ||
           false)
    {
      visit_binary_expression (e);
    }
  else if (e->op == ".=" || // string only
           false)
    {
      if (t == pe_long || t == pe_stats)
        invalid (e->tok, t);

      t = pe_string;
      e->left->visit (this);
      t = pe_string;
      e->right->visit (this);
      if (e->type == pe_unknown)
        {
          e->type = pe_string;
          resolved (e->tok, e->type);
        }
    }
  else if (e->op == "=") // overloaded = for string & numeric operands
    {
      // logic similar to ternary_expression
      exp_type sub_type = t;

      // Infer types across the l/r values
      if (sub_type == pe_unknown && e->type != pe_unknown)
        sub_type = e->type;

      t = (sub_type != pe_unknown) ? sub_type :
        (e->right->type != pe_unknown) ? e->right->type :
        pe_unknown;
      e->left->visit (this);
      t = (sub_type != pe_unknown) ? sub_type :
        (e->left->type != pe_unknown) ? e->left->type :
        pe_unknown;
      e->right->visit (this);

      if ((sub_type != pe_unknown) && (e->type == pe_unknown))
        {
          e->type = sub_type;
          resolved (e->tok, e->type);
        }
      if ((sub_type == pe_unknown) && (e->left->type != pe_unknown))
        {
          e->type = e->left->type;
          resolved (e->tok, e->type);
        }

      if (e->left->type != pe_unknown &&
          e->right->type != pe_unknown &&
          e->left->type != e->right->type)
        mismatch (e);

      // Propagate type details from the RHS to the assignment
      if (e->type == e->right->type &&
          e->right->type_details && !e->type_details)
        resolved_details(e->right->type_details, e->type_details);

      // Propagate type details from the assignment to the LHS
      if (e->type == e->left->type && e->type_details)
        {
          if (e->left->type_details &&
              *e->left->type_details != *e->type_details &&
              *e->left->type_details != *null_type)
            resolved_details(null_type, e->left->type_details);
          else if (!e->left->type_details)
            resolved_details(e->type_details, e->left->type_details);
        }
    }
  else
    throw SEMANTIC_ERROR (_("unsupported assignment operator ") + (string)e->op);
}


void
typeresolution_info::visit_embedded_expr (embedded_expr *e)
{
  if (e->type == pe_unknown)
    {
      if (e->code.find ("/* string */") != string::npos)
        e->type = pe_string;
      else // if (e->code.find ("/* long */") != string::npos)
        e->type = pe_long;

      resolved (e->tok, e->type);
    }
}


void
typeresolution_info::visit_binary_expression (binary_expression* e)
{
  if (t == pe_stats || t == pe_string)
    invalid (e->tok, t);

  t = pe_long;
  e->left->visit (this);
  t = pe_long;
  e->right->visit (this);

  if (e->left->type != pe_unknown &&
      e->right->type != pe_unknown &&
      e->left->type != e->right->type)
    mismatch (e);

  if (e->type == pe_unknown)
    {
      e->type = pe_long;
      resolved (e->tok, e->type);
    }
}


void
typeresolution_info::visit_pre_crement (pre_crement *e)
{
  visit_unary_expression (e);
}


void
typeresolution_info::visit_post_crement (post_crement *e)
{
  visit_unary_expression (e);
}


void
typeresolution_info::visit_unary_expression (unary_expression* e)
{
  if (t == pe_stats || t == pe_string)
    invalid (e->tok, t);

  t = pe_long;
  e->operand->visit (this);

  if (e->type == pe_unknown)
    {
      e->type = pe_long;
      resolved (e->tok, e->type);
    }
}


void
typeresolution_info::visit_ternary_expression (ternary_expression* e)
{
  exp_type sub_type = t;

  t = pe_long;
  e->cond->visit (this);

  // Infer types across the true/false arms of the ternary expression.

  if (sub_type == pe_unknown && e->type != pe_unknown)
    sub_type = e->type;
  t = sub_type;
  e->truevalue->visit (this);
  t = sub_type;
  e->falsevalue->visit (this);

  if ((sub_type == pe_unknown) && (e->type != pe_unknown))
    ; // already resolved
  else if ((sub_type != pe_unknown) && (e->type == pe_unknown))
    {
      e->type = sub_type;
      resolved (e->tok, e->type);
    }
  else if ((sub_type == pe_unknown) && (e->truevalue->type != pe_unknown))
    {
      e->type = e->truevalue->type;
      resolved (e->tok, e->type);
    }
  else if ((sub_type == pe_unknown) && (e->falsevalue->type != pe_unknown))
    {
      e->type = e->falsevalue->type;
      resolved (e->tok, e->type);
    }
  else if (e->type != sub_type)
    mismatch (e->tok, sub_type, e->type);

  // Propagate type details from both true/false branches
  if (!e->type_details &&
      e->type == e->truevalue->type && e->type == e->falsevalue->type &&
      e->truevalue->type_details && e->falsevalue->type_details &&
      *e->truevalue->type_details == *e->falsevalue->type_details)
    resolved_details(e->truevalue->type_details, e->type_details);
}


template <class Referrer, class Referent>
void resolve_2types (Referrer* referrer, Referent* referent,
                    typeresolution_info* r, exp_type t, bool accept_unknown = false)
{
  exp_type& re_type = referrer->type;
  const token* re_tok = referrer->tok;
  exp_type& te_type = referent->type;

  if (t != pe_unknown && re_type == t && re_type == te_type)
    ; // do nothing: all three e->types in agreement
  else if (t == pe_unknown && re_type != pe_unknown && re_type == te_type)
    ; // do nothing: two known e->types in agreement
  else if (re_type != pe_unknown && te_type != pe_unknown && re_type != te_type)
    r->mismatch (re_tok, re_type, referent); // referrer-referent
  else if (re_type != pe_unknown && t != pe_unknown && re_type != t)
    r->mismatch (re_tok, t, referent); // referrer-t
  else if (te_type != pe_unknown && t != pe_unknown && te_type != t)
    r->mismatch (re_tok, t, referent); // referent-t
  else if (re_type == pe_unknown && t != pe_unknown)
    {
      // propagate from upstream
      re_type = t;
      r->resolved (re_tok, re_type);
      // catch re_type/te_type mismatch later
    }
  else if (re_type == pe_unknown && te_type != pe_unknown)
    {
      // propagate from referent
      re_type = te_type;
      r->resolved (re_tok, re_type);
      // catch re_type/t mismatch later
    }
  else if (re_type != pe_unknown && te_type == pe_unknown)
    {
      // propagate to referent
      te_type = re_type;
      r->resolved (re_tok, re_type, referent);
      // catch re_type/t mismatch later
    }
  else if (! accept_unknown)
    r->unresolved (re_tok);
}


void
typeresolution_info::visit_symbol (symbol* e)
{
  if (e->referent == 0)
    throw SEMANTIC_ERROR (_F("internal error: unresolved symbol '%s'",
                             e->name.to_string().c_str()), e->tok);

  resolve_2types (e, e->referent, this, t);

  if (e->type == e->referent->type)
    {
      // If both have type details, then they either must agree;
      // otherwise force them both to null.
      if (e->type_details && e->referent->type_details &&
          *e->type_details != *e->referent->type_details)
        {
          resolved_details(null_type, e->type_details);
          resolved_details(null_type, e->referent->type_details);
        }
      else if (e->type_details && !e->referent->type_details)
        resolved_details(e->type_details, e->referent->type_details);
      else if (!e->type_details && e->referent->type_details)
        resolved_details(e->referent->type_details, e->type_details);
    }
}


void
typeresolution_info::visit_target_symbol (target_symbol* e)
{
  // This occurs only if a target symbol was not resolved over in
  // tapset.cxx land, that error was properly suppressed, and the
  // later unused-expression-elimination pass didn't get rid of it
  // either.  So we have a target symbol that is believed to be of
  // genuine use, yet unresolved by the provider.

  if (session.verbose > 2)
    {
      clog << _("Resolution problem with ");
      if (current_function)
        {
          clog << "function " << current_function->name << endl;
          current_function->body->print (clog);
          clog << endl;
        }
      else if (current_probe)
        {
          clog << "probe " << *current_probe->sole_location() << endl;
          current_probe->body->print (clog);
          clog << endl;
        }
      else
        //TRANSLATORS: simply saying not an issue with a probe or function
        clog << _("other") << endl;
    }

  if (e->saved_conversion_error)
    throw (* (e->saved_conversion_error));
  else
    throw SEMANTIC_ERROR(_("unresolved target-symbol expression"), e->tok);
}


void
typeresolution_info::visit_atvar_op (atvar_op* e)
{
  // This occurs only if an @var() was not resolved over in
  // tapset.cxx land, that error was properly suppressed, and the
  // later unused-expression-elimination pass didn't get rid of it
  // either.  So we have an @var() that is believed to be of
  // genuine use, yet unresolved by the provider.

  if (session.verbose > 2)
    {
      clog << _("Resolution problem with ");
      if (current_function)
        {
          clog << "function " << current_function->name << endl;
          current_function->body->print (clog);
          clog << endl;
        }
      else if (current_probe)
        {
          clog << "probe " << *current_probe->sole_location() << endl;
          current_probe->body->print (clog);
          clog << endl;
        }
      else
        //TRANSLATORS: simply saying not an issue with a probe or function
        clog << _("other") << endl;
    }

  if (e->saved_conversion_error)
    throw (* (e->saved_conversion_error));
  else
    throw SEMANTIC_ERROR(_("unresolved @var() expression"), e->tok);
}


void
typeresolution_info::visit_defined_op (defined_op* e)
{
  throw SEMANTIC_ERROR(_("unexpected @defined"), e->tok);
}


void
typeresolution_info::visit_entry_op (entry_op* e)
{
  throw SEMANTIC_ERROR(_("@entry is only valid in .return probes"), e->tok);
}


void
typeresolution_info::visit_cast_op (cast_op* e)
{
  // Like target_symbol, a cast_op shouldn't survive this far
  // unless it was not resolved and its value is really needed.
  if (e->saved_conversion_error)
    throw (* (e->saved_conversion_error));
  else
    throw SEMANTIC_ERROR(_F("type definition '%s' not found in '%s'",
                            e->type_name.to_string().c_str(),
                            e->module.to_string().c_str()), e->tok);
}


void
typeresolution_info::visit_autocast_op (autocast_op* e)
{
  // Like cast_op, a implicit autocast_op shouldn't survive this far
  // unless it was not resolved and its value is really needed.
  if (assert_resolvability && e->saved_conversion_error)
    throw (* (e->saved_conversion_error));
  else if (assert_resolvability)
    throw SEMANTIC_ERROR(_("unknown type in dereference"), e->tok);

  t = pe_long;
  e->operand->visit (this);

  num_still_unresolved++;
  if (e->operand->type_details &&
      e->operand->type_details->expandable())
    num_available_autocasts++;
}


void
typeresolution_info::visit_perf_op (perf_op* e)
{
  // A perf_op should already be resolved
  if (t == pe_stats || t == pe_string)
    invalid (e->tok, t);

  e->type = pe_long;

  // (There is no real need to visit our operand - by parser
  // construction, it's always a string literal, with its type already
  // set.)
  t = pe_string;
  e->operand->visit (this);
}


void
typeresolution_info::visit_arrayindex (arrayindex* e)
{

  symbol *array = NULL;
  hist_op *hist = NULL;
  classify_indexable(e->base, array, hist);

  // Every hist_op has type [int]:int, that is to say, every hist_op
  // is a pseudo-one-dimensional integer array type indexed by
  // integers (bucket numbers).

  if (hist)
    {
      if (e->indexes.size() != 1)
	unresolved (e->tok);
      t = pe_long;
      e->indexes[0]->visit (this);
      if (e->indexes[0]->type != pe_long)
	unresolved (e->tok);
      hist->visit (this);
      if (e->type != pe_long)
	{
	  e->type = pe_long;
	  resolved (e->tok, e->type);
	}
      return;
    }

  // Now we are left with "normal" map inference and index checking.

  assert (array);
  assert (array->referent != 0);
  resolve_2types (e, array->referent, this, t);

  // now resolve the array indexes

  // if (e->referent->index_types.size() == 0)
  //   // redesignate referent as array
  //   e->referent->set_arity (e->indexes.size ());

  if (e->indexes.size() != array->referent->index_types.size())
    unresolved (e->tok); // symbol resolution should prevent this
  else for (unsigned i=0; i<e->indexes.size(); i++)
    {
      if (e->indexes[i])
        {
          expression* ee = e->indexes[i];
          exp_type& ft = array->referent->index_types [i];
          t = ft;
          ee->visit (this);
          exp_type at = ee->type;

          if ((at == pe_string || at == pe_long) && ft == pe_unknown)
            {
              // propagate to formal type
              ft = at;
              resolved (ee->tok, ft, array->referent, i);
            }
          if (at == pe_stats)
            invalid (ee->tok, at);
          if (ft == pe_stats)
            invalid (ee->tok, ft);
          if (at != pe_unknown && ft != pe_unknown && ft != at)
            mismatch (ee->tok, ee->type, array->referent, i);
          if (at == pe_unknown)
              unresolved (ee->tok);
        }
    }
}


void
typeresolution_info::visit_functioncall (functioncall* e)
{
  if (e->referents.empty())
    throw SEMANTIC_ERROR (_F("internal error: unresolved function call to '%s'",
                             e->function.to_string().c_str()), e->tok);

  exp_type original = t;
  for (unsigned fd = 0; fd < e->referents.size(); fd++)
    {
      t = original; // type may be changed by overloaded functions so restore it
      functiondecl* referent = e->referents[fd];
      resolve_2types (e, referent, this, t, true); // accept unknown type

      if (e->type == pe_stats)
        invalid (e->tok, e->type);

      const exp_type_ptr& func_type = referent->type_details;
      if (func_type && referent->type == e->type
          && (!e->type_details || *func_type != *e->type_details))
        resolved_details(referent->type_details, e->type_details);

      // now resolve the function parameters
      if (e->args.size() != referent->formal_args.size())
        unresolved (e->tok); // symbol resolution should prevent this
      else for (unsigned i=0; i<e->args.size(); i++)
        {
          expression* ee = e->args[i];
          exp_type& ft = referent->formal_args[i]->type;
          const token* fe_tok = referent->formal_args[i]->tok;
          t = ft;
          ee->visit (this);
          exp_type at = ee->type;

          if (((at == pe_string) || (at == pe_long)) && ft == pe_unknown)
            {
              // propagate to formal arg
              ft = at;
              resolved (ee->tok, ft, referent->formal_args[i], i);
            }
          if (at == pe_stats)
            invalid (ee->tok, at);
          if (ft == pe_stats)
            invalid (fe_tok, ft);
          if (at != pe_unknown && ft != pe_unknown && ft != at)
            mismatch (ee->tok, ee->type, referent->formal_args[i], i);
          if (at == pe_unknown)
            unresolved (ee->tok);
        }
    }
}


void
typeresolution_info::visit_block (block* e)
{
  for (unsigned i=0; i<e->statements.size(); i++)
    {
      t = pe_unknown;
      e->statements[i]->visit (this);
    }
}


void
typeresolution_info::visit_try_block (try_block* e)
{
  if (e->try_block)
    e->try_block->visit (this);
  if (e->catch_error_var)
    {
      t = pe_string;
      e->catch_error_var->visit (this);
    }
  if (e->catch_block)
    e->catch_block->visit (this);
}


void
typeresolution_info::visit_embeddedcode (embeddedcode* s)
{
  // PR11573.  If we have survived thus far with a piece of embedded
  // code that requires uprobes, we need to track this.
  //
  // This is an odd place for this check, as opposed
  // to a separate 'optimization' pass, or c_unparser::visit_embeddedcode
  // over yonder in pass 3.  However, we want to do it during pass 2 so
  // that cached sessions also get the uprobes treatment.
  if (! session.need_uprobes
      && s->code.find("/* pragma:uprobes */") != string::npos)
    {
      if (session.verbose > 2)
        clog << _("Activating uprobes support because /* pragma:uprobes */ seen.") << endl;
      session.need_uprobes = true;
    }

  // PR15065. Likewise, we need to detect /* pragma:tagged_dfa */
  // before the gen_dfa_table pass. Again, the typechecking part of
  // pass 2 is a good place for this.
  if (! session.need_tagged_dfa
      && s->code.find("/* pragma:tagged_dfa */") != string::npos)
    {
      // if (session.verbose > 2)
      //   clog << _F("Turning on DFA subexpressions, pragma:tagged_dfa found in %s",
      // current_function->name.c_str()) << endl;
      // session.need_tagged_dfa = true;
      throw SEMANTIC_ERROR (_("Tagged DFA support is not yet available"), s->tok);
    }
}


void
typeresolution_info::visit_if_statement (if_statement* e)
{
  t = pe_long;
  e->condition->visit (this);

  t = pe_unknown;
  e->thenblock->visit (this);

  if (e->elseblock)
    {
      t = pe_unknown;
      e->elseblock->visit (this);
    }
}


void
typeresolution_info::visit_for_loop (for_loop* e)
{
  t = pe_unknown;
  if (e->init) e->init->visit (this);
  t = pe_long;
  e->cond->visit (this);
  t = pe_unknown;
  if (e->incr) e->incr->visit (this);
  t = pe_unknown;
  e->block->visit (this);
}


void
typeresolution_info::visit_foreach_loop (foreach_loop* e)
{
  // See also visit_arrayindex.
  // This is different in that, being a statement, we can't assign
  // a type to the outer array, only propagate to/from the indexes

  // if (e->referent->index_types.size() == 0)
  //   // redesignate referent as array
  //   e->referent->set_arity (e->indexes.size ());

  exp_type wanted_value = pe_unknown;
  symbol *array = NULL;
  hist_op *hist = NULL;
  classify_indexable(e->base, array, hist);

  if (hist)
    {
      if (e->indexes.size() != 1)
	unresolved (e->tok);
      t = pe_long;
      e->indexes[0]->visit (this);
      if (e->indexes[0]->type != pe_long)
	unresolved (e->tok);
      hist->visit (this);
      wanted_value = pe_long;
    }
  else
    {
      assert (array);
      if (e->indexes.size() != array->referent->index_types.size())
	unresolved (e->tok); // symbol resolution should prevent this
      else
        {
          for (unsigned i=0; i<e->indexes.size(); i++)
            {
              expression* ee = e->indexes[i];
              exp_type& ft = array->referent->index_types [i];
              t = ft;
              ee->visit (this);
              exp_type at = ee->type;

              if ((at == pe_string || at == pe_long) && ft == pe_unknown)
                {
                  // propagate to formal type
                  ft = at;
                  resolved (ee->tok, ee->type, array->referent, i);
                }
              if (at == pe_stats)
                invalid (ee->tok, at);
              if (ft == pe_stats)
                invalid (ee->tok, ft);
              if (at != pe_unknown && ft != pe_unknown && ft != at)
                mismatch (ee->tok, ee->type, array->referent, i);
              if (at == pe_unknown)
                unresolved (ee->tok);
            }
          for (unsigned i=0; i<e->array_slice.size(); i++)
            if (e->array_slice[i])
              {
                expression* ee = e->array_slice[i];
                exp_type& ft = array->referent->index_types [i];
                t = ft;
                ee->visit (this);
                exp_type at = ee->type;

                if ((at == pe_string || at == pe_long) && ft == pe_unknown)
                  {
                    // propagate to formal type
                    ft = at;
                    resolved (ee->tok, ee->type, array->referent, i);
                  }
                if (at == pe_stats)
                  invalid (ee->tok, at);
                if (ft == pe_stats)
                  invalid (ee->tok, ft);
                if (at != pe_unknown && ft != pe_unknown && ft != at)
                  mismatch (ee->tok, ee->type, array->referent, i);
                if (at == pe_unknown)
                  unresolved (ee->tok);
              }
        }
      t = pe_unknown;
      array->visit (this);
      wanted_value = array->type;
    }

  if (e->value)
    {
      if (wanted_value == pe_stats)
        invalid(e->value->tok, wanted_value);
      else if (wanted_value != pe_unknown)
        check_arg_type(wanted_value, e->value);
      else
        {
          t = pe_unknown;
          e->value->visit (this);
        }
    }

  /* Prevent @sum etc. aggregate sorting on non-statistics arrays. */
  if (wanted_value != pe_unknown)
    if (e->sort_aggr != sc_none && wanted_value != pe_stats)
      invalid (array->tok, wanted_value);

  if (e->limit)
    {
      t = pe_long;
      e->limit->visit (this);
    }

  t = pe_unknown;
  e->block->visit (this);
}


void
typeresolution_info::visit_null_statement (null_statement*)
{
}


void
typeresolution_info::visit_expr_statement (expr_statement* e)
{
  t = pe_unknown;
  e->value->visit (this);
}


struct delete_statement_typeresolution_info:
  public throwing_visitor
{
  typeresolution_info *parent;
  delete_statement_typeresolution_info (typeresolution_info *p):
    throwing_visitor (_("invalid operand of delete expression")),
    parent (p)
  {}

  void visit_arrayindex (arrayindex* e)
  {
    parent->visit_arrayindex (e);
  }

  void visit_symbol (symbol* e)
  {
    exp_type ignored = pe_unknown;
    assert (e->referent != 0);
    resolve_2types (e, e->referent, parent, ignored);
  }
};


void
typeresolution_info::visit_delete_statement (delete_statement* e)
{
  delete_statement_typeresolution_info di (this);
  t = pe_unknown;
  e->value->visit (&di);
}


void
typeresolution_info::visit_next_statement (next_statement*)
{
}


void
typeresolution_info::visit_break_statement (break_statement*)
{
}


void
typeresolution_info::visit_continue_statement (continue_statement*)
{
}


void
typeresolution_info::visit_array_in (array_in* e)
{
  // all unary operators only work on numerics
  exp_type t1 = t;
  t = pe_unknown; // array value can be anything
  e->operand->visit (this);

  if (t1 == pe_unknown && e->type != pe_unknown)
    ; // already resolved
  else if (t1 == pe_string || t1 == pe_stats)
    mismatch (e->tok, t1, pe_long);
  else if (e->type == pe_unknown)
    {
      e->type = pe_long;
      resolved (e->tok, e->type);
    }
}


void
typeresolution_info::visit_return_statement (return_statement* e)
{
  // This is like symbol, where the referent is
  // the return value of the function.

  // translation pass will print error
  if (current_function == 0)
    return;

  exp_type& e_type = current_function->type;
  t = current_function->type;
  e->value->visit (this);

  if (e_type != pe_unknown && e->value->type != pe_unknown
      && e_type != e->value->type)
    mismatch (e->value->tok, e->value->type, current_function);
  if (e_type == pe_unknown &&
      (e->value->type == pe_long || e->value->type == pe_string))
    {
      // propagate non-statistics from value
      e_type = e->value->type;
      resolved (e->value->tok, e_type, current_function);
    }
  if (e->value->type == pe_stats)
    invalid (e->value->tok, e->value->type);

  const exp_type_ptr& value_type = e->value->type_details;
  if (value_type && current_function->type == e->value->type)
    {
      exp_type_ptr& func_type = current_function->type_details;
      if (!func_type)
        // The function can take on the type details of the return value.
        resolved_details(value_type, func_type);
      else if (*func_type != *value_type && *func_type != *null_type)
        // Conflicting return types?  NO TYPE FOR YOU!
        resolved_details(null_type, func_type);
    }
}

void
typeresolution_info::visit_print_format (print_format* e)
{
  size_t unresolved_args = 0;

  if (e->hist)
    {
      e->hist->visit(this);
    }

  else if (e->print_with_format)
    {
      // If there's a format string, we can do both inference *and*
      // checking.

      // First we extract the subsequence of formatting components
      // which are conversions (not just literal string components)

      unsigned expected_num_args = 0;
      std::vector<print_format::format_component> components;
      for (size_t i = 0; i < e->components.size(); ++i)
	{
	  if (e->components[i].type == print_format::conv_unspecified)
	    throw SEMANTIC_ERROR (_("Unspecified conversion in print operator format string"),
				  e->tok);
	  else if (e->components[i].type == print_format::conv_literal)
	    continue;
	  components.push_back(e->components[i]);
	  ++expected_num_args;
	  if (e->components[i].widthtype == print_format::width_dynamic)
	    ++expected_num_args;
	  if (e->components[i].prectype == print_format::prec_dynamic)
	    ++expected_num_args;
	}

      // Then we check that the number of conversions and the number
      // of args agree.

      if (expected_num_args != e->args.size())
	throw SEMANTIC_ERROR (_("Wrong number of args to formatted print operator"),
			      e->tok);

      // Then we check that the types of the conversions match the types
      // of the args.
      unsigned argno = 0;
      for (size_t i = 0; i < components.size(); ++i)
	{
	  // Check the dynamic width, if specified
	  if (components[i].widthtype == print_format::width_dynamic)
	    {
	      check_arg_type (pe_long, e->args[argno]);
	      ++argno;
	    }

	  // Check the dynamic precision, if specified
	  if (components[i].prectype == print_format::prec_dynamic)
	    {
	      check_arg_type (pe_long, e->args[argno]);
	      ++argno;
	    }

	  exp_type wanted = pe_unknown;

	  switch (components[i].type)
	    {
	    case print_format::conv_unspecified:
	    case print_format::conv_literal:
	      assert (false);
	      break;

	    case print_format::conv_pointer:
	    case print_format::conv_number:
	    case print_format::conv_binary:
	    case print_format::conv_char:
	    case print_format::conv_memory:
	    case print_format::conv_memory_hex:
	      wanted = pe_long;
	      break;

	    case print_format::conv_string:
	      wanted = pe_string;
	      break;
	    }

	  assert (wanted != pe_unknown);
	  check_arg_type (wanted, e->args[argno]);
	  ++argno;
	}
    }
  else
    {
      // Without a format string, the best we can do is require that
      // each argument resolve to a concrete type.
      for (size_t i = 0; i < e->args.size(); ++i)
	{
	  t = pe_unknown;
	  e->args[i]->visit (this);
	  if (e->args[i]->type == pe_unknown)
	    {
	      unresolved (e->args[i]->tok);
	      ++unresolved_args;
	    }
	}
    }

  if (unresolved_args == 0)
    {
      if (e->type == pe_unknown)
	{
	  if (e->print_to_stream)
	    e->type = pe_long;
	  else
	    e->type = pe_string;
	  resolved (e->tok, e->type);
	}
    }
  else
    {
      e->type = pe_unknown;
      unresolved (e->tok);
    }
}


void
typeresolution_info::visit_stat_op (stat_op* e)
{
  t = pe_stats;
  e->stat->visit (this);
  if (e->type == pe_unknown)
    {
      e->type = pe_long;
      resolved (e->tok, e->type);
    }
  else if (e->type != pe_long)
    mismatch (e->tok, pe_long, e->type);
}

void
typeresolution_info::visit_hist_op (hist_op* e)
{
  t = pe_stats;
  e->stat->visit (this);
}


void
typeresolution_info::check_arg_type (exp_type wanted, expression* arg)
{
  t = wanted;
  arg->visit (this);

  if (arg->type == pe_unknown)
    {
      arg->type = wanted;
      resolved (arg->tok, arg->type);
    }
  else if (arg->type != wanted)
    {
      mismatch (arg->tok, wanted, arg->type);
    }
}


void
typeresolution_info::check_local (vardecl* v)
{
  if (v->arity != 0)
    {
      num_still_unresolved ++;
      if (assert_resolvability)
        session.print_error
          (SEMANTIC_ERROR (_("array locals not supported, missing global declaration? "), v->tok));
    }

  if (v->type == pe_unknown)
    unresolved (v->tok);
  else if (v->type == pe_stats)
    {
      num_still_unresolved ++;
      if (assert_resolvability)
        session.print_error
          (SEMANTIC_ERROR (_("stat locals not supported, missing global declaration? "), v->tok));
    }
  else if (!(v->type == pe_long || v->type == pe_string))
    invalid (v->tok, v->type);
}


void
typeresolution_info::unresolved (const token* tok)
{
  num_still_unresolved ++;

  if (assert_resolvability && mismatch_complexity <= 0)
    {
      stringstream msg;
      msg << _("unresolved type ");
      session.print_error (SEMANTIC_ERROR (msg.str(), tok));
    }
}


void
typeresolution_info::invalid (const token* tok, exp_type pe)
{
  num_still_unresolved ++;

  if (assert_resolvability)
    {
      stringstream msg;
      if (tok && tok->type == tok_operator)
        msg << _("invalid operator");
      else
        msg << _("invalid type ") << pe;
      session.print_error (SEMANTIC_ERROR (msg.str(), tok));
    }
}

void
typeresolution_info::mismatch (const binary_expression* e)
{
  num_still_unresolved ++;

  if (assert_resolvability && mismatch_complexity <= 1)
    {
      stringstream msg;
      msg << _F("type mismatch: left and right sides don't agree (%s vs %s)",
                lex_cast(e->left->type).c_str(), lex_cast(e->right->type).c_str());
      session.print_error (SEMANTIC_ERROR (msg.str(), e->tok));
    }
  else if (!assert_resolvability)
    mismatch_complexity = max(1, mismatch_complexity);
}

/* tok   token where mismatch occurred
 * t1    type we expected (the 'good' type)
 * t2    type we received (the 'bad' type)
 * */
void
typeresolution_info::mismatch (const token* tok, exp_type t1, exp_type t2)
{
  num_still_unresolved ++;

  if (assert_resolvability && mismatch_complexity <= 2)
    {
      stringstream msg;
      msg << _F("type mismatch: expected %s", lex_cast(t1).c_str());
      if (t2 != pe_unknown)
        msg << _F(" but found %s", lex_cast(t2).c_str());
      session.print_error (SEMANTIC_ERROR (msg.str(), tok));
    }
  else if (!assert_resolvability)
    mismatch_complexity = max(2, mismatch_complexity);
}

/* tok   token where the mismatch happened
 * type  type we received (the 'bad' type)
 * decl  declaration of mismatched symbol
 * index if index-based (array index or function arg)
 * */
void
typeresolution_info::mismatch (const token *tok, exp_type type,
                               const symboldecl* decl, int index)
{
  num_still_unresolved ++;

  if (assert_resolvability && mismatch_complexity <= 3)
    {
      assert(decl != NULL);

      // If mismatch is against a function parameter from within the function
      // itself (rather than a function call), then the index will be -1. We
      // check here if the decl corresponds to one of the params and if so,
      // adjust the index.
      if (current_function != NULL && index == -1)
        {
          vector<vardecl*>& args = current_function->formal_args;
          for (unsigned i = 0; i < args.size() && index < 0; i++)
            if (args[i] == decl)
              index = i;
        }

      // get the declaration's original type and token
      const resolved_type *original = NULL;
      for (vector<resolved_type>::const_iterator it = resolved_types.begin();
           it != resolved_types.end() && original == NULL; ++it)
        {
          if (it->decl == decl && it->index == index)
            original = &(*it);
        }

      // print basic mismatch msg if we couldn't find the decl (this can happen
      // for explicitly typed decls e.g. myvar:long or for fabricated (already
      // resolved) decls e.g. __perf_read_*)
      if (original == NULL)
        {
          session.print_error (SEMANTIC_ERROR (
            _F("type mismatch: expected %s but found %s",
               lex_cast(type).c_str(),
               lex_cast(decl->type).c_str()),
            tok));
          return;
        }

      // print where mismatch happened and chain with origin of decl type
      // resolution
      stringstream msg;

      if (index >= 0)
        msg << _F("index %d ", index);
      msg << _F("type mismatch (%s)", lex_cast(type).c_str());
      semantic_error err(ERR_SRC, msg.str(), tok);

      stringstream chain_msg;
      chain_msg << _("type");
      if (index >= 0)
        chain_msg << _F(" of index %d", index);
      chain_msg << _F(" was first inferred here (%s)",
                      lex_cast(decl->type).c_str());
      semantic_error chain(ERR_SRC, chain_msg.str(), original->tok);

      err.set_chain(chain);
      session.print_error (err);
    }
  else if (!assert_resolvability)
    mismatch_complexity = max(3, mismatch_complexity);
}


/* tok   token where resolution occurred
 * type  type to which we resolved
 * decl  declaration of resolved symbol
 * index if index-based (array index or function arg)
 * */
void
typeresolution_info::resolved (const token *tok, exp_type,
                               const symboldecl* decl, int index)
{
  num_newly_resolved ++;

  // We only use the resolved_types vector to give better mismatch messages
  // involving symbols. So don't bother adding it if we're not given a decl
  if (decl != NULL)
    {
      // As a fail-safe, if the decl & index is already in the vector, then
      // modify it instead of adding another one to ensure uniqueness. This
      // should never happen since we only call resolved once for each decl &
      // index, but better safe than sorry. (IE. if it does happen, better have
      // the latest resolution info for better mismatch reporting later).
      for (unsigned i = 0; i < resolved_types.size(); i++)
        {
          if (resolved_types[i].decl == decl
              && resolved_types[i].index == index)
            {
              resolved_types[i].tok = tok;
              return;
            }
        }
      resolved_type res(tok, decl, index);
      resolved_types.push_back(res);
    }
}

void
typeresolution_info::resolved_details (const exp_type_ptr& src,
                                       exp_type_ptr& dest)
{
  num_newly_resolved ++;
  dest = src;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
