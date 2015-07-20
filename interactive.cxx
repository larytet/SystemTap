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
#include <readline/readline.h>
#include <readline/history.h>
}

// FIXME: these declarations don't really belong here.
extern int
passes_0_4 (systemtap_session &s);
extern int
pass_5 (systemtap_session &s, vector<remote*> targets);

static void
interactive_cmds ()
{
  cout << endl
       << "List of commands:\n\n"
       << "!help             -- Print this command list\n"
       << "!set OPTION VALUE -- Set option value. Supported options are\n"
       << "                     'keep_tmpdir', 'last_pass' and 'verbose'.\n"
       << "!show OPTION      -- Show option value.\n"
       << "!quit             -- Exit systemtap\n";
}

// FIXME: this isn't very elegant, and will quickly become longer and
// longer with more supported options. However, for now...
static void
handle_setshow_cmd (systemtap_session &s, vector<string> &tokens)
{
    bool set = (tokens[0] == "!set");

    if ((set && tokens.size() != 3) || (!set && tokens.size() != 2))
    {
	cout << endl
	     << "Invalid command\n";
	interactive_cmds();
	return;
    }
    
    if (tokens[1] == "keep_tmpdir")
      {
	if (set)
	  s.keep_tmpdir = (tokens[2] != "0");
	else
	  cout << "keep_tmpdir: " << s.keep_tmpdir << endl;
      }
    else if (tokens[1] == "last_pass")
      {
	if (set)
	  {
	    char *end;
	    long val;

	    errno = 0;
	    val = strtol (tokens[2].c_str(), &end, 10);
	    if (errno != 0 || *end != '\0' || val < 1 || val > 5)
	      cout << endl
		   << "Invalid option value (should be 1-5)\n";
	    else
	      s.last_pass = val;
	  }
	else
	  cout << "last_pass: " << s.last_pass << endl;
      }
    else if (tokens[1] == "verbose")
      {
	if (set)
	  {
	    char *end;
	    long val;

	    errno = 0;
	    val = strtol (tokens[2].c_str(), &end, 10);
	    if (errno != 0 || *end != '\0' || val < 0)
	      cout << endl
		   << "Invalid option value (should be greater than 0)\n";
	    else
	      {
		s.verbose = val;
		for (unsigned i=0; i<5; i++)
		  s.perpass_verbose[i] = val;
	      }
	  }
	else
	  cout << "verbose: " << s.verbose << endl;
      }
    else
      {
	cout << endl
	     << "Invalid option name\n";
	interactive_cmds();
      }
    return;
}

// Interactive mode, passes 0 through 5 and back again.
int
interactive_mode (systemtap_session &s, vector<remote*> targets)
{
  int rc;
  string delimiters = " \t";
  bool input_handled;

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
	  if (tokens[0] == "!quit")
	    {
	      input_handled = true;
	      break;
	    }
	  else if (tokens[0] == "!help")
	    {
	      input_handled = true;
	      interactive_cmds();
	    }
	  else if (tokens[0] == "!set" || tokens[0] == "!show")
	    {
	      input_handled = true;
	      handle_setshow_cmd(s, tokens);
	    }
	}

      // If it isn't a command, we assume it is a script to run.
      //
      // FIXME: Later this could be a line from a script that we have
      // to keep track of.
      if (!input_handled)
        {
	  // Try creating a new systemtap session object so that we
	  // don't get leftovers from the last script we compiled.
	  systemtap_session* ss = s.clone(s.architecture, s.kernel_release);
	  clog << "orig tmpdir: " << s.tmpdir
	       << ", new tmpdir: " << ss->tmpdir << endl;

	  ss->clear_script_data();
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
