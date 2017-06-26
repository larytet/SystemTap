// -*- C++ -*-
// Copyright (C) 2010-2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
#ifndef CSCLIENT_H
#define CSCLIENT_H

#include "cscommon.h"
#include "string"

class client_backend
{
public:
  client_backend (systemtap_session &s) : s(s) {}
  ~client_backend () {}

  void set_tmpdir (std::string &tmpdir) { client_tmpdir = tmpdir; }
  virtual int initialize () = 0;
  virtual int package_request () = 0;
  virtual int find_and_connect_to_server () = 0;
  virtual int unpack_response () = 0;
  virtual int process_response () = 0;

  virtual int add_protocol_version (const std::string &version) = 0;
  virtual int add_sysinfo () = 0;
  virtual int include_file_or_directory (const std::string &subdir,
					 const std::string &path) = 0;
  virtual int add_tmpdir_file (const std::string &path) = 0;
  virtual int add_cmd_arg (const std::string &arg) = 0;

  virtual void add_localization_variable(const std::string &var,
					 const std::string &value) = 0;
  virtual int finalize_localization_variables() = 0;

  virtual void add_mok_fingerprint(const std::string &fingerprint) = 0;
  virtual int finalize_mok_fingerprints() = 0;

protected:
  systemtap_session &s;
  std::string client_tmpdir;
};

class compile_server_client
{
public:
  compile_server_client (systemtap_session &s) : s(s), backend(NULL) {}
  int passes_0_4 ();

private:
  // Client/server session methods.
  int initialize ();
  int create_request ();

  // Client/server utility methods.
  int add_cmd_args ();
  int add_localization_variables();

  void show_server_compatibility () const;

  systemtap_session &s;
  client_backend *backend;

  std::string client_tmpdir;
};

#endif // CSCLIENT_H
