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
#include <sys/utsname.h>
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

    string distro_name;

    // The current architecture.
    string arch;
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

    // Notice we don't error if we can't get the distro name. This
    // isn't a fatal error, since other backends might be able to
    // handle this request.
    vector<string> info;
    get_distro_info(info);
    if (! info.empty()) {
	distro_name = info[0];
    }

    // Get the current arch name.
    struct utsname buf;
    (void)uname(&buf);
    arch = buf.machine;
}

bool
local_backend::can_generate_module(const struct client_request_data *crd)
{
    // See if we support the kernel/arch/distro combination.
    if (supported_kernels.count(crd->kver) == 1 && arch == crd->arch
	&& distro_name == crd->distro_name) {
	return true;
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
	rc = errno;
	clog << "Error in spawn: " << strerror(errno) << endl;
	(void)posix_spawn_file_actions_destroy(&actions);
	return rc;
    }

    // Wait on the spawned process to finish.
    rc = stap_waitpid(0, pid);
    if (rc < 0) {			// stap_waitpid() failed
	clog << "waitpid failed: " << strerror(errno) << endl;
	(void)posix_spawn_file_actions_destroy(&actions);
	return rc;
    }

    clog << "Spawned process returned " << rc << endl;
    (void)posix_spawn_file_actions_destroy(&actions);

    return rc;
}


class docker_backend : public backend_base
{
public:
    docker_backend();

    bool can_generate_module(const struct client_request_data *crd);
    int generate_module(const struct client_request_data *crd,
			const vector<string> &argv,
			const string &tmp_dir,
			const string &stdout_path,
			const string &stderr_path);

private:
    // The docker executable path.
    string docker_path;

    // The docker data directory.
    string datadir;
    
    // List of docker data filenames. <distro name, path>
    map<string, string> data_files;

    // The current architecture.
    string arch;
};


docker_backend::docker_backend()
{
    try {
	docker_path = find_executable("docker");
    }
    catch (...) {
	// It really isn't an error for the system to not have the
	// "docker" executable. We'll just disallow builds using the
	// docker backend (down in
	// docker_backend::can_generate_module()).
	docker_path.clear();
    }
    
    datadir = string(PKGDATADIR) + "/httpd/docker";

    glob_t globber;
    string pattern = datadir + "/*.json";
    int rc = glob(pattern.c_str(), GLOB_ERR, NULL, &globber);
    if (rc) {
	// We weren't able to find any JSON docker data files. This
	// isn't a fatal error, since one of the other backends might
	// be able to satisfy requests.
	return;
    }
    for (unsigned int i = 0; i < globber.gl_pathc; i++) {
	string path = globber.gl_pathv[i];
	
	size_t found = path.find_last_of("/");
	if (found != string::npos) {
	    // First, get the file basename ("FOO.json").
	    string filename = path.substr(found + 1);

	    // Now, chop off the .json extension.
	    size_t found = filename.find_last_of(".");
	    if (found != string::npos) {
		string distro = filename.substr(found + 1);
		data_files.insert({distro, path});
	    }
	}
    }
    globfree(&globber);

    // Get the current arch name.
    struct utsname buf;
    (void)uname(&buf);
    arch = buf.machine;
}

bool
docker_backend::can_generate_module(const struct client_request_data *crd)
{
    // If we don't have a docker executable, we're done.
    if (docker_path.empty())
	return false;

    // We have to see if we have a JSON data file for that distro and
    // the arches match.
    if (data_files.count(crd->distro_name) == 1 && arch == crd->arch) {
	return true;
    }

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
