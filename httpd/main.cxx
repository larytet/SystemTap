// systemtap compile-server web api server
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "server.h"
#include "iostream"
#include "iomanip"
#include <sstream>
#include "../util.h"

extern "C" {
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/signalfd.h>
#include <uuid/uuid.h>
#include <json-c/json_object.h>
}

string get_uuid_representation(const uuid_t uuid)
{
    ostringstream os;

    os << hex << setfill('0');
    for (const unsigned char *ptr = uuid; ptr < uuid + sizeof(uuid_t); ptr++)
        os << setw(2) << (unsigned int)*ptr;
    return os.str();
}

struct build_info
{
    uuid_t uuid;
    string uuid_str;
    string uri;

    string kver;
    string arch;
    vector<string> cmd_args;

    build_info() {
	uuid_generate(uuid);
	uuid_str = get_uuid_representation(uuid);
	uri = "/builds/" + uuid_str;
    }

    string content();
    void start_module_build();
};

string build_info::content()
{
    ostringstream os;
    os << "{" << endl;
    os << "  \"uuid\": \"" << uuid_str << "\"" << endl;
    os << "  \"kver\": \"" << kver << "\"" << endl;
    os << "  \"arch\": \"" << arch << "\"" << endl;

    os << "  \"cmd_args\": [" << endl;
    for (auto it = cmd_args.begin(); it != cmd_args.end(); it++) {
	struct json_object *j = json_object_new_string((*it).c_str());
	if (j) {
	    os << "    "
	       << json_object_to_json_string_ext(j, JSON_C_TO_STRING_PLAIN)
	       << endl;
	    json_object_put(j);
	}
    }
    os << "  ]" << endl;

    os << "}" << endl;
    return os.str();
}

void build_info::start_module_build()
{
    stap_spawn(1, cmd_args, NULL);
}


mutex builds_mutex;
vector<build_info *> build_infos;

static void cleanup()
{    
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(builds_mutex);
	for (size_t idx = 0; idx < build_infos.size(); idx++) {
	    delete build_infos[idx];
	}
    }
}

class build_collection : public request_handler
{
public:
    response POST(const request &req);

    build_collection(string n) : request_handler(n) {}
};

response build_collection::POST(const request &req)
{
    // Create a build with the information we've gathered.
    build_info *b = new build_info;
    for (auto it = req.params.begin(); it != req.params.end(); it++) {
	if (it->first == "kver") {
	    b->kver = it->second[0];
	}
	else if (it->first == "arch") {
	    b->arch = it->second[0];
	}
	else if (it->first == "cmd_args") {
	    b->cmd_args = it->second;
	}
    }

    // Make sure we've got everything we need.
    if (b->kver.empty() || b->arch.empty() || b->cmd_args.empty()) {
	// Return an error.
	clog << "400 - bad request" << endl;
	response error400(400);
	error400.content = "<h1>Bad request</h1>";
	return error400;
    }

    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(builds_mutex);
	build_infos.push_back(b);
    }

    clog << "Returning a 202" << endl;
    response resp(202);
    resp.headers["Location"] = b->uri;
    resp.headers["Retry-After"] = "20";
    return resp;
}

class individual_build : public request_handler
{
public:
    response GET(const request &req);

    individual_build(string n) : request_handler(n) {}
};

response individual_build::GET(const request &req)
{
    clog << "individual_build::GET" << endl;

    // matches[0] is the entire string '/builds/XXXX'. matches[1] is
    // just the buildid 'XXXX'.
    string buildid = req.matches[1];
    build_info *b = NULL;
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(builds_mutex);
	for (auto it = build_infos.begin(); it != build_infos.end(); it++) {
	    clog << "Comparing '" << buildid << "'to '" << (*it)->uuid_str << "'" << endl;
	    if (buildid == (*it)->uuid_str) {
		b = *it;
		break;
	    }
	}
    }

    if (b == NULL) {
	clog << "Couldn't find build '" << buildid << "'" << endl;
	return get_404_response();
    }

    response rsp(200, "application/json");
    rsp.content = b->content();
    return rsp;
}

build_collection builds("build collection");
individual_build build("individual build");

server *httpd = NULL;

static void *
signal_thread(void *arg)
{
    int signal_fd = (int)(long)arg;

    while (1) {
	struct signalfd_siginfo si;
	ssize_t s;

	s = read(signal_fd, &si, sizeof(si));
	if (s != sizeof(si)) {
	    cerr << "signal fd read error: "<< strerror(errno) << endl;
	    continue;
	}

	// FIXME: we might think about using SIGHUP to aks us to
	// re-read configuration data.
	if (si.ssi_signo == SIGINT || si.ssi_signo == SIGTERM
	    || si.ssi_signo == SIGHUP || si.ssi_signo == SIGQUIT
	    || si.ssi_signo == SIGCHLD) {

	    // Since we're using signalfd(), we can call code that
	    // isn't signal-safe (like server::stop).
	    httpd->stop();
	    break;
	}
    }
    close(signal_fd);
    return NULL;
}

static void
setup_main_signals(pthread_t *tid)
{
    static sigset_t s;

    /* Block several signals; other threads created by main() will
     * inherit a copy of the signal mask. */
    sigemptyset(&s);
    sigaddset(&s, SIGINT);
    sigaddset(&s, SIGTERM);
    sigaddset(&s, SIGHUP);
    sigaddset(&s, SIGQUIT);
    sigaddset(&s, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &s, NULL);

    /* Create a signalfd. This way we can synchronously handle the
     * signals. */
    int signal_fd = signalfd(-1, &s, SFD_CLOEXEC);
    if (signal_fd < 0) {
	cerr << "Failed to create signal file descriptor: "
	     << strerror(errno) << endl;
	exit(1);
    }

    /* Let the special signal thread handle signals. */
    if (pthread_create(tid, NULL, signal_thread, (void *)(long)signal_fd) < 0) {
	cerr << "Failed to create thread: " << strerror(errno) << endl;
	exit(1);
    }
}

int
main(int /*argc*/, char *const /*argv*/[])
{
    pthread_t tid;

    setup_main_signals(&tid);

    httpd = new server(1234);
    httpd->add_request_handler("/builds$", builds);
    httpd->add_request_handler("/builds/([0-9a-f]+)$", build);

    // Wait for the server to shut itself down.
    httpd->wait();
    delete httpd;

    // Clean up the signal thread.
    pthread_join(tid, NULL);

    // Do other cleanup.
    cleanup();
    return 0;
}
