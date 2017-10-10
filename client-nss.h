// -*- C++ -*-
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef CLIENT_NSS_H
#define CLIENT_NSS_H

#if HAVE_NSS

#include "session.h"
#include "csclient.h"
#include "cscommon.h"
#include <string>
#include <vector>

// Utility functions
void nss_client_query_server_status (systemtap_session &s);
void nss_client_manage_server_trust (systemtap_session &s);

struct compile_server_info;

class nss_client_backend : public client_backend
{
public:
  nss_client_backend (systemtap_session &s);

  int initialize ();
  int add_protocol_version (const std::string &version);
  int add_sysinfo ();
  int include_file_or_directory (const std::string &subdir,
				 const std::string &path);
  int add_tmpdir_file (const std::string &) { return 0; };
  int add_cmd_arg (const std::string &arg);

  void add_localization_variable(const std::string &var,
				 const std::string &value);
  int finalize_localization_variables();

  void add_mok_fingerprint(const std::string &fingerprint);
  int finalize_mok_fingerprints();

  int package_request ();
  int find_and_connect_to_server ();
  int unpack_response ();

private:
  unsigned argc;
  std::string client_zipfile;
  std::string server_zipfile;
  std::string locale_vars;
  std::ostringstream mok_fingerprints;

  std::vector<std::string> private_ssl_dbs;
  std::vector<std::string> public_ssl_dbs;

  int compile_using_server (std::vector<compile_server_info> &servers);
  void show_server_compatibility () const;
};

#endif	// HAVE_NSS

#endif	// CLIENT_NSS_H
