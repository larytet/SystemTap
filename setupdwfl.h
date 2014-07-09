// Setup routines for creating fully populated DWFLs. Used in pass 2 and 3.
// Copyright (C) 2009-2014 Red Hat, Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
//
#ifndef SETUP_DWFLPP_H
#define SETUP_DWFLPP_H

#ifndef NT_GNU_BUILD_ID
#define NT_GNU_BUILD_ID 3
#endif

#include "config.h"
#include "session.h"

#include <set>
#include <string>
#include <vector>

extern "C" {
#include <elfutils/libdwfl.h>
}

std::string modname_from_path(const std::string &path);

Dwfl *setup_dwfl_kernel(const std::string &name,
			  unsigned *found,
			  systemtap_session &s);
Dwfl *setup_dwfl_kernel(const std::set<std::string> &names,
			  unsigned *found,
			  systemtap_session &s);

Dwfl *setup_dwfl_user(const std::string &name);
Dwfl *setup_dwfl_user(std::vector<std::string>::const_iterator &begin,
		        const std::vector<std::string>::const_iterator &end,
		        bool all_needed, systemtap_session &s);

// user-space files must be full paths and not end in .ko
bool is_user_module(const std::string &m);

int internal_find_debuginfo (Dwfl_Module *mod,
			      void **userdata __attribute__ ((unused)),
			      const char *modname __attribute__ ((unused)),
			      GElf_Addr base __attribute__ ((unused)),
			      const char *file_name,
			      const char *debuglink_file,
			      GElf_Word debuglink_crc,
			      char **debuginfo_file_name);
int execute_abrt_action_install_debuginfo_to_abrt_cache (std::string hex);
std::string get_kernel_build_id (systemtap_session &s);
int download_kernel_debuginfo (systemtap_session &s, std::string hex);
void debuginfo_path_insert_sysroot(std::string sysroot);

#endif
