// -*- C++ -*-
// Copyright (C) 2017 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"

#ifdef HAVE_HTTP_SUPPORT
#include "session.h"
#include "client-http.h"
#include "util.h"
#include "staptree.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>


extern "C" {
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <json-c/json.h>
#include <sys/stat.h>
}

using namespace std;


class http_client
{
public:
  http_client (systemtap_session &s):
    root(0),
    s(s),
    curl(0),
    retry(0),
    location(nullptr) {}
  ~http_client () {if (curl) curl_easy_cleanup(curl);};

  json_object *root;
  std::string host;
  std::map<std::string, std::string> header_values;
  std::vector<std::string> script_files;
  enum download_type {json_type, file_type};

  bool download (const std::string & url, enum download_type type);
  bool post (const std::string & url, std::vector<std::tuple<std::string, std::string>> &request_parameters);
  void add_script_file (std::string script_type, std::string script_file);
  void get_header_field (const std::string & data, const std::string & field);
  static size_t get_data (void *ptr, size_t size, size_t nitems,
                          http_client * data);
  static size_t get_file (void *ptr, size_t size, size_t nitems,
                          FILE * stream);
  static size_t get_header (void *ptr, size_t size, size_t nitems,
                            http_client * data);
private:
  static std::string basename (std::string & pathname);
  systemtap_session &s;
  void *curl;
  int retry;
  std::string *location;
};

// TODO is there a better way than making this static?
static http_client *http;


size_t
http_client::get_data (void *ptr, size_t size, size_t nitems, http_client * client)
{
  string data ((const char *) ptr, (size_t) size * nitems);

  if (data.front () == '{')
    {
      enum json_tokener_error json_error;
      client->root = json_tokener_parse_verbose (data.c_str(), &json_error);

      if (client->s.verbose >= 3)
        clog << json_object_to_json_string (client->root) << endl;
      if (client->root == NULL)
        throw SEMANTIC_ERROR (json_tokener_error_desc (json_error));
    }
  return size * nitems;
}

size_t
http_client::get_header (void *ptr, size_t size, size_t nitems, http_client * client)
{
  string data ((const char *) ptr, (size_t) size * nitems);

  unsigned long colon = data.find(':');
  if (colon != string::npos)
    {
      string key = data.substr (0, colon);
      string value = data.substr (colon + 2, data.length() - colon - 4);
      client->header_values[key] = value;
    }

  return size * nitems;
}

size_t
http_client::get_file (void *ptr, size_t size, size_t nitems, std::FILE * stream)
{
  size_t written;
  written = fwrite (ptr, size, nitems, stream);
  std::fflush (stream);
  return written;
}

bool
http_client::download (const std::string & url, http_client::download_type type)
{
  struct curl_slist *headers = NULL;

  if (curl)
    curl_easy_reset (curl);
  curl = curl_easy_init ();
  curl_global_init (CURL_GLOBAL_ALL);
  curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
  curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt (curl, CURLOPT_NOSIGNAL, 1); //Prevent "longjmp causes uninitialized stack frame" bug
  curl_easy_setopt (curl, CURLOPT_ACCEPT_ENCODING, "deflate");
  headers = curl_slist_append (headers, "Accept: */*");
  headers = curl_slist_append (headers, "Content-Type: text/html");
  curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt (curl, CURLOPT_HTTPGET, 1);

  static void *
    lthis = this;
  if (type == json_type)
    {
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, lthis);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, http_client::get_data);
    }
  else if (type == file_type)
    {
      std::string filename = url;
      std::string ko_suffix = ".ko\"";
      std::string filepath;
      if (filename.back() == '/')
        filename.erase(filename.length()-1);

      if (std::equal(ko_suffix.rbegin(), ko_suffix.rend(), filename.rbegin()))
        filepath = s.tmpdir + "/" + s.module_name + ".ko";
      else
        filepath = s.tmpdir + "/" + filename.substr (filename.rfind ('/')+1);

      if (s.verbose >= 3)
        clog << "Downloaded " + filepath << endl;
      std::FILE *File = std::fopen (filepath.c_str(), "wb");
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, File);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, http_client::get_file);
    }
  curl_easy_setopt (curl, CURLOPT_HEADERDATA, lthis);
  curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, http_client::get_header);

  CURLcode res = curl_easy_perform (curl);

  if (res != CURLE_OK)
    {
      clog << "curl_easy_perform() failed: " << curl_easy_strerror (res) << endl;
      return false;
    }
  else
      return true;
}


std::string
http_client::basename (std::string &pathname)
{
  const size_t last_slash_idx = pathname.find_last_of("\\/");
  if (std::string::npos != last_slash_idx)
    return pathname.substr (last_slash_idx+1);
  else return pathname;
}


bool
http_client::post (const std::string & url, std::vector<std::tuple<std::string, std::string>> &request_parameters)
{
  struct curl_slist *headers=NULL;
  int still_running = false;
  struct curl_httppost *formpost=NULL;
  struct curl_httppost *lastptr=NULL;
// struct curl_slist *headerlist=NULL;
  CURLM *multi_handle;
  static const char buf[] = "Expect:";
  headers = curl_slist_append (headers, buf);

  for (vector<std::tuple<std::string, std::string>>::const_iterator it = request_parameters.begin ();
      it != request_parameters.end ();
      ++it)
    {
      string parm_type = get<0>(*it);
      char *parm_data = (char*)get<1>(*it).c_str();
      curl_formadd (&formpost,
          &lastptr,
          CURLFORM_COPYNAME, parm_type.c_str(),
          CURLFORM_COPYCONTENTS, parm_data,
          CURLFORM_END);
    }

  // Fill in the file upload field; libcurl will load data from the given file name
  for (vector<std::string>::const_iterator it = script_files.begin ();
      it != script_files.end ();
      ++it)
    {
      string script_file = (*it);
      string script_base = basename (script_file);

      curl_formadd (&formpost,
          &lastptr,
          CURLFORM_COPYNAME, script_base.c_str(),
          CURLFORM_FILE, script_file.c_str(),
          CURLFORM_END);
      // Fill in the filename field
      curl_formadd (&formpost,
          &lastptr,
          CURLFORM_COPYNAME, "files",
          CURLFORM_FILENAME, script_base.c_str(),
          CURLFORM_COPYCONTENTS, script_base.c_str(),
          CURLFORM_END);
      // script name is not in cmd_args so add it manually
      curl_formadd (&formpost,
          &lastptr,
          CURLFORM_COPYNAME, "cmd_args",
          CURLFORM_COPYCONTENTS, script_base.c_str(),
          CURLFORM_END);
    }

  // Fill in the submit field too
  curl_formadd (&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "submit",
               CURLFORM_COPYCONTENTS, "send",
               CURLFORM_END);

  multi_handle = curl_multi_init();

  curl_easy_setopt (curl, CURLOPT_URL, url.c_str());
  if (s.verbose >= 3)
    curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt (curl, CURLOPT_HTTPPOST, formpost);

  curl_multi_add_handle (multi_handle, curl);

  curl_multi_perform (multi_handle, &still_running);

  do {
      struct timeval timeout;
      int rc; // select() return code
      CURLMcode mc; // curl_multi_fdset() return code

      fd_set fdread;
      fd_set fdwrite;
      fd_set fdexcep;
      int maxfd = -1;

      long curl_timeo = -1;

      FD_ZERO (&fdread);
      FD_ZERO (&fdwrite);
      FD_ZERO (&fdexcep);

      // set a suitable timeout to play around with
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      curl_multi_timeout (multi_handle, &curl_timeo);
      if (curl_timeo >= 0)
        {
          timeout.tv_sec = curl_timeo / 1000;
          if (timeout.tv_sec > 1)
            timeout.tv_sec = 1;
          else
            timeout.tv_usec = (curl_timeo % 1000) * 1000;
        }

      // get file descriptors from the transfers
      mc = curl_multi_fdset (multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

      if (mc != CURLM_OK)
        {
          clog << "curl_multi_fdset() failed" << curl_multi_strerror (mc) << endl;
          return false;
        }

      /* On success the value of maxfd is guaranteed to be >= -1. We call
         select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
         no fds ready yet so we call select(0, ...)to sleep 100ms,
         the minimum suggested value */

      if (maxfd == -1)
        {
          struct timeval wait = { 0, 100 * 1000 }; // 100ms
          rc = select (0, NULL, NULL, NULL, &wait);
        }
      else
        rc = select (maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

      switch (rc)
      {
      case -1:
        /* select error */
        break;
      case 0:
      default:
        curl_multi_perform (multi_handle, &still_running);
        break;
      }
  } while (still_running);

  curl_multi_cleanup (multi_handle);

  curl_formfree (formpost);

  curl_slist_free_all (headers);

  return true;
}


void
http_client::add_script_file (std::string, std::string script_file)
{
  this->script_files.push_back (script_file);
}


int
http_client_backend::initialize ()
{
  http = new http_client (s);
  request_parameters.clear();
  request_files.clear();
  return 0;
}

int
http_client_backend::package_request ()
{
  return 0;
}

int
http_client_backend::find_and_connect_to_server ()
{
  for (vector<std::string>::const_iterator i = s.http_servers.begin ();
      i != s.http_servers.end ();
      ++i)
    if (http->download (*i + "/builds", http->json_type))
      {
        http->host = *i;
        if (! http->post (http->host + "/builds", request_parameters))
          return 1;
      }

  std::string::size_type found = http->host.find ("/builds");
  std::string uri;
  std::map<std::string, std::string>::iterator it_loc;
  it_loc = http->header_values.find("Location");
  if (it_loc == http->header_values.end())
    clog << "Cannot get location from server" << endl;
  if (found != std::string::npos)
    uri = http->host.substr (0, found) + http->header_values["Location"];
  else
    uri = http->host + http->header_values["Location"];

   while (true)
     {
       int retry = std::stoi (http->header_values["Retry-After"], nullptr, 10);
       if (s.verbose >= 2)
         clog << "Waiting " << retry << " seconds" << endl;
       sleep (retry);
       if (http->download (http->host + http->header_values["Location"], http->json_type))
         {
           json_object *files;
           json_object_object_get_ex (http->root, "files", &files);
           for (int k = 0; k < json_object_array_length (files); k++)
             {
               json_object *files_element = json_object_array_get_idx (files, k);
               json_object *loc;
               found = json_object_object_get_ex (files_element, "location", &loc);
               string location = json_object_to_json_string (loc);
               http->download (http->host + location, http->file_type);
             }
           break;
         }
       return 1;
     }
   json_object *stdio_loc;
   found = json_object_object_get_ex (http->root, "stderr_location", &stdio_loc);
   string stdio_loc_str = json_object_to_json_string (stdio_loc);
   http->download (http->host + stdio_loc_str, http->file_type);

   std::ifstream ferr(s.tmpdir + "/stderr");
   if (ferr.is_open())
     std::cout << ferr.rdbuf() << endl;
   ferr.close();

   found = json_object_object_get_ex (http->root, "stdout_location", &stdio_loc);
   stdio_loc_str = json_object_to_json_string (stdio_loc);
   http->download (http->host + stdio_loc_str, http->file_type);

   std::ifstream fout(s.tmpdir + "/stdout");
   if (fout.is_open())
     std::cout << fout.rdbuf() << endl;
   fout.close();

  return 0;
}

int
http_client_backend::unpack_response ()
{
  return 0;
}

int
http_client_backend::process_response ()
{
  return 0;
}

int
http_client_backend::add_protocol_version (const std::string &version)
{
  // Add the protocol version (so the server can ensure we're
  // compatible).
  request_parameters.push_back(make_tuple("version", version));
  return 0;
}

int
http_client_backend::add_sysinfo ()
{
  // Add the sysinfo.
  request_parameters.push_back(make_tuple("kver", s.kernel_release));
  request_parameters.push_back(make_tuple("arch", s.architecture));
  return 0;
}

int
http_client_backend::include_file_or_directory (const std::string &script_type,
						const std::string &script_file)
{
  // FIXME: this is going to be interesting. We can't add a whole
  // directory at one shot, we'll have to traverse the directory and
  // add each file, preserving the directory structure somehow.
  http->add_script_file (script_type, script_file);
  return 0;
}

int
http_client_backend::add_tmpdir_file (const std::string &file)
{
  request_files.push_back(make_tuple("files", file));
  return 0;
}

int
http_client_backend::add_cmd_arg (const std::string &arg)
{
  request_parameters.push_back(make_tuple("cmd_args", arg));
  return 0;
}

void
http_client_backend::add_localization_variable(const std::string &,
					       const std::string &)
{
  // FIXME: We'll probably just add to the request_parameters here.
  return;
}

void
http_client_backend::add_mok_fingerprint(const std::string &)
{
  // FIXME: We'll probably just add to the request_parameters here.
  return;
}

#endif /* HAVE_HTTP_SUPPORT */
