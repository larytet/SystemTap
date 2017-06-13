// systemtap compile-server web api server
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "server.h"
#include <iostream>
#include <string>
#include "../util.h"

extern "C"
{
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
}

using namespace std;

static int
get_key_values(void *cls, enum MHD_ValueKind /*kind*/,
	       const char *key, const char *value)
{
    struct request *rq_info = static_cast<struct request *>(cls);

    rq_info->params[key].push_back(value ? value : "");
    return MHD_YES;
}


struct upload_file_info
{
    string filename;
    int fd;

    upload_file_info() : fd(-1) {}
};

struct connection_info
{
    static int
    postdataiterator_shim(void *cls,
			  enum MHD_ValueKind kind,
			  const char *key,
			  const char *filename,
			  const char *content_type,
			  const char *transfer_encoding,
			  const char *data,
			  uint64_t off,
			  size_t size);

    int postdataiterator(enum MHD_ValueKind kind,
			 const char *key,
			 const char *filename,
			 const char *content_type,
			 const char *transfer_encoding,
			 const char *data,
			 uint64_t off,
			 size_t size);

    MHD_PostProcessor *postprocessor;
    map<string, vector<string>> post_params;

    string post_dir;
    map<string, vector<upload_file_info>> post_files;
    void post_files_cleanup();

    connection_info(struct MHD_Connection *connection)
    {
        postprocessor
	    = MHD_create_post_processor(connection, 1024,
					&connection_info::postdataiterator_shim,
					this);
    }

    ~connection_info()
    {
        if (postprocessor)
        {
            MHD_destroy_post_processor(postprocessor);
	    postprocessor = NULL;
        }
	post_files_cleanup();
    }
};

void
connection_info::post_files_cleanup()
{
    post_dir.clear();
    for (auto i = post_files.begin(); i != post_files.end(); i++) {
	if (! i->second.empty()) {
	    for (auto j = i->second.begin(); j != i->second.end();
		 j++) {
		if ((*j).fd == -1) {
		    close((*j).fd);
		    (*j).fd = -1;
		}
	    }
	}
    }
    post_files.clear();
}

int
connection_info::postdataiterator_shim(void *cls,
				       enum MHD_ValueKind kind,
				       const char *key,
				       const char *filename,
				       const char *content_type,
				       const char *transfer_encoding,
				       const char *data,
				       uint64_t off,
				       size_t size)
{
    connection_info *con_info = static_cast<connection_info *>(cls);

    if (!cls)
	return MHD_NO;
    return con_info->postdataiterator(kind, key, filename, content_type,
				      transfer_encoding, data, off, size);
}

int
connection_info::postdataiterator(enum MHD_ValueKind kind,
				  const char *key,
				  const char *filename,
				  const char */*content_type*/,
				  const char */*transfer_encoding*/,
				  const char *data,
				  uint64_t /*offset*/,
				  size_t size)
{
    if (filename && key) {
	clog << __FUNCTION__ << ":" << __LINE__
	     << ": key='" << key
	     << "', filename='" << filename 
	     << ", size=" << size << endl;

	// If we've got a filename, we need a temporary directory to
	// put it in. Otherwise if we're handling multiple requests at
	// once, the requests could overwrite each other's files.
	if (post_dir.empty()) {
	    char tmpdir[PATH_MAX];
	    snprintf(tmpdir, PATH_MAX, "%s/stap-server.XXXXXX",
		     getenv("TMPDIR") ?: "/tmp");
	    char *rc = mkdtemp(tmpdir);
	    if (rc == NULL) {
		return MHD_NO;
	    }
	    post_dir = rc;
	}

	// See if we've seen this key before.
	upload_file_info ufi;
	auto it = post_files.find(key);
	// We've seen this key before, but perhaps not this filename.
	bool filename_found = false;
	if (it != post_files.end()) {
	    for (auto j = it->second.begin(); j != it->second.end(); j++) {
		if ((*j).filename == filename) {
		    ufi = (*j);
		    filename_found = true;
		    break;
		}
	    }
	}

	if (! filename_found) {
	    // If we haven't seen this filename before, validate the
	    // path. We only want a plain filename - no directories
	    // ('/') or relative paths ('..') allowed. We don't want
	    // the user trying anything sneaky like
	    // "../../etc/passwd".
	    ufi.filename = filename;
	    if (ufi.filename.find("..") != string::npos
		|| ufi.filename.find('/') != string::npos) {
		return MHD_NO;
	    }

	    // Open the file.
	    string full_path = post_dir + "/" + filename;
	    ufi.fd = open(full_path.c_str(),
			  O_WRONLY|O_CLOEXEC|O_CREAT|O_EXCL, S_IRWXU);
	    if (ufi.fd < 0) {
		return MHD_NO;
	    }

	    // Remember this file. Note that key values don't have to
	    // be unique.
	    post_files[key].push_back(ufi);
	}	
	if (size > 0) {
	    ssize_t rc = write(ufi.fd, data, size);
	    if (rc < 0 || (size_t)rc != size)
		return MHD_NO;
	}
    }
    else if (key) {
	return get_key_values(&post_params, kind, key, data);
    }
    return MHD_YES;
}

response
get_404_response()
{
    response error404(404);
    error404.content = "<h1>Not found</h1>";
    return error404;
}

response
request_handler::GET(const request &)
{
    return get_404_response();
}
response
request_handler::PUT(const request &)
{
    return get_404_response();
}
response
request_handler::POST(const request &)
{
    return get_404_response();
}
response
request_handler::DELETE(const request &)
{
    return get_404_response();
}

void
server::add_request_handler(const string &url_path_re, request_handler &handler)
{
    // Use a lock_guard to ensure the mutex gets released even if an
    // exception is thrown.
    lock_guard<mutex> lock(srv_mutex);
    request_handlers.push_back(make_tuple(url_path_re, &handler));
}

int
server::access_handler_shim(void *cls,
			    struct MHD_Connection *connection,
			    const char *url,
			    const char *method,
			    const char *version,
			    const char *upload_data,
			    size_t *upload_data_size,
			    void **con_cls)
{
    server *srv = static_cast<server *>(cls);

    if (!cls)
	return MHD_NO;
    return srv->access_handler(connection, url, method, version,
			       upload_data, upload_data_size, con_cls);
}

enum class request_method
{
    UNKNOWN = 0,
    GET,
    POST,
    PUT,
    DELETE,
};


int
server::access_handler(struct MHD_Connection *connection,
		       const char *url,
		       const char *method,
		       const char */*version*/,
		       const char *upload_data,
		       size_t *upload_data_size,
		       void **con_cls)
{
    string url_str = url;

    if (! *con_cls)
    {
	// Allocate connection info struct here.
	connection_info *con_info = new connection_info(connection);
	if (!con_info) {
	    // FIXME: should we queue a response here?
	    return MHD_NO;
	}
	*con_cls = con_info;
	return MHD_YES;
    }

    // Convert the method string to an value.
    request_method rq_method;
    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
	rq_method = request_method::GET;
    }
    else if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
	rq_method = request_method::POST;
    }
    else if (strcmp(method, MHD_HTTP_METHOD_PUT) == 0) {
	rq_method = request_method::PUT;
    }
    else if (strcmp(method, MHD_HTTP_METHOD_DELETE) == 0) {
	rq_method = request_method::DELETE;
    }
    else {
	// We got a method we don't support. Fail.
	return queue_response(get_404_response(), connection);
    }

    struct request rq_info;
    request_handler *rh = NULL;
    clog << "Looking for a matching request handler match with '"
	 << url_str << "'..." << endl;
    {
	// Use a lock_guard to ensure the mutex gets released even if an
	// exception is thrown.
	lock_guard<mutex> lock(srv_mutex);

	// Search the request handlers for a matching entry.
	for (auto it = request_handlers.begin();
	     it != request_handlers.end(); it++) {
	    string url_path_re = get<0>(*it);
	    if (regexp_match(url_str, url_path_re, rq_info.matches) == 0) {
		rh = get<1>(*it);
		clog << "Found a match with '" << rh->name << "'" << endl;
		break;
	    }
	}
    }

    // If we didn't find a request handler, return an error.
    if (rh == NULL)
	return queue_response(get_404_response(), connection);

    // Prepare to call the appropriate request handler method by
    // gathering up all the request info.
    enum MHD_ValueKind kind = ((rq_method == request_method::POST)
			       ? MHD_POSTDATA_KIND
			       : MHD_GET_ARGUMENT_KIND);
    MHD_get_connection_values(connection, kind, &get_key_values,
			      &rq_info);

    // POST data might or might not have been handled by
    // MHD_get_connection_values(). We have to post-process the POST
    // data.
    connection_info *con_info = static_cast<connection_info *>(*con_cls);
    if (*upload_data_size != 0) {
	if (MHD_post_process(con_info->postprocessor, upload_data,
			     *upload_data_size) == MHD_NO) {
	    // FIXME: What should we do here?
	    return MHD_NO;
	}
	// Let MHD know we processed everything.
	*upload_data_size = 0;
	return MHD_YES;
    }
    else if (!con_info->post_params.empty()) {
	// Copy all the POST parameters into the request parameters.
	rq_info.params.insert(con_info->post_params.begin(),
			      con_info->post_params.end());
	con_info->post_params.clear();
    }
    if (!con_info->post_files.empty()) {
	// Copy all the POST file info into the request.
	rq_info.base_dir = con_info->post_dir;
	for (auto i = con_info->post_files.begin();
	     i != con_info->post_files.end(); i++) {
	    for (auto j = i->second.begin(); j != i->second.end(); j++) {
		rq_info.files[i->first].push_back(j->filename);
	    }
	}
	con_info->post_files_cleanup();
    }

    // Now that all the request info has been gathered, call the right
    // method and pass it the request info.
    switch (rq_method) {
      case request_method::GET:
	return queue_response(rh->GET(rq_info), connection);
      case request_method::POST:
	return queue_response(rh->POST(rq_info), connection);
      case request_method::PUT:
	return queue_response(rh->PUT(rq_info), connection);
      case request_method::DELETE:
	return queue_response(rh->DELETE(rq_info), connection);
      default:
	return queue_response(get_404_response(), connection);
    }
}

int
server::queue_response(const response &response, MHD_Connection *connection)
{
    struct MHD_Response *mhd_response;

    // If we have a file, ignore the rest of the response.
    if (! response.file.empty()) {
	int fd = open(response.file.c_str(), O_RDONLY);
	// If we can't open the file, send a 404.
	if (fd < 0) {
	    return queue_response(get_404_response(), connection);
	}
	struct stat stat_buf;
	if (fstat(fd, &stat_buf) < 0) {
	    close(fd);
	    return queue_response(get_404_response(), connection);
	}	    
	mhd_response = MHD_create_response_from_fd(stat_buf.st_size, fd);
    }
    else {
	mhd_response = MHD_create_response_from_buffer(response.content.length(),
						       (void *) response.content.c_str(),
						       MHD_RESPMEM_MUST_COPY);
    }
    if (mhd_response == NULL) {
	return MHD_NO;
    }

    for (auto it = response.headers.begin(); it != response.headers.end();
	 it++) {
        MHD_add_response_header(mhd_response, it->first.c_str(),
				it->second.c_str());
    }
    MHD_add_response_header(mhd_response, MHD_HTTP_HEADER_CONTENT_TYPE,
			    response.content_type.c_str());

//    MHD_add_response_header(mhd_response, MHD_HTTP_HEADER_SERVER, server_identifier_.c_str());
    int ret = MHD_queue_response(connection, response.status_code,
				 mhd_response);
    MHD_destroy_response (mhd_response);
    return ret;
}

void
server::start()
{
    dmn_ipv4 = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY
#ifdef MHD_USE_EPOLL
				| MHD_USE_EPOLL
#ifdef MHD_USE_EPOLL_TURBO
				| MHD_USE_EPOLL_TURBO
#endif
#else
#ifdef MHD_USE_POLL
				| MHD_USE_POLL
#endif
#endif
				| MHD_USE_DEBUG,
				port,
				NULL, NULL, // default accept policy
				&server::access_handler_shim, this,
				MHD_OPTION_THREAD_POOL_SIZE, 4,
				MHD_OPTION_NOTIFY_COMPLETED,
				&server::request_completed_handler_shim, this,
				MHD_OPTION_END);

    if (dmn_ipv4 == NULL) {
	string msg = "Error starting microhttpd daemon on port "
	    + lex_cast(port);
	throw runtime_error(msg);
	return;
    }
}

void
server::stop()
{
    if (dmn_ipv4 != NULL) {
	clog << "Stopping daemon..." << endl;
	MHD_stop_daemon(dmn_ipv4);
	dmn_ipv4 = NULL;
        running_cv.notify_all(); // notify waiters
    }
}

void
server::wait()
{
    std::mutex m;
    {
        std::unique_lock<std::mutex> lk(m);
        running_cv.wait(lk, [this]{return (dmn_ipv4 == NULL);});
    }
}

void
server::request_completed_handler_shim(void *cls,
				       struct MHD_Connection *connection,
				       void **con_cls,
				       enum MHD_RequestTerminationCode toe)
{
    server *srv = static_cast<server *>(cls);

    if (!cls)
	return;
    return srv->request_completed_handler(connection, con_cls, toe);
}

void
server::request_completed_handler(struct MHD_Connection */*connection*/,
				  void **con_cls,
				  enum MHD_RequestTerminationCode /*toe*/)
{
    if (*con_cls) {
	struct connection_info *con_info = (struct connection_info *)*con_cls;
	delete con_info;
	*con_cls = NULL;
    }
}
