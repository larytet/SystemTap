// -*- C++ -*-
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"

#ifdef HAVE_HTTP_SUPPORT
#include "client-http.h"
#include "util.h"

#include <iostream>

extern "C" {
#include <string.h>
}
using namespace std;

int
http_client_backend::initialize ()
{
  request_parameters.clear();
  request_files.clear();
  return 0;
}

int
http_client_backend::package_request ()
{
  return 0;
}

int
http_client_backend::find_and_connect_to_server ()
{
  // FIXME: We need real code here.
#if 0
  // Send it to the server.
  http_session hs;
  http_response r = http_requests.post(?, request_parameters, request_files);
  assert_no_interrupts();
  //if (rc != 0) goto done;
#endif
  clog << "http client code not finished" << endl;
  return 1;
}

int
http_client_backend::unpack_response ()
{
  return 0;
}

int
http_client_backend::process_response ()
{
  return 0;
}

int
http_client_backend::add_protocol_version (const std::string &version)
{
  // Add the protocol version (so the server can ensure we're
  // compatible).
  request_parameters.push_back(make_tuple("version", version));
  return 0;
}

int
http_client_backend::add_sysinfo ()
{
  // Add the sysinfo.
  request_parameters.push_back(make_tuple("kver", s.kernel_release));
  request_parameters.push_back(make_tuple("arch", s.architecture));
  return 0;
}

int
http_client_backend::include_file_or_directory (const std::string &,
						const std::string &)
{
  // FIXME: this is going to be interesting. We can't add a whole
  // directory at one shot, we'll have to traverse the directory and
  // add each file, preserving the directory structure somehow.
  return 0;
}

int
http_client_backend::add_tmpdir_file (const std::string &file)
{
  request_files.push_back(make_tuple("files", file));
  return 0;
}

int
http_client_backend::add_cmd_arg (const std::string &arg)
{
  request_parameters.push_back(make_tuple("cmd_args", arg));
  return 0;
}

void
http_client_backend::add_localization_variable(const std::string &,
					       const std::string &)
{
  // FIXME: We'll probably just add to the request_parameters here.
  return;
}

void
http_client_backend::add_mok_fingerprint(const std::string &)
{
  // FIXME: We'll probably just add to the request_parameters here.
  return;
}

#endif /* HAVE_HTTP_SUPPORT */
