// -*- C++ -*-
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"

#ifdef HAVE_HTTP_SUPPORT
#include "http_client.h"
#include "stap-probe.h"
#include "util.h"

#include <iostream>

extern "C" {
#include <string.h>
}
using namespace std;

int
http_client::passes_0_4 ()
{
  PROBE1(stap, http_client__start, &s);
  int rc = 0;

  if (s.verbose)
    clog << _("Using an http server.") << endl;

  // Create the request.
  rc = initialize ();
  assert_no_interrupts();
  if (rc != 0) goto done;
  rc = create_request ();
  assert_no_interrupts();
  if (rc != 0) goto done;

  // FIXME: more stuff here...

done:
  PROBE1(stap, http_client__end, &s);

  return rc;
}

// Initialize a client/server session.
int
http_client::initialize ()
{
  int rc = 0;

#if 0
  // Initialize session state
  argc = 0;

  // Private location for server certificates.
  private_ssl_dbs.push_back (private_ssl_cert_db_path ());

  // Additional public location.
  public_ssl_dbs.push_back (global_ssl_cert_db_path ());
#endif

  // Create a temporary directory to package things in.
  client_tmpdir = s.tmpdir + "/client";
  rc = create_dir (client_tmpdir.c_str ());
  if (rc != 0)
    {
      const char* e = strerror (errno);
      clog << _("ERROR: cannot create temporary directory (\"")
	   << client_tmpdir << "\"): " << e
	   << endl;
    }

  return rc;
}

// Create the request.
int
http_client::create_request ()
{
  int rc = 0;

  // Add the current systemtap version.
  request_parameters.push_back(make_tuple("version", VERSION));

  // Add the script file.
  if (s.script_file != "")
    {
      if (s.script_file == "-")
	{
	  // Copy the script from stdin
	  string script_path = client_tmpdir + "/-";
	  rc = ! copy_file("/dev/stdin", script_path);
	  if (rc != 0)
	    return rc;

	  // Add the script to the list of files to transfer.
	  request_files.push_back(make_tuple("-", script_path));
	}
      else
	{
	  // Add the script to the list of files to transfer.
          // FIXME: This should probably be the basename of the
          // script, not "script".
	  request_files.push_back(make_tuple("script", s.script_file));
	}
    }

  // FIXME: this is going to be interesting. We can't add a whole
  // directory at one shot, we'll have to traverse the directory and
  // add each file, preserving the directory structure somehow.
#if 0
  // Add -I paths. Skip the default directory.
  if (s.include_arg_start != -1)
    {
      unsigned limit = s.include_path.size ();
      for (unsigned i = s.include_arg_start; i < limit; ++i)
	{
	  rc = add_package_arg ("-I");
	  if (rc != 0)
	    return rc;
	  rc = include_file_or_directory ("tapset", s.include_path[i]);
	  if (rc != 0)
	    return rc;
	}
    }
#endif

  // Add stap arguments to be passed to the server.
  for (auto it = s.server_args.begin(); it != s.server_args.end(); it++)
    {
      request_parameters.push_back(make_tuple("cmd_args", *it));
    }

  // Add script arguments.
  if (! s.args.empty())
    {
      request_parameters.push_back(make_tuple("cmd_args", "--"));
      for (auto it = s.args.begin(); it != s.args.end(); it++)
        {
	  request_parameters.push_back(make_tuple("cmd_args", *it));
	}
    }

  // Add the sysinfo.
  request_parameters.push_back(make_tuple("kver", s.kernel_release));
  request_parameters.push_back(make_tuple("arch", s.architecture));
			       
#if 0
  // Add localization data
  rc = add_localization_variables();

  // Add the machine owner key (MOK) fingerprints file, if needed.
  if (! s.mok_fingerprints.empty())
    {
      ostringstream fingerprints;
      vector<string>::const_iterator it;
      for (it = s.mok_fingerprints.begin(); it != s.mok_fingerprints.end();
	   it++)
	fingerprints << *it << endl;

      rc = write_to_file(client_tmpdir + "/mok_fingerprints",
			 fingerprints.str());
      if (rc != 0)
	  return rc;
    }
#endif

  return rc;
}


#endif /* HAVE_HTTP_SUPPORT */
