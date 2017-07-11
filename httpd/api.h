// systemtap compile-server web api header
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef __API_H__
#define __API_H__

#include "server.h"
#include <string>
#include <vector>

struct client_request_data
{
    std::string kver;
    std::string arch;
    std::string base_dir;
    std::vector<std::string> cmd_args;
    std::vector<std::string> files;
};

//extern bool
//api_handler(const char *url, const map<string, string> &url_args,
//	    const char *method, ostringstream &output);

void api_add_request_handlers(server &httpd);
void api_cleanup();

#endif	/* __API_H__ */
