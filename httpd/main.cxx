// systemtap compile-server web api server
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "server.h"

extern "C" {
#include <unistd.h>
#if 0
#include <signal.h>
#include <errno.h>
#endif
}
//using namespace std;

#define PAGE "<html><head><title>Error</title></head><body>Bad data</body></html>"

class request_handler build_collection("build collection");
class request_handler build("individual build");

int
main(int /*argc*/, char *const /*argv*/[])
{
    server httpd(1234);

    httpd.add_request_handler("/builds", build_collection);
    httpd.add_request_handler("/builds/[0-9]+", build);
    // FIXME: Should this be pthread_cond_wait()/pthread_cond_timedwait()?
    while (1) {
	sleep(1);
    }
    return 0;
}
