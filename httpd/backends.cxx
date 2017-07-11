// systemtap compile-server server backends.
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "backends.h"
#include <iostream>
#include "../util.h"

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <spawn.h>
#include <string.h>
}

using namespace std;


class local_backend : public backend_base
{
public:
    bool can_generate_module(const struct client_request_data *crd);
    int generate_module(const struct client_request_data *crd,
			const vector<string> &argv,
			const string &tmp_dir,
			const string &stdout_path,
			const string &stderr_path);
};

#if 0
class docker_backend : public backend_base
{
public:
    docker_backend(string &tmp_dir) : backend_base(tmp_dir) { }

    bool can_generate_module(const string &kver, const string &arch);
    int generate_module(vector<string> &argv);
};
#endif

bool
local_backend::can_generate_module(const struct client_request_data *)
{
    // FIXME: We'll need to check to see if this system has the right
    // kernel development environment and the arches match.
    return true;
}

int
local_backend:: generate_module(const struct client_request_data *,
				const vector<string> &argv,
				const string &,
				const string &stdout_path,
				const string &stderr_path)
{
    // Handle capturing stdout and stderr (along with using /dev/null
    // for stdin).
    posix_spawn_file_actions_t actions;
    int rc = posix_spawn_file_actions_init(&actions);
    if (rc == 0) {
	rc = posix_spawn_file_actions_addopen(&actions, 0, "/dev/null",
					      O_RDONLY, S_IRWXU);
    }
    if (rc == 0) {
	rc = posix_spawn_file_actions_addopen(&actions, 1,
					      stdout_path.c_str(),
					      O_WRONLY|O_CREAT|O_EXCL,
					      S_IRWXU);
    }
    if (rc == 0) {
	rc = posix_spawn_file_actions_addopen(&actions, 2,
					      stderr_path.c_str(),
					      O_WRONLY|O_CREAT|O_EXCL,
					      S_IRWXU);
    }
    if (rc != 0) {
	clog << "posix_spawn_file_actions failed: " << strerror(errno)
	     << endl;
	return rc;
    }

    // Kick off stap.
    pid_t pid;
    pid = stap_spawn(2, argv, &actions);
    clog << "spawn returned " << pid << endl;

    // If stap_spawn() failed, no need to wait.
    if (pid == -1) {
	clog << "Error in spawn: " << strerror(errno) << endl;
	return errno;
    }

    // Wait on the spawned process to finish.
    rc = stap_waitpid(0, pid);
    if (rc < 0) {			// stap_waitpid() failed
	return rc;
    }

    clog << "Spawned process returned " << rc << endl;
    (void)posix_spawn_file_actions_destroy(&actions);

    return rc;
}

void
get_backends(vector<backend_base *> &backends)
{
    static vector<backend_base *>saved_backends;

    if (saved_backends.empty()) {
	saved_backends.push_back(new local_backend());
    }
    backends.clear();
    backends = saved_backends;
}
