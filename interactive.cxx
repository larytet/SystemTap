// systemtap interactive mode
// Copyright (C) 2015 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include "interactive.h"
#include "session.h"
#include "util.h"

#include "stap-probe.h"

#include <cstdlib>

using namespace std;

extern "C" {
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
}

// FIXME: these declarations don't really belong here.
extern int
passes_0_4 (systemtap_session &s);
extern int
pass_5 (systemtap_session &s, vector<remote*> targets);

// Class that describes an interactive command or an option for the
// set/show commands.
class cmdopt
{
protected:
  string _help_text;

public:
  string name;				// command/option name 
  string usage;				// command usage (includes options)

  // help_text() returns the help text for a command/option
  virtual string help_text(size_t indent) const { return _help_text; }

  // handler() is the code associated with a command/option
  virtual bool handler(systemtap_session &s, vector<string> &tokens) = 0;
};

typedef vector<cmdopt*> cmdopt_vector;
typedef vector<cmdopt*>::const_iterator cmdopt_vector_const_iterator;
typedef vector<cmdopt*>::iterator cmdopt_vector_iterator;

// A vector of all commands.
static cmdopt_vector commands;
// A vector of all commands that take options.
static cmdopt_vector option_commands;
// A vector of all options;
static cmdopt_vector options;

struct match_item;
typedef std::map<std::string, match_item*> match_item_map;

struct match_item
{
    string match_text;
    string regexp;
    bool terminal;
    match_item_map sub_matches;

    match_item() { terminal = false; }
    bool full_match(const string &text);
    bool partial_match(const string &text);
};

bool
match_item::full_match(const string &text)
{
    if (regexp.empty())
	return (text == match_text);

    vector<string> matches;
    size_t len = match_text.length();
    if (len < text.length() && text.substr(0, len) == match_text
	&& regexp_match(text.substr(len), regexp, matches) == 0)
	return true;
    return false;
}

bool
match_item::partial_match(const string &text)
{
    // You can't really do a partial regexp match, so we won't even
    // try. Just match the starting static text of the match_item.
    return (match_text.compare(0, text.length(), text) == 0);
}

typedef std::map<std::string, match_item*>::const_iterator match_item_map_const_iterator;
typedef std::map<std::string, match_item*>::iterator match_item_map_iterator;

static match_item_map probe_map;

static string saved_token;

static void interactive_usage();

//
// Supported commands.
// 

class help_cmd: public cmdopt
{
public:
  help_cmd()
  {
    name = usage = "!help";
    _help_text = "Print this command list";
  }
  virtual bool handler(systemtap_session &s, vector<string> &tokens)
  {
    interactive_usage();
    return false;
  }
};

class list_cmd : public cmdopt
{
public:
  list_cmd()
  {
    name = usage = "!list";
    _help_text = "Display the current script";
  }
  bool handler(systemtap_session &s, vector<string> &tokens)
  {
    // FIXME: Hmm, we might want to use 'printscript' here...
    if (s.have_script)
      cout << s.cmdline_script << endl;
    else
      cout << "(No script input.)" << endl;
    return false;
  }
};

class set_cmd: public cmdopt
{
public:
  set_cmd()
  {
    name = "!set";
    usage = "!set OPTION VALUE";
    _help_text = "Set option value. Supported options are:";
  }
  string help_text(size_t indent) const
  {
    ostringstream buffer;
    size_t width = 1;

    // Find biggest option "name" field.
    for (cmdopt_vector_const_iterator it = options.begin();
	 it != options.end(); ++it)
      {
	if ((*it)->name.size() > width)
	  width = (*it)->name.size();
      }

    // Add each option to the output.
    buffer << _help_text;
    for (cmdopt_vector_iterator it = options.begin();
	 it != options.end(); ++it)
      {
	buffer << endl << setw(indent + 2) << " ";
	buffer << setw(width) << left << (*it)->name << " -- "
	       << (*it)->help_text(0);
      }
    return buffer.str();
  }
  bool handler(systemtap_session &s, vector<string> &tokens)
  {
    bool option_found = false;
    if (tokens.size() != 3)
      {
	cout << endl << "Invalid command" << endl;
	interactive_usage();
	return false;
      }

    // Search the option list for the option to display.
    for (cmdopt_vector_iterator it = options.begin();
	 it != options.end(); ++it)
      {
	if (tokens[1] == (*it)->name)
	{
	  option_found = true;
	  (*it)->handler(s, tokens);
	  break;
	}
      }
    if (!option_found)
      {
	cout << "Invalid option name" << endl;
	interactive_usage();
      }
    return false;
  }
};

class show_cmd: public cmdopt
{
public:
  show_cmd()
  {
    name = "!show";
    usage = "!show OPTION";
    _help_text = "Show option value";
  }
  bool handler(systemtap_session &s, vector<string> &tokens)
  {
    bool option_found = false;
    if (tokens.size() != 2)
      {
	cout << endl << "Invalid command" << endl;
	interactive_usage();
	return false;
      }

    // Search the option list for the option to display.
    for (cmdopt_vector_iterator it = options.begin();
	 it != options.end(); ++it)
      {
	if (tokens[1] == (*it)->name)
	  {
	    option_found = true;
	    (*it)->handler(s, tokens);
	    break;
	  }
      }
    if (!option_found)
      {
	cout << "Invalid option name" << endl;
	interactive_usage();
      }
    return false;
  }
};

class quit_cmd : public cmdopt
{
public:
  quit_cmd()
  {
    name = usage = "!quit";
    _help_text = "Quit systemtap";
  }
  bool handler(systemtap_session &s, vector<string> &tokens)
  {
    return true;
  }
};

//
// Supported options for the "!set" and "!show" commands.
// 

class keep_tmpdir_opt: public cmdopt
{
public:
  keep_tmpdir_opt()
  {
    name = "keep_tmpdir";
    _help_text = "Keep temporary directory";
  }
  bool handler(systemtap_session &s, vector<string> &tokens)
  {
    bool set = (tokens[0] == "!set");
    if (set)
      s.keep_tmpdir = (tokens[2] != "0");
    else
      cout << name << ": " << s.keep_tmpdir << endl;
    return false;
  }
};

class last_pass_opt: public cmdopt
{
public:
  last_pass_opt()
  {
      name = "last_pass";
      _help_text = "Stop after pass NUM 1-5";
  }
  bool handler(systemtap_session &s, vector<string> &tokens)
  {
    bool set = (tokens[0] == "!set");
    if (set)
      {
	char *end;
	long val;

	errno = 0;
	val = strtol (tokens[2].c_str(), &end, 10);
	if (errno != 0 || *end != '\0' || val < 1 || val > 5)
	  cout << "Invalid option value (should be 1-5)" << endl;
	else
	  s.last_pass = val;
      }
    else
      cout << name << ": " << s.last_pass << endl;
    return false;
  }
};

class verbose_opt: public cmdopt
{
public:
  verbose_opt()
  {
    name = "verbose";
    _help_text = "Add verbosity to all passes";
  }
  bool handler(systemtap_session &s, vector<string> &tokens)
  {
    bool set = (tokens[0] == "!set");
    if (set)
      {
	char *end;
	long val;

	errno = 0;
	val = strtol (tokens[2].c_str(), &end, 10);
	if (errno != 0 || *end != '\0' || val < 0)
	  cout << "Invalid option value (should be greater than 0)" << endl;
	else
	  {
	    s.verbose = val;
	    for (unsigned i=0; i<5; i++)
	      s.perpass_verbose[i] = val;
	  }
      }
    else
      cout << name << ": " << s.verbose << endl;
    return false;
  }
};

static void
interactive_usage ()
{
  cout << "List of commands:" << endl << endl;

  // Find biggest "usage" field.
  size_t width = 1;
  for (cmdopt_vector_const_iterator it = commands.begin();
       it != commands.end(); ++it)
    {
      if ((*it)->usage.size() > width)
	  width = (*it)->usage.size();
    }
  // Print usage field and help text for each command.
  for (cmdopt_vector_const_iterator it = commands.begin();
       it != commands.end(); ++it)
    {
      cout << setw(width) << left << (*it)->usage << " -- "
	   << (*it)->help_text(width + 4) << endl;
    }
}

// Generator function for command completion.  STATE lets us know
// whether to start from scratch; without any state (i.e. STATE == 0),
// then we start at the top of the list.
static char *
command_generator(const char *text, int state)
{
  static size_t list_index, len;
  static bool interactive_cmd = false;

  // If this is a new word to complete, initialize now.  This includes
  // saving the length of TEXT for efficiency, and initializing the
  // index variable to 0.
  if (!state)
  {
    list_index = 0;
    len = strlen(text);
    interactive_cmd = (len > 0 && text[0] == '!');
  }

  // For now, *only* complete interactive mode commands themselves.
  if (interactive_cmd)
  {
    // Return the next name which partially matches from the command list.
    while (list_index < commands.size())
    {
      cmdopt *cmd = commands[list_index];
      list_index++;
      if (strncmp(cmd->name.c_str(), text, len) == 0)
	return strdup(cmd->name.c_str());
    }
  }

  // If no names matched, then return NULL.
  return NULL;
}

// Generator function for option completion.  STATE lets us know
// whether to start from scratch; without any state (i.e. STATE == 0),
// then we start at the top of the list.
static char *
option_generator(const char *text, int state)
{
  static size_t list_index, len;

  // If this is a new word to complete, initialize now.  This includes
  // saving the length of TEXT for efficiency, and initializing the
  // index variable to 0.
  if (!state)
  {
    list_index = 0;
    len = strlen(text);
  }

  // Return the next name which partially matches from the option list.
  while (list_index < options.size())
  {
    cmdopt *opt = options[list_index];
    list_index++;
    if (strncmp(opt->name.c_str(), text, len) == 0)
      return strdup(opt->name.c_str());
  }

  // If no names matched, then return NULL.
  return NULL;
}

static void
descend_tree(match_item_map &map, const string &prefix, vector<string> &matches)
{
    for (match_item_map_const_iterator it = map.begin(); it != map.end(); ++it)
    {
	match_item *item = it->second;

	if (item->terminal)
	    matches.push_back(prefix + "." + it->first);
	if (!item->sub_matches.empty())
	{
	    string new_prefix = prefix + "." + it->first;
	    descend_tree(item->sub_matches, new_prefix, matches);
	}
    }
}

static void
partial_matches(const char *text, match_item_map &map, vector<string> &matches)
{
    for (match_item_map_const_iterator it = map.begin(); it != map.end(); ++it)
    {
	match_item *item = it->second;

	if (item->partial_match(text))
	{
	    if (item->terminal)
		matches.push_back(it->first);
	    if (!item->sub_matches.empty())
		descend_tree(item->sub_matches, it->first, matches);
	}
    }
}

static char *
probe_generator(const char *text, int state)
{
  static vector<string> matches;
  static size_t list_index;

  // If this is a new word to complete, initialize everything we need.
  if (!state)
  {
    // OK, so this is the first time we're trying to expand this
    // word. We only get the last "word", but we need to know where we
    // are in the probe expansion. For example, is someone trying to
    // expand "ker", "fun" from "kernel.fun", or "re" from
    // "kernel.function("sys_foo").re"?
    //
    // We're going to "cheat" here, and reuse the 2nd token of the
    // line from where interactive_completion() saved it for us. We're
    // going to break down the 2nd token into its components.
    vector<string> tokens;
    tokenize(saved_token, tokens, ".");
	
    match_item_map *match_map = &probe_map;
    for (vector<string>::const_iterator it = tokens.begin();
	 it != tokens.end(); ++it)
    {
	bool found = false;
	for (match_item_map_const_iterator map_it = match_map->begin();
	     map_it != match_map->end(); ++map_it)
	{
	    if (map_it->second->full_match(*it))
	    {
		found = true;
#ifdef DEBUG
		clog << "found " << (*it) << endl;
#endif
		match_map = &(map_it->second->sub_matches);
		break;
	    }
	}
	if (! found)
	    break;
    }

    // Now we're at the right match_item sub_matches map. Process it.
    matches.clear();
    partial_matches(text, *match_map, matches);
    list_index = 0;
  }

  // Return the next list item.
  if (list_index < matches.size())
  {
      char *str = strdup(matches[list_index].c_str());
      list_index++;
#ifdef DEBUG
      clog << "match: " << str << endl;
#endif
      return str;
  }

  // If no names matched, then return NULL.
  return NULL;
}

// Attempt to complete on the contents of TEXT.  START and END bound
// the region of rl_line_buffer that contains the word to complete.
// TEXT is the word to complete.  We can use the entire contents of
// rl_line_buffer in case we want to do some simple parsing.  Return
// the array of matches, or NULL if there aren't any.
static char **
interactive_completion(const char *text, int start, int end)
{
  char **matches = (char **)NULL;

  // Setting 'rl_attempted_completion_over' to non-zero means to
  // suppress normal filename completion after the user-specified
  // completion function has been called.
  rl_attempted_completion_over = 1;

  // If this word is at the start of the line, then it is a command to
  // complete.
  if (start == 0)
    matches = rl_completion_matches(text, command_generator);
  else
  {
    const string delimiters = " \t";
    vector<string> tokens;

    tokenize(rl_line_buffer, tokens, delimiters);
    if (! tokens.size())
      return matches;

    if (tokens.size() <= 2)
    {
      // If we're in a command that takes options, then we've got an
      // option to complete, if we're on the 2nd token.
      for (cmdopt_vector_const_iterator it = option_commands.begin();
	   it != option_commands.end(); ++it)
      {
	if ((*it)->name == tokens[0])
	{
	  matches = rl_completion_matches(text, option_generator);
	  break;
	}
      }

      // Perhaps we're in a probe declaration.
      if (tokens[0] == "probe")
      {
	  // Save 2nd token for use by probe_generator().
	  saved_token = tokens[1];
	  matches = rl_completion_matches(text, probe_generator);
      }
    }
  }
  return matches;
}

static void
process_probe_list(istream &probe_stream, bool handle_regexps)
{
    while (! probe_stream.eof())
    {
	string line;
	match_item_map *match_map = &probe_map;

	getline(probe_stream, line);
	if (line.empty())
	    continue;

	vector<string> tokens;
	tokenize(line, tokens, ".");

#ifdef DEBUG
	clog << "processing " << line << endl;
#endif
	for (vector<string>::const_iterator it = tokens.begin();
	     it != tokens.end(); ++it)
	{
	    if (match_map->count(*it) == 0)
	    {
		match_item *mi = new match_item;
		size_t number_pos = string::npos;
		size_t string_pos = string::npos;
	      
		if (handle_regexps)
		{
		    number_pos = (*it).find("(number)");
		    string_pos = (*it).find("(string)");
		}
		if (number_pos != string::npos)
		{
		    mi->match_text = (*it).substr(0, number_pos);
		    mi->regexp = "^\\([x0-9a-fA-F]+\\)$";
		}
		else if (string_pos != string::npos)
		{
		    mi->match_text = (*it).substr(0, string_pos);
		    mi->regexp = "^\\(\"[^\"]+\"\\)$";
		}
		else
		{
		    mi->match_text = (*it);
		}
		(*match_map)[*it] = mi;
		match_map = &(mi->sub_matches);
		mi->terminal = (*it == tokens.back());
	    }
	    else
	    {
		match_map = &((*match_map)[*it]->sub_matches);
	    }
	}
    }
}

//
// Interactive mode, passes 0 through 5 and back again.
//

int
interactive_mode (systemtap_session &s, vector<remote*> targets)
{
  int rc;
  string delimiters = " \t";
  bool input_handled;

  // Tell readline's completer we want a crack at the input first.
  rl_attempted_completion_function = interactive_completion;

  // FIXME: this has been massively simplified from the default.
  rl_completer_word_break_characters = (char *)" \t\n.{";

  // Set up command list, along with a list of commands that take
  // options.
  commands.push_back(new help_cmd);
  commands.push_back(new list_cmd);
  commands.push_back(new set_cmd);
  option_commands.push_back(commands.back());
  commands.push_back(new show_cmd);
  option_commands.push_back(commands.back());
  commands.push_back(new quit_cmd);

  // Set up !set/!show option list.
  options.push_back(new keep_tmpdir_opt);
  options.push_back(new last_pass_opt);
  options.push_back(new verbose_opt);

  // Get the list of "base" probe types, the same output you'd get
  // from doing 'stap --dump-probe-types'.
  s.clear_script_data();
  systemtap_session* ss = s.clone(s.architecture, s.kernel_release);
  stringstream probes;

  ss->verbose = 0;
  for (unsigned i=0; i<5; i++)
      ss->perpass_verbose[i] = 0;
  ss->last_pass = 2;
  ss->dump_mode = systemtap_session::dump_probe_types;

  // We want to capture the output of pattern_root->dump(), which
  // normally goes to 'cout'. So, redirect where 'cout' goes.
  streambuf *former_buff = cout.rdbuf(probes.rdbuf());
  passes_0_4(*ss);

  // Restore cout.
  cout.rdbuf(former_buff);

  // Now that we have the list of "base" probe types, call
  // process_probe_list() to turn that into our parse tree.
  process_probe_list(probes, true);

  // FIXME: Now we'll need to get all the probe aliases ("stap
  // --dump-probe-aliases").

#ifdef DEBUG
  {
      vector<string> matches;

      // Print tree.
      clog << "Dumping tree:" << endl;
      partial_matches("", probe_map, matches);
      for (vector<string>::const_iterator it = matches.begin();
	   it != matches.end(); ++it)
      {
	  clog << (*it) << endl;
      }
  }
#endif

  while (1)
    {
      char *line_tmp = readline("stap> ");
      if (line_tmp && *line_tmp)
	add_history(line_tmp);
      else
	continue;

      string line = string(line_tmp);
      free(line_tmp);

      vector<string> tokens;
      tokenize(line, tokens, delimiters);

      input_handled = false;
      if (tokens.size())
        {
	  bool quit = false;
	  // Search list for command to execute.
	  for (cmdopt_vector_iterator it = commands.begin();
	       it != commands.end(); ++it)
	    {
	      if (tokens[0] == (*it)->name)
	        {
		  input_handled = true;
		  quit = (*it)->handler(s, tokens);
		  break;
		}
	    }
	    
	  if (input_handled && quit)
	    break;
	}

      // If it isn't a command, we assume it is a script to run.
      //
      // FIXME: Later this could be a line from a script that we have
      // to keep track of.
      if (!input_handled)
        {
	  // Try creating a new systemtap session object so that we
	  // don't get leftovers from the last script we compiled.
	  s.clear_script_data();
	  systemtap_session* ss = s.clone(s.architecture, s.kernel_release);

	  ss->cmdline_script = line;
	  ss->have_script = true;
	  rc = passes_0_4(*ss);

	  // Run pass 5, if passes 0-4 worked.
	  if (rc == 0 && ss->last_pass >= 5 && !pending_interrupts)
	    rc = pass_5 (*ss, targets);
	  ss->reset_tmp_dir();
	}
    }
  return 0;
}
