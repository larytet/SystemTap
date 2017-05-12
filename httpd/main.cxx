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

extern "C" {
#include <unistd.h>
#if 0
#include <signal.h>
#include <errno.h>
#endif
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
    string cmdline;

    build_info() {
	uuid_generate(uuid);
	uuid_str = get_uuid_representation(uuid);
	uri = "/builds/" + uuid_str;
    }

    string content();
};

string build_info::content()
{
    ostringstream os;
    os << "{" << endl;
    os << "  \"uuid\": \"" << uuid_str << "\"" << endl;
    os << "  \"kver\": \"" << kver << "\"" << endl;
    os << "  \"arch\": \"" << arch << "\"" << endl;

    struct json_object *j = json_object_new_string(cmdline.c_str());
    if (j) {
	os << "  \"cmdline\": "
	   << json_object_to_json_string_ext(j, JSON_C_TO_STRING_PLAIN) << endl;
    }

    os << "}" << endl;
    return os.str();
}

mutex builds_mutex;
vector<build_info *> build_infos;

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
	    b->kver = it->second;
	}
	else if (it->first == "arch") {
	    b->arch = it->second;
	}
	else if (it->first == "cmdline") {
	    b->cmdline = it->second;
	}
    }

    // Make sure we've got everything we need.
    if (b->kver.empty() || b->arch.empty() || b->cmdline.empty()) {
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

int
main(int /*argc*/, char *const /*argv*/[])
{
    server httpd(1234);

    httpd.add_request_handler("/builds$", builds);
    httpd.add_request_handler("/builds/([0-9a-f]+)$", build);
    // FIXME: Should this be pthread_cond_wait()/pthread_cond_timedwait()?
    while (1) {
	sleep(1);
    }
    return 0;
}
