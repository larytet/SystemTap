// -*- C++ -*-
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef CLIENT_HTTP_H
#define CLIENT_HTTP_H

#ifdef HAVE_HTTP_SUPPORT

#include "session.h"
#include "csclient.h"
#include <json/json.h>

class http_client_backend : public client_backend
{
public:
  http_client_backend (systemtap_session &s) : client_backend(s) {}

  int initialize ();
  int package_request ();
  int find_and_connect_to_server ();
  int unpack_response ();
  int process_response ();

  int add_protocol_version (const std::string &version);
  int add_sysinfo ();
  int include_file_or_directory (const std::string &subdir,
				 const std::string &path);
  int add_tmpdir_file (const std::string &file);
  int add_cmd_arg (const std::string &arg);

  void add_localization_variable(const std::string &var,
				 const std::string &value);
  int finalize_localization_variables() { return 0; };

  void add_mok_fingerprint(const std::string &fingerprint);
  int finalize_mok_fingerprints() { return 0; };

private:
  Json::Value root;
  std::string host;
  std::string query;
  std::map<std::string, std::string> header_values;
  enum download_type {json_type, file_type};
  void *curl;
  int retry;
  std::string *location;
  bool download (const std::string & url, enum download_type type);
  void post (const std::string & url, const std::string & data);
  void get_header_field (const std::string & data, const std::string & field);
  static size_t get_data (void *ptr, size_t size, size_t nitems,
                          http_client_backend * data);
  static size_t get_file (void *ptr, size_t size, size_t nitems,
                          FILE * stream);
  static size_t get_header (void *ptr, size_t size, size_t nitems,
                            http_client_backend * data);

  // FIXME: The 'request_parameters' data item isn't right. This means
  // we can only add string parameters, not numeric parameters. We
  // could have 'request_string_parameters' and
  // 'request_numeric_parameters' - but then we get the ordering
  // wrong.
  std::vector<std::tuple<std::string, std::string>> request_parameters;
  std::vector<std::tuple<std::string, std::string>> request_files;
};

#endif	// HAVE_HTTP_SUPPORT

#endif	// CLIENT_HTTP_H
