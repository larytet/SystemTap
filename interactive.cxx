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
#include "staptree.h"
#include "parse.h"
#include "csclient.h"
#include "client-nss.h"

#include "stap-probe.h"

#include <cstdlib>
#include <stack>
#include <sstream>
#include <iterator>
#include <ext/stdio_filebuf.h>

using namespace std;
using namespace __gnu_cxx;

extern "C" {
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ctype.h>

#if HAVE_LIBSQLITE3
  #include <sqlite3.h>
#endif
}

// FIXME: these declarations don't really belong here.
extern int
passes_0_4 (systemtap_session &s);
extern int
pass_5 (systemtap_session &s, vector<remote*> targets);

static int
forked_passes_0_4 (systemtap_session &s);

static int
forked_parse_pass (systemtap_session &s, vector<string> &script);
static int
forked_semantic_pass (systemtap_session &s, vector<string> &script);

// Ask user a y-or-n question and return 1 iff answer is yes.  The
// prompt argument should end in "? ". Note that this is a simplified
// version of gdb's defaulted_query() function.
#if HAVE_LIBSQLITE3
struct metafile {
  string name;
  string title;
  string description;
  string path;
};
#endif

enum query_default { no_default,	// There isn't a default.
		     default_yes,	// The default is "yes".
		     default_no };	// THe default is "no".

int
query (const char *prompt, query_default qdefault)
{
  int def_value;
  char def_answer, not_def_answer;
  const char *y_string, *n_string;
  int retval;
    
  // Set up according to which answer is the default.
  if (qdefault == no_default)
    {
      def_value = 1;
      def_answer = 'Y';
      not_def_answer = 'N';
      y_string = "y";
      n_string = "n";
    }
  else if (qdefault == default_yes)
    {
      def_value = 1;
      def_answer = 'Y';
      not_def_answer = 'N';
      y_string = "[y]";
      n_string = "n";
    }
  else
    {
      def_value = 0;
      def_answer = 'N';
      not_def_answer = 'Y';
      y_string = "y";
      n_string = "[n]";
    }

  // If input isn't coming from the user directly, just say what
  // question we're asking, and then answer the default automatically.
  if (! isatty(fileno(stdin)))
    {
      clog << prompt
	   << _F("(%s or %s) [answered %c; input not from terminal]\n", 
		 y_string, n_string, def_answer);
      return def_value;
    }

  while (1)
    {
      char *response, answer;

      response = readline(_F("%s(%s or %s) ", prompt, y_string,
			     n_string).c_str());
      if (response == NULL)		// EOF
	{
	  clog << _F("EOF [assumed %c]\n", def_answer);
	  retval = def_value;
	  break;
	}

      answer = toupper(response[0]);
      free (response);

      // Check answer.  For the non-default, the user must specify the
      // non-default explicitly.
      if (answer == not_def_answer)
	{
	  retval = !def_value;
	  break;
	}
      // Otherwise, if a default was specified, the user may either
      // specify the required input or have it default by entering
      // nothing.
      if (answer == def_answer
	  || (qdefault != no_default && answer == '\0'))
	{
	  retval = def_value;
	  break;
	}
      // Invalid entries are not defaulted and require another selection.
      clog << _F("Please answer %s or %s.\n", y_string, n_string);
    }
  return retval;
}

// Class that describes an interactive command or an option for the
// set/show commands.
class cmdopt
{
protected:
  string _help_text;

public:
  virtual ~cmdopt() { }
  string name;				// command/option name 
  string usage;				// command usage (includes options)
  vector<string> aliases;

  // help_text() returns the help text for a command/option
  virtual string help_text(size_t indent __attribute ((unused))) const
  {
    return _help_text;
  }

  // handler() is the code associated with a command/option
  virtual bool handler(systemtap_session &s, vector<string> &tokens,
		       string &input) = 0;

  bool accept(const string& input) const;
};

bool cmdopt::accept(const string &input) const
{
  if (input == name)
    return true;
  for (auto it = aliases.begin(); it != aliases.end(); ++it)
    {
      if (input == *it)
        return true;
    }
  return false;
}

typedef vector<cmdopt*> cmdopt_vector;
typedef vector<cmdopt*>::const_iterator cmdopt_vector_const_iterator;
typedef vector<cmdopt*>::iterator cmdopt_vector_iterator;

// A vector of all commands.
static cmdopt_vector command_vec;
// A vector of all commands that take options.
static cmdopt_vector option_command_vec;
// A vector of all options;
static cmdopt_vector option_vec;
// A vector containing the user's script, one probe/function/etc. per
// string.
static vector<string> script_vec;
static bool auto_analyze = true;

struct match_item;
typedef map<string, match_item*> match_item_map;
typedef map<string, match_item*>::const_iterator match_item_map_const_iterator;
typedef map<string, match_item*>::iterator match_item_map_iterator;
typedef map<string, match_item*>::reverse_iterator match_item_map_rev_iterator;
typedef map<string, match_item*>::const_reverse_iterator match_item_map_const_rev_iterator;

struct match_item
{
    match_item() { terminal = false; }
    ~match_item();

    string match_text;
    string regexp;
    bool terminal;
    match_item_map sub_matches;

    bool full_match(const string &text);
    bool partial_match(const string &text);
};

static void
delete_match_map_items(match_item_map *map)
{
    match_item_map_iterator it = map->begin();
    while (it != map->end())
    {
	match_item *item = it->second;
	map->erase(it++);
	delete item;
    }
}

match_item::~match_item()
{
    delete_match_map_items(&sub_matches);
}

// match_item::full_match() looks for a "full" match. A full match
// completely matches an item. Examples are:
//
//    USER INPUT		MATCH ITEM
//    'kernel'			'kernel'
//    'kernel("sys_read")'	'kernel(string)'
//    'process(123)'		'process(number)'
//    'function("main")'	'function(string)'
//
// '(number)' and '(string)' matching are done via regexps.
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

// match_item::partial_match() looks for a "partial" match. A partial
// match looks like:
//
//    USER INPUT		MATCH ITEM
//    'ke'			'kernel',
//    'proc'			'process', 'process(number)', and
//				'process(string)'
bool
match_item::partial_match(const string &text)
{
    // You can't really do a partial regexp match, so we won't even
    // try. Just match the starting static text of the match_item.
    size_t len = text.length();
    if (len == 0)
	return true;
    return (match_text.compare(0, len, text) == 0);
}

static match_item_map probe_map;

static string saved_token;
static vector<remote*> *saved_targets;

static void interactive_usage();

//
// Supported commands.
// 

class help_cmd: public cmdopt
{
public:
  help_cmd()
  {
    name = usage = "help";
    aliases = { "h", "?" };
    _help_text = "Print command list or description of a specific command.";
  }
  bool handler(systemtap_session &s __attribute ((unused)),
	       vector<string> &tokens __attribute ((unused)),
	       string &input __attribute ((unused)))
  {
    tokens.erase(tokens.begin());
    if (tokens.size() == 0) {
        interactive_usage();
        return false;
    }

    for (auto it = command_vec.begin();
	 it != command_vec.end(); ++it)
      {
        if ((*it)->accept(tokens[0])) {
	  cout << (*it)->usage << ": " << (*it)->help_text((*it)->usage.size()) << endl;
          return false;
	}
      }

    cout <<"Undefined command \""<< tokens[0]<< "\". Try \"help\" or \"?\" for a full list of commands." << endl;
    return false;
  }
};

class list_cmd : public cmdopt
{
public:
  list_cmd()
  {
    name = usage = "list";
    aliases = { "l" };
    _help_text = "Display the current script.";
  }
  bool handler(systemtap_session &s __attribute ((unused)),
	       vector<string> &tokens __attribute ((unused)),
	       string &input __attribute ((unused)))
  {
    // FIXME: We will want to use 'printscript' here, once we store
    // parser output instead of strings.
    size_t width = 1;
    size_t len = script_vec.size();
    if (len >= 10000) { len /= 10000; width += 4; }
    if (len >= 100) { len /= 100; width += 2; }
    if (len >= 10) { len /= 10; width += 1; }
    size_t i = 1;
    for (vector<string>::const_iterator it = script_vec.begin();
	 it != script_vec.end(); ++it)
    {
	clog << setw(width) << i++ << ": " << (*it) << endl;
    }
    return false;
  }
};

class set_cmd: public cmdopt
{
public:
  set_cmd()
  {
    name = "set";
    usage = "set OPTION VALUE";
    _help_text = "Set option value. Supported options are:";
  }
  string help_text(size_t indent __attribute ((unused))) const
  {
    ostringstream buffer;
    size_t width = 1;

    // Find biggest option "name" field.
    for (cmdopt_vector_const_iterator it = option_vec.begin();
	 it != option_vec.end(); ++it)
      {
	if ((*it)->name.size() > width)
	  width = (*it)->name.size();
      }

    // Add each option to the output.
    buffer << _help_text;
    for (cmdopt_vector_iterator it = option_vec.begin();
	 it != option_vec.end(); ++it)
      {
	buffer << endl << setw(4) << " ";
	buffer << setw(width) << left << (*it)->name << " -- "
	       << (*it)->help_text(0);
      }
    return buffer.str();
  }
  bool handler(systemtap_session &s, vector<string> &tokens, string &input)
  {
    bool option_found = false;
    if (tokens.size() < 3)
      {
	cout  << "Invalid command" << endl;
	cout << usage << ": " << help_text(0) <<endl;
	return false;
      }

    // Search the option list for the option to display.
    for (cmdopt_vector_iterator it = option_vec.begin();
	 it != option_vec.end(); ++it)
      {
	if ((*it)->accept(tokens[1]))
	{
	  option_found = true;
	  (*it)->handler(s, tokens, input);
	  break;
	}
      }
    if (!option_found)
      {
	cout << "Invalid option name" << endl;
	cout << usage << ": " << help_text(0) <<endl;
      }
    return false;
  }
};

class show_cmd: public cmdopt
{
public:
  show_cmd()
  {
    name = "show";
    usage = "show OPTION";
    _help_text = "Show option value.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens, string &input)
  {
    bool option_found = false;

    if (tokens.size() == 1)
      {
        // Show all options.
	for (cmdopt_vector_iterator it = option_vec.begin();
	     it != option_vec.end(); ++it)
	  {
	    (*it)->handler(s, tokens, input);
	  }
	return false;
      }
    else if (tokens.size() != 2)
      {
	cout << endl << "Invalid command" << endl;
        cout << usage << ": " << help_text(0) <<endl;
	return false;
      }

    // Search the option list for the option to display.
    for (cmdopt_vector_iterator it = option_vec.begin();
	 it != option_vec.end(); ++it)
      {
	if ((*it)->accept(tokens[1]))
	  {
	    option_found = true;
	    (*it)->handler(s, tokens, input);
	    break;
	  }
      }
    if (!option_found)
      {
	cout << "Invalid option name" << endl << "Use \"help set\" for a list of options." <<endl;
      }
    return false;
  }
};

class quit_cmd : public cmdopt
{
public:
  quit_cmd()
  {
    name = usage = "quit";
    aliases = { "q" };
    _help_text = "Quit systemtap.";
  }
  bool handler(systemtap_session &s __attribute ((unused)),
	       vector<string> &tokens __attribute ((unused)),
	       string &input __attribute ((unused)))
  {
    return true;
  }
};

class add_cmd: public cmdopt
{
public:
  add_cmd()
  {
    name = usage = "add";
    aliases = { "a" };
    _help_text = "Add a global, probe, or function.";
  }
  bool handler(systemtap_session &s,
	       vector<string> &tokens __attribute ((unused)),
	       string &input)
  {
    vector<string> tmp_script_vec;

    // NB: At some point, we might store stap's parser output instead
    // of just a string.

    // Skip past the add command itself by removing the 1st token.
    tokens.erase(tokens.begin());
    if (tokens.size() == 0) {
      cout << name << ": " << _help_text << endl;
      return false;
    }

    // Find the "add" command text and skip past it.  Note that we're
    // using the "raw" (not tokenized) input, so that if someone was
    // trying to print "1    2", we preserve those embedded spaces.
    string::size_type add_pos = input.find("add");
    if (add_pos == string::npos)
	return false;			// shouldn't happen...
    string::size_type input_pos = input.find_first_not_of(" ", add_pos + 3);
    if (input_pos == string::npos)
	return false;			// shouldn't happen...

    // Add the rest of the line to the temporary script vector.
    string line = input.substr(input_pos);
    tmp_script_vec.push_back(line);

    // Try to parse the input. If it worked, we're done (since the
    // user entered an entire probe/function/global).
    int rc = forked_parse_pass(s, tmp_script_vec);
    if (rc != 0)
      {
	// If parsing above didn't work, we've either got a syntax
	// error or the user hasn't completed the function or probe
	// he's entering. So, prompt for more input.
	while (1)
	  {
	    char *response;
	    response = readline("stap>>> ");
	    if (response == NULL)		// EOF
	      {
		clog << endl;
		break;
	      }

	    // Try to parse the entire "add" command input.
	    tmp_script_vec.push_back(response);
	    int rc = forked_parse_pass(s, tmp_script_vec);
	    if (rc == 0)
	      break;
	  }
      }

    // Add the "add" command input to the script.
    script_vec.insert(script_vec.end(), tmp_script_vec.begin(),
		      tmp_script_vec.end());
    if (auto_analyze)
      (void)forked_semantic_pass(s, script_vec);
    return false;
  }
};

class delete_cmd: public cmdopt
{
public:
  delete_cmd()
  {
    name = "delete";
    aliases = { "d" };
    usage = "delete LINE_NUM_RANGE";
    _help_text = "Delete script line(s) by line numbers. To delete a single line, specify LINE_NUM_RANGE as \"10\". To delete a range of lines, specify LINE_NUM_RANGE as \"12-20\". To delete the entire script, give no argument.";
  }
  bool handler(systemtap_session &s __attribute ((unused)),
	       vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    // FIXME 2: Unlike gdb, our numbers get rearranged after a
    // delete. Example:
    //
    //   stap> list
    //   1: probe begin { exit() }
    //   2: probe end { printf("end\n") }
    //   stap> delete 1
    //   stap> list
    //   1: probe end { printf("end\n") }
    //
    // We could fix this if we stored the numbers along with the
    // script lines.

    if (tokens.size() == 1)
      {
	if (query("Delete entire script? ", no_default))
	  {
	    // Delete all probes/functions.
	    script_vec.clear();
	  }
	return false;
      }
    // At this point, we've either got a single number or a range. So,
    // the max number of extra tokens we could have would be 3 (NUM1 -
    // NUM2). 
    else if (tokens.size() > 4)
      {
	cout << endl << "Invalid command" << endl;
	interactive_usage();
	return false;
      }

    // If we've got more than 1 token, smash them together.
    string line_number_range = tokens[1];
    if (tokens.size() == 3)
	line_number_range += tokens[2];
    if (tokens.size() == 4)
	line_number_range += tokens[3];

    // Look for the 1st number in the (potential) range.
    char *end;
    long val;
    const char *lnr = line_number_range.c_str();

    errno = 0;
    val = strtol (lnr, &end, 10);
    if (errno != 0 || (*end != '\0' && *end != '-') || val < 0)
      {
	cout << _("Invalid starting script line range value.") << endl;
	return false;
      }

    // Does this script line exist?
    size_t line_start = val - 1;
    if (line_start > script_vec.size())
      {
	cout << _F("No line %ld present in script.", val) << endl;
	return false;
      }
      
    size_t line_end = line_start;
    if (*end == '-')
      {
	errno = 0;
	char *start = end + 1;
	val = strtol (start, &end, 10);
	if (errno != 0 || *end != '\0' || val < 0)
	  {
	    cout << _("Invalid ending script line range value.") << endl;
	    return false;
	  }
	line_end = val;

	if (line_end > script_vec.size())
	  {
	    cout << _F("No line %lu present in script.",
		       (unsigned long)line_end) << endl;
	    return false;
	  }
	else if (line_end < line_start)
	  {
	    cout << _("Invalid script line range value - starting line is greater than ending line.") << endl;
	    return false;
	  }
      }

    // Delete script line(s).
    if (line_start == line_end)
      script_vec.erase(script_vec.begin() + line_start);
    else
      script_vec.erase(script_vec.begin() + line_start,
		       script_vec.begin() + line_end);
    if (auto_analyze)
      (void)forked_semantic_pass(s, script_vec);
    return false;
  }
};

class load_cmd : public cmdopt
{
public:
  load_cmd()
  {
    name = "load";
    usage = "load FILE";
    _help_text = "Load a script from a file into the current session.";
  }
  bool handler(systemtap_session &s __attribute ((unused)),
	       vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    if (tokens.size() != 2)
      {
	cout << "FILE must be specified." << endl;
        cout << usage << ": " << help_text(0) <<endl;
	return false;
      }

    if (!script_vec.empty())
      {
        if (query("Script exists. Overwrite existing script? ", no_default))
          script_vec.clear();
        else
          return false;
      }

    // Originally, we called stap's parser here to read in the
    // script. However, doing so discards comments, preprocessor
    // directives, and rearranges the script. So, let's just read the
    // script as a series of strings.
    ifstream f(tokens[1].c_str(), ios::in);
    if (f.fail())
      {
	cout << endl
	     << _F("File '%s' couldn't be opened for reading.",
		   tokens[1].c_str()) << endl;
	return false;
      }
	
    string line;
    while (getline(f, line))
      {
	script_vec.push_back(line);
      }
    f.close();
    return false;
  }
};

class save_cmd : public cmdopt
{
public:
  save_cmd()
  {
    name = "save";
    usage = "save FILE";
    _help_text = "Save a script to a file from the current session.";
  }
  bool handler(systemtap_session &s __attribute ((unused)),
	       vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    if (tokens.size() != 2)
      {
	cout << "FILE must be specified." << endl;
        cout << usage << ": " << help_text(0) <<endl;
	return false;
      }

    ofstream f(tokens[1].c_str(), ios::out);
    if (f.fail())
      {
	cout << endl
	     << _F("File '%s' couldn't be opened for writing.",
		   tokens[1].c_str()) << endl;
	return false;
      }

    string script;
    if (!script_vec.empty())
      script = join(script_vec, "\n");
    f << script << endl;
    f.close();
    return false;
  }
};

class run_cmd : public cmdopt
{
public:
  run_cmd()
  {
    name = usage = "run";
    aliases = { "r" };
    _help_text = "Run the current script.";
  }
  bool handler(systemtap_session &s,
	       vector<string> &tokens __attribute ((unused)),
	       string &input __attribute ((unused)))
  {
    if (script_vec.empty())
      {
	clog << "No script specified." << endl;
	return false;
      }

    // FIXME: there isn't any real point to calling
    // systemtap_session::clone() here, since it doesn't (usually)
    // create a new session, but returns a cached session. So, we'll
    // just use the current session.
    s.cmdline_script = join(script_vec, "\n");
    s.have_script = true;
    int rc = forked_passes_0_4(s);
#if 0
    if (rc)
    {
	// Compilation failed.
	// Try again using a server if appropriate.
	if (s.try_server ())
	    rc = passes_0_4_again_with_server (s);
    }
#endif
    if (rc || s.perpass_verbose[0] >= 1)
	s.explain_auto_options ();

    // Run pass 5, if passes 0-4 worked.
    if (rc == 0 && s.last_pass >= 5 && !pending_interrupts)
      rc = pass_5 (s, *saved_targets);
    s.reset_tmp_dir();
    return false;
  }
};

class edit_cmd : public cmdopt
{
public:
  edit_cmd()
  {
    name = usage = "edit";
    aliases = { "e" };
    _help_text = "Edit the current script. Uses EDITOR environment variable contents as editor (or ex as default).";
  }
  bool handler(systemtap_session &s,
	       vector<string> &tokens __attribute ((unused)),
	       string &input __attribute ((unused)))
  {
    const char *editor;
    char temp_path[] = "/tmp/stapXXXXXX";
    int fd;

    // Get EDITOR value.
    if ((editor = getenv ("EDITOR")) == NULL)
      editor = "/bin/ex";

    // Get a temporary file.
    fd = mkstemp(temp_path);
    if (fd < 0)
      {
	cout << endl
	     << _F("Temporary file '%s' couldn't be opened: %s",
		   temp_path, strerror(errno)) << endl;
	return false;
      }

    // Write the script contents to the temporary file.
    if (!script_vec.empty())
      {
	string script = join(script_vec, "\n");
	if (write(fd, script.c_str(), script.length()) < 0)
	  {
	    cout << endl
		 << _F("Writing to temporary file '%s' failed: %s",
		       temp_path, strerror(errno)) << endl;
	    (void)close(fd);
	    (void)unlink(temp_path);
	    return false;
	  }
      }

    // Run the editor on the temporary file.
    vector<string> edit_cmd;
    edit_cmd.push_back(editor);
    edit_cmd.push_back(temp_path);
    if (stap_system(s.verbose, "edit", edit_cmd, false, false) != 0)
      {
	// Assume stap_system() reported an error.
	(void)close(fd);
	(void)unlink(temp_path);
	return false;
      }

    // Read the new script contents. First, rewind the fd.
    if (lseek(fd, 0, SEEK_SET) < 0)
      {
	cout << endl
	     << _F("Rewinding the temporary file fd failed: %s",
		   strerror(errno)) << endl;
	(void)close(fd);
	(void)unlink(temp_path);
	return false;
      }
    
    script_vec.clear();
    
    // Read the new file contents. We'd like to use C++ stream
    // operations here, but there isn't a easy way to convert a file
    // descriptor to a C++ stream.
    FILE *fp = fdopen(fd, "r");
    if (fp == NULL)
      {
	cout << endl
	     << _F("Converting the file descriptor to a stream failed: %s",
		   strerror(errno)) << endl;
	(void)close(fd);
	(void)unlink(temp_path);
	return false;
      }

    char line[2048];
    while (fgets(line, sizeof(line), fp) != NULL)
      {
	// Zap the ending '\n'.
	size_t len = strlen(line);
	if (line[len - 1] == '\n')
	  line[len - 1] = '\0';
	script_vec.push_back(line);
      }

    (void)fclose(fp);
    (void)unlink(temp_path);
    if (auto_analyze && !script_vec.empty())
      (void)forked_semantic_pass(s, script_vec);
    return false;
  }
};

#if HAVE_LIBSQLITE3
class sample_cmd: public cmdopt
{
public:
  sample_cmd()
  {
    name = "sample";
    usage = "sample STATEMENT";
    _help_text = "Search and load an example script using criteria specified in STATEMENT. STATEMENT can contain any number of case insensitive keywords seperated by OR, AND or NOT (case sensitive). Wildcards can be postfixed to any number of keywords. STATEMENT can contain anything supported by FTS full-text index queries. Example: sample socket AND tcp* OR disk NOT virtual";
  }
  bool handler (systemtap_session &s __attribute ((unused)),
                vector<string> &tokens __attribute ((unused)),
                string &input __attribute ((unused)))
  {
    if (tokens.size() == 1)
      {
        cout << usage << ": " << _help_text << endl;
        return false;
      }

    sqlite3* db;
    int rc;
    vector <metafile>  metalist;

    string dirstr = PKGDATADIR;
    dirstr.append("/examples/");

    rc = sqlite3_open((dirstr + "metadatabase.db").c_str(), &db);
    if (rc)
      {
	cout << "Error connecting to meta database. RC: " << rc << endl;
	return false;
      }

    string sqlstr = "SELECT name, title, description, path FROM metavirt WHERE metavirt MATCH ?;";
    sqlite3_stmt* sqlstmt;
    string tokenstring = tokens[1];
    for (unsigned it = 2; it < tokens.size(); it++)
      {
        tokenstring.append(" " + tokens[it]);
      }

    rc = sqlite3_prepare_v2(db, sqlstr.c_str(), sqlstr.size() + tokenstring.size(), &sqlstmt, NULL);
    if (rc)
      {
        cout << "Error preparing SQL statement. RC: " << rc << endl;
        return false;
      }

    rc = sqlite3_bind_text(sqlstmt, 1, tokenstring.c_str(), tokenstring.size(), 0);
    if (rc)
      {
        cout << "Error binding SQL statement. RC: " << rc << endl;
        return false;
      }

    metafile file;
    while (1)
      {
	rc = sqlite3_step(sqlstmt);
	if (rc == SQLITE_ROW)
	  {
	    file.name = (char*)sqlite3_column_text(sqlstmt,0);
            file.title = (char*)sqlite3_column_text(sqlstmt,1);
            file.description = (char*)sqlite3_column_text(sqlstmt,2);
            file.path = (char*)sqlite3_column_text(sqlstmt,3);
            metalist.push_back(file);
	  }
	else if (rc == SQLITE_DONE)
	  break;
	else
	  {
            cout << "Incomplete statement." << endl;
	    return false;
	  }
      }
    sqlite3_finalize(sqlstmt);

    if (metalist.size() == 0)
      {
        cout << "No sample scripts found using \"" << tokenstring << "\"." << endl;
	cout << "Note, AND, OR and NOT are case sensitive." << endl;
        return false;
      }

    char* response;
    metafile selection;
    while (selection.name == "")
      {
	cout << "Enter a script name for a description or use q to quit:" << endl;
        for (unsigned it=0; it < metalist.size();it++)
	  cout << "  " << metalist[it].name << ":  " << metalist[it].title <<  endl;

	response = readline(_F("%s","stap sample>> ").c_str());

        if (response == NULL) // EOF
          {
            cout << endl;
            return false;
          }
        if (*response == 'q')
          return false;

       for (unsigned i=0; i < metalist.size();i++)
	 {
	   if (metalist[i].name.find(response) != string::npos)
	     selection = metalist[i];
         }
         if (selection.name == "")
	   {
            cout << "Unknown selection." << endl;
	    continue;
           }
	 cout << endl << "Title: " << selection.title << endl << "Name: " << selection.name << endl;
	 cout << "Description: " << selection.description << endl;
         if (!query("Would you like to load this example? ", no_default))
	   {
	     selection.name = "";
	    if (!query("Would you like to select another related script? ", no_default))
	      return false;
           }
      }

    tokens.erase(tokens.begin() + 1, tokens.begin() + tokens.size());
    auto load = new load_cmd;
    tokens.push_back(dirstr + selection.path + "/" + selection.name);
    load -> handler(s, tokens, input);

    sqlite3_close(db);
    return false;
  }
};
#endif

//
// Supported options for the "set" and "show" commands.
// 

class keep_tmpdir_opt: public cmdopt
{
public:
  keep_tmpdir_opt()
  {
    name = "keep_tmpdir";
    _help_text = "Keep temporary directory.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
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
      _help_text = "Stop after pass NUM 1-5.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
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
    _help_text = "Add verbosity to all passes.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
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

class guru_mode_opt: public cmdopt
{
public:
  guru_mode_opt()
  {
    name = "guru_mode";
    _help_text = "Guru mode.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
    if (set)
      s.guru_mode = (tokens[2] != "0");
    else
      cout << name << ": " << s.guru_mode << endl;
    return false;
  }
};

class suppress_warnings_opt: public cmdopt
{
public:
  suppress_warnings_opt()
  {
    name = "suppress_warnings";
    _help_text = "Suppress warnings.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
    if (set)
      s.suppress_warnings = (tokens[2] != "0");
    else
      cout << name << ": " << s.suppress_warnings << endl;
    return false;
  }
};


class panic_warnings_opt: public cmdopt
{
public:
  panic_warnings_opt()
  {
    name = "panic_warnings";
    _help_text = "Turn warnings into errors.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
    if (set)
      s.panic_warnings = (tokens[2] != "0");
    else
      cout << name << ": " << s.panic_warnings << endl;
    return false;
  }
};

class timing_opt: public cmdopt
{
public:
  timing_opt()
  {
    name = "timing";
    _help_text = "Collect probe timing information.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
    if (set)
      s.timing = (tokens[2] != "0");
    else
      cout << name << ": " << s.timing << endl;
    return false;
  }
};

class unoptimized_opt: public cmdopt
{
public:
  unoptimized_opt()
  {
    name = "unoptimized";
    _help_text = "Unoptimized translation.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
    if (set)
      s.unoptimized = (tokens[2] != "0");
    else
      cout << name << ": " << s.unoptimized << endl;
    return false;
  }
};

class target_pid_opt: public cmdopt
{
public:
  target_pid_opt()
  {
    name = "target_pid";
    _help_text = "Sets target() to PID.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
    if (set)
      {
	char *end;
	unsigned long val;

	if (s.cmd != "")
	  {
	    cerr << _("You can't specify a target pid and a cmd together.")
		 << endl;
	    return false;
	  }

	errno = 0;
	val = strtoul (tokens[2].c_str(), &end, 10);
	if (errno != 0 || *end != '\0' || val < 1 || val > 5)
	  cout << _("Invalid target process ID number.") << endl;
	else
	  s.target_pid = val;
      }
    else
      cout << name << ": " << s.target_pid << endl;
    return false;
  }
};

class cmd_opt: public cmdopt
{
public:
  cmd_opt()
  {
    name = "cmd";
    _help_text = "Start the probes, run CMD, and exit when it finishes.";
  }
  bool handler(systemtap_session &s, vector<string> &tokens,
	       string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
    if (set)
      {
	if (s.target_pid != 0)
	  {
	    cerr << _("You can't specify a target pid and a cmd together.")
		 << endl;
	    return false;
	  }

	s.cmd.clear();
	for (unsigned i = 2; i < tokens.size(); ++i)
	  {
	    // Paste the tokens back together.
	    if (!s.cmd.empty())
	      s.cmd += " ";
	    s.cmd += tokens[i];
	  }
	// If the string is quoted, remove the outer quotes.
	if ((s.cmd[0] == '"' || s.cmd[0] == '\'')
	    && s.cmd[0] == s.cmd[s.cmd.size() - 1])
	  {
	    s.cmd.erase(0, 1);
	    s.cmd.erase(s.cmd.size() - 1, 1);
	  }
      }
    else
      cout << name << ": \"" << s.cmd << "\"" << endl;
    return false;
  }
};

class auto_analyze_opt: public cmdopt
{
public:
  auto_analyze_opt()
  {
    name = "auto_analyze";
    _help_text = "Automatically analyze the current script after changes.";
  }
  bool handler(systemtap_session &s __attribute ((unused)),
	       vector<string> &tokens, string &input __attribute ((unused)))
  {
    bool set = (tokens[0] == "set");
    if (set)
      auto_analyze = (tokens[2] != "0");
    else
      cout << name << ": " << auto_analyze << endl;
    return false;
  }
};

static void
interactive_usage ()
{
  cout << "List of commands:" << endl << endl;

  // Find biggest "usage" field.
  size_t width = 1;
  for (cmdopt_vector_const_iterator it = command_vec.begin();
       it != command_vec.end(); ++it)
    {
      if ((*it)->usage.size() > width)
	  width = (*it)->usage.size();
    }
  // Print usage field and help text for each command.
  for (cmdopt_vector_const_iterator it = command_vec.begin();
       it != command_vec.end(); ++it)
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

  // If this is a new word to complete, initialize now.  This includes
  // saving the length of TEXT for efficiency, and initializing the
  // index variable to 0.
  if (!state)
  {
    list_index = 0;
    len = strlen(text);
  }

  // Return the next name which partially matches from the command list.
  while (list_index < command_vec.size())
  {
    cmdopt *cmd = command_vec[list_index];
    list_index++;
    if (strncmp(cmd->name.c_str(), text, len) == 0)
      return strdup(cmd->name.c_str());
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
  while (list_index < option_vec.size())
  {
    cmdopt *opt = option_vec[list_index];
    list_index++;
    if (strncmp(opt->name.c_str(), text, len) == 0)
      return strdup(opt->name.c_str());
  }

  // If no names matched, then return NULL.
  return NULL;
}

#ifdef DEBUG
// An iterative process to traverse the parse tree, looking for
// partial matches.
static void
partial_matches(const char *text, match_item_map &map, vector<string> &matches)
{
    // Create an empty stack and push items to it that paritally match.
    std::stack< pair<string, match_item *> > stack;
    for (match_item_map_const_rev_iterator rit = map.rbegin();
	 rit != map.rend(); ++rit)
    {
	if (rit->second->partial_match(text))
	    stack.push(make_pair(rit->first, rit->second));
    }

    // Handle the stack.
    while (stack.empty() == false)
    {
	// Pop the top item from stack and handle it
	match_item *item = stack.top().second;
	string prefix = stack.top().first;
	stack.pop();

	if (item->terminal)
	    matches.push_back(prefix);
 
	// Push all children.
	for (match_item_map_const_rev_iterator rit = item->sub_matches.rbegin();
	     rit != item->sub_matches.rend(); ++rit)
	{
	    match_item *item = rit->second;
	    stack.push(make_pair(prefix + "." + rit->first, item));
	}
    }
}
#endif

static char *
probe_generator(const char *text, int state)
{
  static std::stack< pair<string, match_item *> > stack;

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
		match_map = &(map_it->second->sub_matches);
		break;
	    }
	}
	if (! found)
	    break;
    }

    // Clean out the stack from a previous run. This shouldn't happen,
    // but let's be sure.
    while (stack.empty() == false)
    {
	stack.pop();
    }

    // Now we're at the right match_item sub_matches map. Process it by
    // pushing items to the stack that paritally match.
    for (match_item_map_const_rev_iterator rit = match_map->rbegin();
	 rit != match_map->rend(); ++rit)
    {
	if (rit->second->partial_match(text))
	    stack.push(make_pair(rit->first, rit->second));
    }
  }

  // Handle the stack.
  while (stack.empty() == false)
  {
      // Pop the top item from stack and handle it
      match_item *item = stack.top().second;
      string prefix = stack.top().first;
      stack.pop();

      // Push all children.
      for (match_item_map_const_rev_iterator rit = item->sub_matches.rbegin();
	   rit != item->sub_matches.rend(); ++rit)
      {
	  match_item *item = rit->second;
	  stack.push(make_pair(prefix + "." + rit->first, item));
      }

      // If this item is terminal, return it.
      if (item->terminal)
	  return strdup(prefix.c_str());
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
interactive_completion(const char *text, int start,
		       int end __attribute ((unused)))
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
      for (cmdopt_vector_const_iterator it = option_command_vec.begin();
	   it != option_command_vec.end(); ++it)
      {
	if ((*it)->accept(tokens[0]))
	{
	  matches = rl_completion_matches(text, option_generator);
	  return matches;
	}
      }
    }

    // If we're in an "add" command...
    if (tokens.size() >= 2 && tokens[0] == "add")
    {
      // Perhaps we're in a probe declaration.
      if (tokens[1] == "probe")
      {
	// Save (possible) 2nd token for use by probe_generator().
	if (tokens.size() >= 3)
	  saved_token = tokens[2];
	else
	  saved_token = "";
	matches = rl_completion_matches(text, probe_generator);
      }
    }
    else if (tokens.size() == 2
	     && (tokens[0] == "load" || tokens[0] == "save"))
    {
      // Since the "load" and "save" commands *do* take a filename,
      // turn filename completion back on.
      rl_attempted_completion_over = 0;
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
	size_t space_pos;

	getline(probe_stream, line);
	if (line.empty())
	    continue;
	
	// Delete from a space to the end. Probe aliases look like:
	//
	//    syscall.write = kernel.function("sys_write)
	space_pos = line.find(" ");
	if (space_pos != string::npos)
	    line.erase(space_pos);

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

// Fork a new process for the dirty work
static int
forked_passes_0_4 (systemtap_session &s)
{
  stringstream ss;
  pair<bool,int> ret = stap_fork_read(s.perpass_verbose[0], ss);

  if (ret.first) // Child fork
    {
      int rc = 1;
      try
        {
          rc = passes_0_4 (s);
          stdio_filebuf<char> buf(ret.second, ios_base::out);
          ostream o(&buf);
          if (rc == 0 && s.last_pass > 4)
            {
              o << s.module_name << endl;
              o << s.uprobes_path << endl;
            }
          o.flush();
        }
      catch (...)
        {
          // NB: no cleanup from the fork!
        }
      // FIXME: what about cleanup(), but only for this session?
      // i.e. tapset coverage, report_suppression, but not subsessions.
      exit(rc ? EXIT_FAILURE : EXIT_SUCCESS);
    }

  // For passes <= 4, everything was written to cout.
  // For pass 5, we need the module and maybe uprobes for staprun.
  if (s.last_pass > 4 && ret.second == 0)
    ss >> s.module_name >> s.uprobes_path;

  return ret.second;
}

static int
forked_parse_pass (systemtap_session &s, vector<string> &script)
{
  // Save current session info.
  int saved_last_pass = s.last_pass;
  unsigned saved_verbose = s.verbose;
  unsigned saved_perpass_verbose[5];
  for (unsigned i=0; i<5; i++)
    saved_perpass_verbose[i] = s.perpass_verbose[i];
  bool saved_monitor = s.monitor;
    
  // Set up session to only run pass 1.
  s.last_pass = 1;
  s.verbose = 0;
  for (unsigned i=0; i<5; i++)
    s.perpass_verbose[i] = 0;
  s.monitor = false;

  // Add the script
  s.cmdline_script = join(script, "\n");
  s.have_script = true;

  // We don't want to see any of the parser output, which normally
  // goes to 'cout' and 'cerr'. So, redirect where 'cout' and 'cerr'
  // go, run the command, then restore 'cout' and 'cerr'.
  //
  // Why don't we want to see the parser output? The user may have
  // just entered "function foo() {" and intends to continue adding
  // more lines to the function.
  ofstream file("/dev/null");
  streambuf *former_cout = cout.rdbuf(file.rdbuf());
  streambuf *former_cerr = cerr.rdbuf(file.rdbuf());
  int rc = forked_passes_0_4 (s);
  cout.rdbuf(former_cout);
  cerr.rdbuf(former_cerr);
  file.close();

  // Restore the session values we changed.
  s.last_pass = saved_last_pass;
  s.verbose = saved_verbose;
  for (unsigned i=0; i<5; i++)
    s.perpass_verbose[i] = saved_perpass_verbose[i];
  s.monitor = saved_monitor;

  return rc;
}

static int
forked_semantic_pass (systemtap_session &s, vector<string> &script)
{
  clog << _("Script analysis:") << endl;

  // Save current session info.
  int saved_last_pass = s.last_pass;
  unsigned saved_verbose = s.verbose;
  unsigned saved_perpass_verbose[5];
  for (unsigned i=0; i<5; i++)
    saved_perpass_verbose[i] = s.perpass_verbose[i];
  bool saved_monitor = s.monitor;
    
  // Set up session to only run through pass 2.
  s.last_pass = 2;
  s.verbose = 0;
  for (unsigned i=0; i<5; i++)
    s.perpass_verbose[i] = 0;
  s.monitor = false;

  // Add the script
  s.cmdline_script = join(script, "\n");
  s.have_script = true;

  // We want the output, so we won't bother with redirection.
  int rc = forked_passes_0_4 (s);

  // Restore the session values we changed.
  s.last_pass = saved_last_pass;
  s.verbose = saved_verbose;
  for (unsigned i=0; i<5; i++)
    s.perpass_verbose[i] = saved_perpass_verbose[i];
  s.monitor = saved_monitor;

  return rc;
}

//
// Interactive mode, passes 0 through 5 and back again.
//

int
interactive_mode (systemtap_session &s, vector<remote*> targets)
{
  string delimiters = " \t";
  bool input_handled;

  // Save the target vector.
  saved_targets = &targets;

  // Tell readline's completer we want a crack at the input first.
  rl_attempted_completion_function = interactive_completion;

  // FIXME: this has been massively simplified from the default.
  rl_completer_word_break_characters = (char *)" \t\n.{";

  // Set up command list, along with a list of commands that take
  // options.
  command_vec.push_back(new add_cmd);
  command_vec.push_back(new delete_cmd);
  command_vec.push_back(new list_cmd);
  command_vec.push_back(new edit_cmd);
  command_vec.push_back(new load_cmd);
  command_vec.push_back(new save_cmd);
  command_vec.push_back(new run_cmd);
  #if HAVE_LIBSQLITE3
    command_vec.push_back(new sample_cmd);
  #endif
  command_vec.push_back(new set_cmd);
  option_command_vec.push_back(command_vec.back());
  command_vec.push_back(new show_cmd);
  option_command_vec.push_back(command_vec.back());
  command_vec.push_back(new help_cmd);
  command_vec.push_back(new quit_cmd);

  // Set up set/show option list.
  option_vec.push_back(new keep_tmpdir_opt);
  option_vec.push_back(new last_pass_opt);
  option_vec.push_back(new verbose_opt);
  option_vec.push_back(new guru_mode_opt);
  option_vec.push_back(new suppress_warnings_opt);
  option_vec.push_back(new panic_warnings_opt);
  option_vec.push_back(new timing_opt);
  option_vec.push_back(new unoptimized_opt);
  option_vec.push_back(new target_pid_opt);
  option_vec.push_back(new cmd_opt);
  option_vec.push_back(new auto_analyze_opt);

  // Feed provided script into interactive buffer
  if (!s.script_file.empty())
    {
      ifstream ifs(s.script_file);
      string line;
      while (getline(ifs, line))
        script_vec.push_back(line);
    }
  else if (!s.cmdline_script.empty())
    script_vec.push_back(s.cmdline_script);
  for (auto it = s.additional_scripts.begin();
       it != s.additional_scripts.end(); ++it)
    script_vec.push_back(*it);
  s.script_file.clear();
  s.cmdline_script.clear();
  s.additional_scripts.clear();

  // FIXME: It might be better to wait to get the list of probes and
  // aliases until they are needed.

  // Save the original state of the session object.
  unsigned saved_verbose = s.verbose;
  unsigned saved_perpass_verbose[5];
  for (unsigned i=0; i<5; i++)
      saved_perpass_verbose[i] = s.perpass_verbose[i];
  int saved_last_pass = s.last_pass;
  bool saved_monitor = s.monitor;

#if HAVE_NSS
  // If requested, query server status. This is independent
  // of other tasks.
  nss_client_query_server_status (s);

  // If requested, manage trust of servers. This is
  // independent of other tasks.
  nss_client_manage_server_trust (s);
#endif
  s.init_try_server ();

  // Get the list of "base" probe types, the same output you'd get
  // from doing 'stap --dump-probe-types'.
  s.verbose = 0;
  for (unsigned i=0; i<5; i++)
      s.perpass_verbose[i] = 0;
  s.dump_mode = systemtap_session::dump_probe_types;
  s.last_pass = 2;
  s.monitor = false;

  // We want to capture the probe output, which normally goes to
  // 'cout'. So, redirect where 'cout' goes, run the command, then
  // restore 'cout'.
  stringstream probes;
  streambuf *former_buff = cout.rdbuf(probes.rdbuf());
  passes_0_4(s);
  cout.rdbuf(former_buff);

  // Now that we have the list of "base" probe types, call
  // process_probe_list() to turn that into our parse tree.
  process_probe_list(probes, true);

  // FIXME: It might be nice instead of completing to:
  //    process(number).function(string)
  // instead we did:
  //    process(PID).function("NAME")
  // i.e. the '(number)' and '(string)' fields were more descriptive.

  // Now we'll need to get all the probe aliases ("stap
  // --dump-probe-aliases").
  s.dump_mode = systemtap_session::dump_probe_aliases;
  
  // We want to capture the alias output, which normally goes to
  // 'cout'. So, redirect where 'cout' goes, run the command, then
  // restore 'cout'.
  stringstream aliases;
  former_buff = cout.rdbuf(aliases.rdbuf());
  passes_0_4(s);
  cout.rdbuf(former_buff);

  // Process the list of probe aliases.
  process_probe_list(aliases, false);

  // FIXME: We could also complete systemtap function names.

  // Restore the original state of the session object.
  s.dump_mode = systemtap_session::dump_none;
  s.verbose = saved_verbose;
  for (unsigned i=0; i<5; i++)
      s.perpass_verbose[i] = saved_perpass_verbose[i];
  s.last_pass = saved_last_pass;
  s.monitor = saved_monitor;
  s.clear_script_data();

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
  cout << "Example commands: 'sample', 'add', 'run', 'help'" << endl;
  while (1)
    {
      // PR19847: clear any pending interrupts from previous commands
      pending_interrupts = 0;

      char *line_tmp = readline("stap> ");
      if (line_tmp == NULL)		// C-d
        {
	  clog << endl;
	  if (query(_("Quit? "), default_yes))
	    break;
	  continue;
	}
      else if (*line_tmp)
	add_history(line_tmp);
      else
	{
	  cout << "Example commands: 'sample', 'add', 'run', 'help'" << endl;
          continue;
        }

      string line = string(line_tmp);
      free(line_tmp);

      vector<string> tokens;
      tokenize(line, tokens, delimiters);

      input_handled = false;
      if (tokens.size())
        {
	  bool quit = false;
	  // Search list for command to execute.
	  for (cmdopt_vector_iterator it = command_vec.begin();
	       it != command_vec.end(); ++it)
	    {
	      if ((*it)->accept(tokens[0]))
	        {
		  input_handled = true;
		  quit = (*it)->handler(s, tokens, line);
		  break;
		}
	    }
	    
	  if (input_handled && quit)
	    break;
	}

      // If it isn't a command, complain.
      if (!input_handled)
	clog << "Undefined command: \"" << tokens[0]
	     << "\". Try \"help\" or \"?\"." << endl;
    }
  delete_match_map_items(&probe_map);
  return 0;
}
