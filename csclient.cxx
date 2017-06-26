/*
 Compile server client functions
 Copyright (C) 2010-2017 Red Hat Inc.

 This file is part of systemtap, and is free software.  You can
 redistribute it and/or modify it under the terms of the GNU General
 Public License (GPL); either version 2, or (at your option) any
 later version.
*/

// Completely disable the client if NSS is not available.
#include "config.h"
#include "session.h"
#include "cscommon.h"
#include "csclient.h"
#include "client-nss.h"
#include "client-http.h"
#include "util.h"
#include "stap-probe.h"

#include <iostream>

extern "C" {
#include <sys/times.h>
#include <sys/time.h>
}

using namespace std;

int
compile_server_client::passes_0_4 ()
{
  // Use the correct backend.
#ifdef HAVE_HTTP_SUPPORT
  if (! s.http_servers.empty())
      backend = new http_client_backend (s);
#endif
#if HAVE_NSS
  if (backend == NULL)
      backend = new nss_client_backend (s);
#endif
  if (backend == NULL)
    {
      clog << _("Using a compile server backend failed.") << endl;
      return 1;
    }

  PROBE1(stap, client__start, &s);

  // arguments parsed; get down to business
  if (s.verbose || ! s.auto_server_msgs.empty ())
    clog << _("Using a compile server.") << endl;

  struct tms tms_before;
  times (& tms_before);
  struct timeval tv_before;
  gettimeofday (&tv_before, NULL);

  // Create the request package.
  int rc = initialize ();
  assert_no_interrupts();
  if (rc != 0) goto done;
  rc = create_request ();
  assert_no_interrupts();
  if (rc != 0) goto done;
  rc = backend->package_request ();
  assert_no_interrupts();
  if (rc != 0) goto done;

  // Submit it to the server.
  rc = backend->find_and_connect_to_server ();
  assert_no_interrupts();
  if (rc != 0) goto done;

  // Unpack and process the response.
  rc = backend->unpack_response ();
  assert_no_interrupts();
  if (rc != 0) goto done;
  rc = backend->process_response ();

 done:
  struct tms tms_after;
  times (& tms_after);
  unsigned _sc_clk_tck = sysconf (_SC_CLK_TCK);
  struct timeval tv_after;
  gettimeofday (&tv_after, NULL);

#define TIMESPRINT "in " << \
           (tms_after.tms_cutime + tms_after.tms_utime \
            - tms_before.tms_cutime - tms_before.tms_utime) * 1000 / (_sc_clk_tck) << "usr/" \
        << (tms_after.tms_cstime + tms_after.tms_stime \
            - tms_before.tms_cstime - tms_before.tms_stime) * 1000 / (_sc_clk_tck) << "sys/" \
        << ((tv_after.tv_sec - tv_before.tv_sec) * 1000 + \
            ((long)tv_after.tv_usec - (long)tv_before.tv_usec) / 1000) << "real ms."

  if (rc == 0)
    {
      // Save the module, if necessary.
      if (s.last_pass == 4)
	s.save_module = true;

      // Copy module to the current directory.
      if (! pending_interrupts)
	{
	  if (s.save_module)
	    {
	      string module_src_path = s.tmpdir + "/" + s.module_filename();
	      string module_dest_path = s.module_filename();
	      copy_file (module_src_path, module_dest_path, s.verbose >= 3);
	      // Also copy the module signature, it it exists.
	      module_src_path += ".sgn";
	      if (file_exists (module_src_path))
		{
		  module_dest_path += ".sgn";
		  copy_file(module_src_path, module_dest_path, s.verbose >= 3);
		}
	    }
	  // Print the name of the module
	  if (s.last_pass == 4)
	    {
	      cout << s.module_filename() << endl;
	    }
	}
    }

  // syntax errors, if any, are already printed
  if (s.verbose)
    {
      string ws = s.winning_server;
      if (ws == "") ws = "?";
      clog << _("Passes: via server ") << ws << " "
           << getmemusage()
           << TIMESPRINT
           << endl;
    }
  if (rc && !s.dump_mode)
    {
      clog << _("Passes: via server failed.  Try again with another '-v' option.") << endl;
    }

  PROBE1(stap, client__end, &s);

  return rc;
}

// Initialize a client/server session.
int
compile_server_client::initialize ()
{
  int rc = 0;

  // Create a temporary directory to package things in.
  client_tmpdir = s.tmpdir + "/client";
  rc = create_dir (client_tmpdir.c_str ());
  if (rc != 0)
    {
      const char* e = strerror (errno);
      clog << _("ERROR: cannot create temporary directory (\"")
	   << client_tmpdir << "\"): " << e
	   << endl;
      return rc;
    }
  backend->set_tmpdir(client_tmpdir);

  return backend->initialize();
}

// Create the request package.
int
compile_server_client::create_request ()
{
  // Add the current protocol version.
  int rc = backend->add_protocol_version (CURRENT_CS_PROTOCOL_VERSION);
  if (rc != 0)
    return rc;

  // Add the script file or script option
  if (s.script_file != "")
    {
      if (s.script_file == "-")
	{
	  // Copy the script from stdin
	  string packaged_script_dir = client_tmpdir + "/script";
	  rc = create_dir (packaged_script_dir.c_str ());
	  if (rc != 0)
	    {
	      const char* e = strerror (errno);
	      clog << _("ERROR: cannot create temporary directory ")
		   << packaged_script_dir << ": " << e
		   << endl;
	      return rc;
	    }
	  rc = ! copy_file("/dev/stdin", packaged_script_dir + "/-");
	  if (rc != 0)
	    return rc;

	  // Let the backend know the file is there.
	  rc = backend->add_tmpdir_file ("script/-");
	  if (rc != 0)
	    return rc;

	  // Name the script in the stap cmd arguments.
	  rc = backend->add_cmd_arg ("script/-");
	  if (rc != 0)
	    return rc;
	}
      else
        {
	  // Add the script.
	  rc = backend->include_file_or_directory ("script", s.script_file);
	  if (rc != 0)
	    return rc;
	}
    }

  // Add -I paths. Skip the default directory.
  if (s.include_arg_start != -1)
    {
      unsigned limit = s.include_path.size ();
      for (unsigned i = s.include_arg_start; i < limit; ++i)
	{
	  rc = backend->include_file_or_directory ("tapset",
						   s.include_path[i]);
	  if (rc != 0)
	    return rc;
	  rc = backend->add_cmd_arg ("-I");
	  if (rc != 0)
	    return rc;
	  rc = backend->add_cmd_arg ("tapset");
	  if (rc != 0)
	    return rc;
	}
    }

  // Add other options.
  rc = add_cmd_args ();
  if (rc != 0)
    return rc;

  // Add the sysinfo.
  rc = backend->add_sysinfo ();
  if (rc != 0)
    return rc;

  // Add localization data
  rc = add_localization_variables();

  // Add the machine owner key (MOK) fingerprints, if needed.
  if (! s.mok_fingerprints.empty())
    {
      ostringstream fingerprints;
      vector<string>::const_iterator it;
      for (it = s.mok_fingerprints.begin(); it != s.mok_fingerprints.end();
	   it++)
	backend->add_mok_fingerprint(*it);
      rc = backend->finalize_mok_fingerprints();
      if (rc != 0)
	return rc;
    }

  return rc;
}

// Add the arguments specified on the command line to the server request
// package, as appropriate.
int
compile_server_client::add_cmd_args ()
{
  // stap arguments to be passed to the server.
  int rc = 0;
  unsigned limit = s.server_args.size();
  for (unsigned i = 0; i < limit; ++i)
    {
      rc = backend->add_cmd_arg (s.server_args[i]);
      if (rc != 0)
	return rc;
    }

  // Script arguments.
  limit = s.args.size();
  if (limit > 0) {
    rc = backend->add_cmd_arg ("--");
    if (rc != 0)
      return rc;
    for (unsigned i = 0; i < limit; ++i)
      {
	rc = backend->add_cmd_arg (s.args[i]);
	if (rc != 0)
	  return rc;
      }
  }
  return rc;
}  

// Add the localization variables to the server request
// package.
int
compile_server_client::add_localization_variables()
{
  const set<string> &locVars = localization_variables();

  /* Note: We don't have to check for the contents of the environment
   * variables here, since they will be checked extensively on the
   * server.
   */
  for (auto it = locVars.begin(); it != locVars.end(); it++)
    {
      char* var = getenv((*it).c_str());
      if (var)
	backend->add_localization_variable(*it, var);
    }
  return backend->finalize_localization_variables();
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
