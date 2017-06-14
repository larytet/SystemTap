// systemtap compile-server web api server
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "api.h"
#include "server.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include "../util.h"

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glob.h>
#include <sched.h>
#include <uuid/uuid.h>
#include <json-c/json_object.h>
}

static string get_uuid_representation(const uuid_t uuid)
{
    ostringstream os;

    os << hex << setfill('0');
    for (const unsigned char *ptr = uuid; ptr < uuid + sizeof(uuid_t); ptr++)
        os << setw(2) << (unsigned int)*ptr;
    return os.str();
}

class resource
{
public:
    resource(string resource_base) {
	uuid_t uuid;

	uuid_generate(uuid);
	uuid_str = get_uuid_representation(uuid);
	uri = resource_base + uuid_str;
    }

    virtual ~resource() { }

    string get_uuid_str() {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(res_mutex);
	return uuid_str;
    }

    string get_uri() {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(res_mutex);
	return uri;
    }

    virtual void generate_response(response &r) = 0;
    virtual void generate_file_response(response &r, string &) { 
	r = get_404_response();
    }

protected:
    mutex res_mutex;
    string uuid_str;
    string uri;
};

class result_info : public resource
{
public:
    result_info(int rc, string &out_path, string &err_path, string &mp,
		mode_t mm)
	: resource("/results/"), rc(rc), stdout_path(out_path),
	  stderr_path(err_path), module_path(mp), module_mode(mm)
    {
	size_t found = stdout_path.find_last_of("/");
	if (found != string::npos) {
	    stdout_file = stdout_path.substr(found + 1);
	}
	found = stderr_path.find_last_of("/");
	if (found != string::npos) {
	    stderr_file = stderr_path.substr(found + 1);
	}
	found = module_path.find_last_of("/");
	if (found != string::npos) {
	    module_file = module_path.substr(found + 1);
	}
    }

    void generate_response(response &r);
    void generate_file_response(response &r, string &f);

protected:
    int rc;
    string stdout_path;
    string stdout_file;
    string stderr_path;
    string stderr_file;
    string module_path;
    string module_file;
    mode_t module_mode;
};

class build_info : public resource
{
public:
    build_info(string &k, string &a, vector<string> &args, string &bd)
	: resource("/builds/")
    {
	builder_thread_running = false;
	kver = k;
	arch = a;
	cmd_args = args;
	base_dir = bd;
	result = NULL;
    }

    ~build_info() {
	if (builder_thread_running) {
	    pthread_join(builder_tid, NULL);
	    builder_thread_running = false;
	}
	if (result) {
	    delete result;
	    result = NULL;
	}
    }

    void generate_response(response &r);
    void start_module_build();

    bool is_build_finished() {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(res_mutex);
	return (result != NULL);
    }

private:
    string kver;
    string arch;
    vector<string> cmd_args;
    string base_dir;

    bool builder_thread_running;
    pthread_t builder_tid;

    static void *module_build_shim(void *arg);
    void *module_build();
    result_info *result;
};

void result_info::generate_response(response &r)
{
    ostringstream os;

    r.status_code = 200;
    r.content_type = "application/json";
    os << "{" << endl;
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(res_mutex);

	os << "  \"uuid\": \"" << uuid_str << "\"";
	os << "," << endl << "  \"rc\": " << rc;

	// Always output stdout and stderr.
	if (!stdout_path.empty()) {
	    os << "," << endl << "  \"stdout_location\": \""
	       << uri + '/' + stdout_file << "\"";
	}
	if (!stderr_path.empty()) {
	    os << "," << endl << "  \"stderr_location\": \""
	       << uri + '/' + stderr_file << "\"";
	}

	// Here we output any extra files, like a module. For each
	// file print the location and mode (in decimal, since JSON
	// doesn't do octal).
	if (!module_file.empty()) {
	    os << "," << endl << "  \"files\": [";
	    os << endl << "    { \"location\": \""
	       << uri + '/' + module_file
	       << "\", \"mode\": " << module_mode << " }";
	    os << endl << "  ]";
	}
    }
    os << endl << "}" << endl;
    r.content = os.str();
}

void result_info::generate_file_response(response &r, string &file)
{
    // We don't want to serve any old file (which would be a security
    // hole), only the files we told the user about.
    clog << "Trying to retrieve file '" << file << "'" << endl;
    string path;
    r.status_code = 200;
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(res_mutex);

	if (!stdout_path.empty() && file == stdout_file) {
	    path = stdout_path;
	}
	else if (!stderr_path.empty() && file == stderr_file) {
	    path = stderr_path;
	}
	else if (!module_file.empty() && file == module_file) {
	    path = module_path;
	}
    }

    if (!path.empty()) {
	cerr << "File requested:  " << file << endl;
	cerr << "Served from   :  " << path << endl;
	r.file = path;
    }
    else {
	cerr << "Couldn't find file" << endl;
	r = get_404_response();
    }
}

void build_info::generate_response(response &r)
{
    ostringstream os;

    r.content_type = "application/json";
    os << "{" << endl;
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(res_mutex);

	if (result == NULL) {
	    r.status_code = 200;
	}
	else {
	    r.status_code = 303;
	    r.headers["Location"] = result->get_uri();
	}

	os << "  \"uuid\": \"" << uuid_str << "\"," << endl;
	os << "  \"kver\": \"" << kver << "\"," << endl;
	os << "  \"arch\": \"" << arch << "\"," << endl;

	os << "  \"cmd_args\": [" << endl;
	bool first = true;
	for (auto it = cmd_args.begin(); it != cmd_args.end(); it++) {
	    struct json_object *j = json_object_new_string((*it).c_str());
	    if (j) {
		if (!first)
		    os << "," << endl;
		else
		    first = false;
		os << "    "
		   << json_object_to_json_string_ext(j, JSON_C_TO_STRING_PLAIN);
		json_object_put(j);
	    }
	}
	os << endl << "  ]" << endl;
    }
    os << "}" << endl;
    r.content = os.str();
}

void build_info::start_module_build()
{
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(res_mutex);
	if (builder_thread_running) {
	    // This really shouldn't happen. Error out.
	    cerr << "Mulitple attempts to build moulde." << endl;
	    return;
	}
	builder_thread_running = true;
    }

    /* Create a thread to handle the module build. */
    if (pthread_create(&builder_tid, NULL, module_build_shim, this) < 0) {
	cerr << "Failed to create thread: " << strerror(errno) << endl;
	exit(1);
    }
}


mutex builds_mutex;
vector<build_info *> build_infos;

mutex results_mutex;
vector<result_info *> result_infos;


class build_collection_rh : public request_handler
{
public:
    response POST(const request &req);

    build_collection_rh(string n) : request_handler(n) {}
};

response build_collection_rh::POST(const request &req)
{
    // Gather up the info we need.
    string kver, arch, base_dir;
    vector<string> cmd_args;
    vector<string> files;
    for (auto it = req.params.begin(); it != req.params.end(); it++) {
	if (it->first == "kver") {
	    kver = it->second[0];
	}
	else if (it->first == "arch") {
	    arch = it->second[0];
	}
	else if (it->first == "cmd_args") {
	    cmd_args = it->second;
	}
    }
    if (! req.files.empty()) {
	clog << "Files received:" << endl;
	base_dir = req.base_dir;
	for (auto i = req.files.begin(); i != req.files.end(); i++) {
	    for (auto j = i->second.begin(); j != i->second.end();
		 j++) {
		clog << *j << endl;
	    }
	}
    }

    // Make sure we've got everything we need.
    if (kver.empty() || arch.empty() || cmd_args.empty()) {
	// Return an error.
	clog << "400 - bad request" << endl;
	response error400(400);
	error400.content = "<h1>Bad request</h1>";
	return error400;
    }

    // Create a build with the information we've gathered.
    build_info *b = new build_info(kver, arch, cmd_args, base_dir);

    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(builds_mutex);
	build_infos.push_back(b);
    }

    // Kick off the module build.
    b->start_module_build();

    // Return a 202 response.
    clog << "Returning a 202" << endl;
    response resp(202);
    resp.headers["Location"] = b->get_uri();
    resp.headers["Retry-After"] = "10";
    return resp;
}

class individual_build_rh : public request_handler
{
public:
    response GET(const request &req);

    individual_build_rh(string n) : request_handler(n) {}
};

response individual_build_rh::GET(const request &req)
{
    clog << "individual_build_rh::GET" << endl;

    // matches[0] is the entire string '/builds/XXXX'. matches[1] is
    // just the buildid 'XXXX'.
    string buildid = req.matches[1];
    build_info *b = NULL;
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(builds_mutex);
	for (auto it = build_infos.begin(); it != build_infos.end(); it++) {
	    if (buildid == (*it)->get_uuid_str()) {
		b = *it;
		break;
	    }
	}
    }

    if (b == NULL) {
	clog << "Couldn't find build '" << buildid << "'" << endl;
	return get_404_response();
    }

    response rsp(0);
    b->generate_response(rsp);
    return rsp;
}

class individual_result_rh : public request_handler
{
public:
    response GET(const request &req);

    individual_result_rh(string n) : request_handler(n) {}
};

response individual_result_rh::GET(const request &req)
{
    clog << "individual_result_rh::GET" << endl;

    // matches[0] is the entire string '/results/XXXX'. matches[1] is
    // just the id_str 'XXXX'.
    string id_str = req.matches[1];
    result_info *ri = NULL;
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(results_mutex);
	for (auto it = result_infos.begin(); it != result_infos.end(); it++) {
	    if (id_str == (*it)->get_uuid_str()) {
		ri = *it;
		break;
	    }
	}
    }

    if (ri == NULL) {
	clog << "Couldn't find result id '" << id_str << "'" << endl;
	return get_404_response();
    }

    response rsp(0);
    ri->generate_response(rsp);
    return rsp;
}

class result_file_rh : public request_handler
{
public:
    response GET(const request &req);

    result_file_rh(string n) : request_handler(n) {}
};

response result_file_rh::GET(const request &req)
{
    clog << "result_file_rh::GET" << endl;

    // matches[0] is the entire string
    // '/results/XXXX/FILE'. matches[1] is the result uuid string
    // 'XXXX'. matches[2] is the filename 'FILE'.
    string id_str = req.matches[1];
    string file_str = req.matches[2];
    result_info *ri = NULL;
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(results_mutex);
	for (auto it = result_infos.begin(); it != result_infos.end(); it++) {
	    if (id_str == (*it)->get_uuid_str()) {
		ri = *it;
		break;
	    }
	}
    }

    if (ri == NULL) {
	clog << "Couldn't find result id '" << id_str << "'" << endl;
	return get_404_response();
    }

    response rsp(0);
    ri->generate_file_response(rsp, file_str);
    return rsp;
}

build_collection_rh builds_rh("build collection");
individual_build_rh build_rh("individual build");
individual_result_rh result_rh("individual result");
result_file_rh result_file_rh("result file");

void *
build_info::module_build_shim(void *arg)
{
    build_info *bi = static_cast<build_info *>(arg);

    return bi->module_build();
}

void *
build_info::module_build()
{
    vector<string> argv;
    char tmp_dir_template[] = "/tmp/stap-httpd.XXXXXX";
    char *tmp_dir;

    // Process the command arguments.
    argv.push_back("stap");

#if 0
//FIXME: more thinking needed here.
    argv.push_back("-r");
    argv.push_back(kver);
#endif

    // Create a temporary directory for stap to use.
    tmp_dir = mkdtemp(tmp_dir_template);
    if (tmp_dir == NULL) {
	clog << "mkdtemp failed: " << strerror(errno) << endl;
	return NULL;
    }
    string tmpdir_opt = string("--tmpdir=") + tmp_dir;
    argv.push_back(tmpdir_opt);

    // Add the "client options" argument, which tells stap to do some
    // extra command line validation and to stop at pass 4.
    argv.push_back("--client-options");

    // Add the rest of the client's arguments.
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(res_mutex);
	for (auto it = cmd_args.begin(); it != cmd_args.end(); it++) {
	    argv.push_back(*it);
	}
    }

    // Handle capturing stdout and stderr (along with using /dev/null
    // for stdin).
    posix_spawn_file_actions_t actions;
    string stdout_path = string(tmp_dir) + "/stdout";
    string stderr_path = string(tmp_dir) + "/stderr";
    int rc = posix_spawn_file_actions_init(&actions);
    if (rc == 0) {
	rc = posix_spawn_file_actions_addopen(&actions, 0, "/dev/null",
					      O_RDONLY, S_IRWXU);
    }
    if (rc == 0) {
	rc = posix_spawn_file_actions_addopen(&actions, 1, stdout_path.c_str(),
					      O_WRONLY|O_CREAT|O_EXCL,
					      S_IRWXU);
    }
    if (rc == 0) {
	rc = posix_spawn_file_actions_addopen(&actions, 2, stderr_path.c_str(),
					      O_WRONLY|O_CREAT|O_EXCL,
					      S_IRWXU);
    }
    if (rc != 0) {
	clog << "posix_spawn_file_actions failed: " << strerror(errno)
	     << endl;
	return NULL;
    }

    // If we've got a base_dir, we need to do a chdir() to that
    // directory. However, all threads in a process share the same
    // root directory and working directory. If we just do a chdir()
    // here and then call spawn, it is possible that a different
    // thread is also here and does a chdir() right after ours but
    // before the spawn. So, instead we'll call unshare() first to
    // "unshare" the thread's working directory. Then when we do a
    // chdir(), it won't affect the other threads working directory.
    if (!base_dir.empty()) {
	if (unshare(CLONE_FS) < 0) {
	    clog << "Error in unshare: " << strerror(errno) << endl;
	    return NULL;
	}
	if (chdir(base_dir.c_str()) < 0) {
	    clog << "Error in chdir: " << strerror(errno) << endl;
	    return NULL;
	}
    }

    // Kick off stap.
    pid_t pid;
    pid = stap_spawn(2, argv, &actions);
    clog << "spawn returned " << pid << endl;

    // If stap_spawn() failed, no need to wait.
    if (pid == -1) {
	clog << "Error in spawn: " << strerror(errno) << endl;
	return NULL;
    }

    // Wait on the spawned process to finish.
    int staprc = stap_waitpid(0, pid);
    if (staprc < 0) {				// stap_waitpid() failed
	return NULL;
    }

    // Update build_info.
    clog << "Spawned process returned " << staprc << endl;
    (void)posix_spawn_file_actions_destroy(&actions);

    // See if we built a module.
    string module_path;
    mode_t module_mode = 0;
    if (staprc == 0) {
	glob_t globber;
	string pattern = string(tmp_dir) + "/*.ko";
	int rc = glob(pattern.c_str(), GLOB_ERR, NULL, &globber);
	if (rc) {
	    clog << "Unable to find a module in " << tmp_dir << endl;
	}
	else if (globber.gl_pathc != 1) {
	    clog << "Too many modules (" << globber.gl_pathc << ") in "
		 << tmp_dir << endl;
	}
	else {
	    module_path = globber.gl_pathv[0];
	    // We've got a path. Also figure out the file mode by
	    // calling stat().
	    struct stat stbuf;
	    if (stat(module_path.c_str(), &stbuf) == 0) {
		module_mode = stbuf.st_mode & 07777;
	    }
	    else {
		module_path.clear();
	    }
	}
    }

    result_info *ri = new result_info(staprc, stdout_path, stderr_path,
				      module_path, module_mode);
    {
	// Use a lock_guard to ensure the mutex gets released
	// even if an exception is thrown.
	lock_guard<mutex> lock(res_mutex);
	result = ri;
    }
    {
	// Use a lock_guard to ensure the mutex gets released
	// even if an exception is thrown.
	lock_guard<mutex> lock(results_mutex);
	result_infos.push_back(ri);
    }

    return NULL;
}

void api_cleanup()
{    
    kill_stap_spawn(SIGTERM);
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(builds_mutex);
	for (size_t idx = 0; idx < build_infos.size(); idx++) {
	    delete build_infos[idx];
	}
    }
}

void api_add_request_handlers(server &httpd)
{
    httpd.add_request_handler("/builds$", builds_rh);
    httpd.add_request_handler("/builds/([0-9a-f]+)$", build_rh);
    httpd.add_request_handler("/results/([0-9a-f]+)$", result_rh);
    httpd.add_request_handler("/results/([^/]+)/([^/]+)$", result_file_rh);
}
