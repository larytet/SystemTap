// systemtap compile-server web api server
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "server.h"
#include "api.h"
#include <iostream>

extern "C" {
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/signalfd.h>
}

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
	    || si.ssi_signo == SIGHUP || si.ssi_signo == SIGQUIT) {

	    // Since we're using signalfd(), we can call code that
	    // isn't signal-safe (like server::stop).
	    if (httpd)
		httpd->stop();
	    break;
	}
	else if (si.ssi_signo == SIGCHLD) {
	    // We just ignore SIGCHLD. We need to keep it enabled for
	    // waitpid() to work properly.
	}
	else {
	    cerr << "Got unhandled signal " << si.ssi_signo << endl;
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

    // Create the server and ask the api to register its handlers.
    httpd = new server(1234);
    api_add_request_handlers(*httpd);

    // Wait for the server to shut itself down.
    httpd->wait();
    delete httpd;

    // Clean up the signal thread.
    pthread_join(tid, NULL);

    // Ask the api to do its cleanup.
    api_cleanup();
    return 0;
}
