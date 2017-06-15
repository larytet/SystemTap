// -*- C++ -*-
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "config.h"

#ifdef HAVE_HTTP_SUPPORT

#include "session.h"
#include <vector>
#include <tuple>
#include <string>

// FIXME: should this class and compile_server_client (in csclient.h)
// derive from the same base class?

class http_client
{
public:
  http_client (systemtap_session &s) : s(s) {}
  int passes_0_4 ();

private:
  // Client/server session methods.
  int initialize ();
  int create_request ();

  systemtap_session &s;
  std::string client_tmpdir;

  // FIXME: The 'request_parameters' data item isn't right. This means
  // we can only add string parameters, not numeric parameters. We
  // could have 'request_string_parameters' and
  // 'request_numeric_parameters'.
  std::vector<std::tuple<std::string, std::string>> request_parameters;
  std::vector<std::tuple<std::string, std::string>> request_files;
};

#endif	/* HAVE_HTTP_SUPPORT */

#endif	/* HTTP_CLIENT_H */
