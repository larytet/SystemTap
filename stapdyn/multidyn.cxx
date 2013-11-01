// stapdyn multi-modules test program
// Copyright (C) 2012-2013 Red Hat Inc.
// Copyright (C) 2013 Serhei Makarov
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.


#include <iostream>
#include <vector>
#include <memory>

#include <boost/shared_ptr.hpp>

extern "C" {
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>
}

#include "config.h"
#include "../git_version.h"
#include "../version.h"

#include "mutator.h"
#include "dynutil.h"


using namespace std;

// TODOXXX distinguish errors from different modules in output
// TODOXXX major snafus occur due to SELinux interference, use permissive for now
// TODOXXX stock Fedora Dyninst is deluded that it's on SysV, -x fails (????)
// TODOXXX limits on process observation really cramp our style

// Basically copies the main() of stapdyn.cxx:
void
launch_module(int argc, char * const argv[], mutator &session)
{
  pid_t pid = 0;
  const char* command = NULL;
  const char* module = NULL;

  // Check if error/warning msgs should be colored
  color_errors = isatty(STDERR_FILENO)
    && strcmp(getenv("TERM") ?: "notdumb", "dumb");

  // First, option parsing.
  int opt;
  while ((opt = getopt (argc, argv, "c:x:vwo:VC:")) != -1)
    {
      switch (opt)
        {
        case 'c':
          command = optarg;
          break;

        case 'x':
          cerr << "FOUND -x" << endl;
          pid = atoi(optarg);
          break;

        case 'v':
          ++stapdyn_log_level;
          break;

        case 'w':
          stapdyn_suppress_warnings = true;
          break;

	case 'o':
	  stapdyn_outfile_name = optarg;
	  break;

        case 'C':
          if (!strcmp(optarg, "never"))
            color_errors = false;
          else if (!strcmp(optarg, "auto"))
            color_errors = isatty(STDERR_FILENO)
              && strcmp(getenv("TERM") ?: "notdumb", "dumb");
          else if (!strcmp(optarg, "always"))
            color_errors = true;
          else {
            staperror() << "Invalid option '" << optarg << "' for -C." << endl;
            exit (1);
          }
          break;
        default:
          staperror() << "unknown option" << endl;
        }
    }

  // The first non-option is our stap module, required.
  if (optind < argc)
    module = argv[optind++];

  // Remaining non-options, if any, specify global variables.
  vector<string> modoptions;
  while (optind < argc)
    {
      modoptions.push_back(string(argv[optind++]));
    }

  if (!module || (command && pid))
    {
      staperror() << "no module, or both command and pid" << endl;
      exit (1);
    }

  // Make sure that environment variables and selinux are set ok.
  if (!check_dyninst_rt())
    exit(1);
  if (!check_dyninst_sebools(pid != 0))
    exit(1);

  boost::shared_ptr<script_module> script
    = session.create_module(module, modoptions);
  if (!script.get() || !script->load())
    {
      staperror() << "Failed to load script module!" << endl;
      exit(1);
    }

  if (command && !script->set_main_target(session.create_process(command)))
    exit(1);

  if (pid && !script->set_main_target(session.attach_process(pid)))
    exit(1);

  if (!script->start())
    exit(1);
}


int
main()
{
  cerr << "INPUT CMDLINES FOR EACH MODULE. FINISH WITH EOF (^D)" << endl;

  vector<string> cmdlines; string line;
  while (getline(cin,line)) cmdlines.push_back(line);

  auto_ptr<mutator> session(new mutator());
  if (!session.get())
    {
      staperror() << "Failed to initialize dyninst session!" << endl;
    }

  for (unsigned i = 0; i < cmdlines.size(); i++)
    {
      // Split the command into words.
      int child_argc; char* const* child_argv; wordexp_t words;
      int rc = wordexp (cmdlines[i].c_str(), &words, WRDE_NOCMD|WRDE_UNDEF);
      if (rc == 0)
        {
          child_argc = (int) words.we_wordc;
          child_argv = (/*cheater*/ char* const*) words.we_wordv;

          // TODOXXX save handle for each module
          launch_module(child_argc, child_argv, *session);
        }
      else
        {
          staperror() << "wordexp parsing error (" << rc << ") in "
                      << cmdlines[i] << endl;
          return 1;
        }
    }

  if (!session->run_to_completion())
    return 1;

  return 0; // TODOXXX get exit values
}
