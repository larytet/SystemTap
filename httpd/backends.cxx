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
#include <glob.h>
}

using namespace std;


class local_backend : public backend_base
{
public:
    local_backend();

    bool can_generate_module(const struct client_request_data *crd);
    int generate_module(const struct client_request_data *crd,
			const vector<string> &argv,
			const string &tmp_dir,
			const string &stdout_path,
			const string &stderr_path);

private:
    // <kernel version, build tree path>
    map<string, string> supported_kernels;
};


local_backend::local_backend()
{
    glob_t globber;
    string pattern = "/lib/modules/*/build";
    int rc = glob(pattern.c_str(), GLOB_ERR, NULL, &globber);

    if (rc) {
	// We weren't able to find any kernel build trees. This isn't
	// a fatal error, since one of the other backends might be
	// able to satisfy requests.
	return;
    }
    for (unsigned int i = 0; i < globber.gl_pathc; i++) {
	string path = globber.gl_pathv[i];

	supported_kernels.insert({kernel_release_from_build_tree(path), path});
    }
    globfree(&globber);
}

bool
local_backend::can_generate_module(const struct client_request_data *crd)
{
    // Search our list of supported kernels for a match.
    for (auto it = supported_kernels.begin(); it != supported_kernels.end();
	 it++) {
	if (it->first == crd->kver) {
	    return true;
	}
    }

    return false;
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


class docker_backend : public backend_base
{
public:
    bool can_generate_module(const struct client_request_data *crd);
    int generate_module(const struct client_request_data *crd,
			const vector<string> &argv,
			const string &tmp_dir,
			const string &stdout_path,
			const string &stderr_path);
};

bool
docker_backend::can_generate_module(const struct client_request_data *)
{
    // FIXME: We'll have to see if we have a docker file for that
    // distro and the arches match.
    return false;
}

int
docker_backend:: generate_module(const struct client_request_data *,
				const vector<string> &,
				const string &,
				const string &,
				const string &)
{
    // FIXME: Here we'll need to generate a docker file, run docker to
    // create the container (and get all the right requirements
    // installed), copy any files over, then finally run "docker exec"
    // to actually run stap.
    return -1;
}

void
get_backends(vector<backend_base *> &backends)
{
    static vector<backend_base *>saved_backends;

    if (saved_backends.empty()) {
	saved_backends.push_back(new local_backend());
	saved_backends.push_back(new docker_backend());
    }
    backends.clear();
    backends = saved_backends;
}
