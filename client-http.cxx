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

#include <iostream>
#include <sstream>
#include <fstream>

extern "C" {
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/curlbuild.h>
#include <sys/stat.h>
}
using namespace std;


size_t
http_client_backend::get_data (void *ptr, size_t size, size_t nitems,
                         http_client_backend * client)
{
  string data ((const char *) ptr, (size_t) size * nitems);

  if (data.front () == '{')
    {
      Json::Reader reader;
      bool parsedSuccess = reader.parse (data,
                                         client->root,
                                         false);
      if (not parsedSuccess)
        clog << "Failed to parse JSON" << reader.getFormattedErrorMessages() << endl;
    }
  return size * nitems;
}

size_t
http_client_backend::get_header (void *ptr, size_t size, size_t nitems,
    http_client_backend * client)
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
http_client_backend::get_file (void *ptr, size_t size, size_t nitems,
                         std::FILE * stream)
{
  size_t written;
  written = fwrite(ptr, size, nitems, stream);
  std::fflush(stream);
  return written;
}

bool
http_client_backend::download (const std::string & url, http_client_backend::download_type type)
{
  struct curl_slist *headers = NULL;

  curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
  /* example.com is redirected, so we tell libcurl to follow redirection */
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
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, http_client_backend::get_data);
    }
  else if (type == file_type)
    {
      std::string filename = url;
      std::string ko_suffix = ".ko";
      std::string filepath;
      if (filename.back() == '/')
        filename.erase(filename.length()-1);

      if (std::equal(ko_suffix.rbegin(), ko_suffix.rend(), filename.rbegin()))
        filepath = s.tmpdir + "/" + s.module_name + ".ko";
      else
        filepath = s.tmpdir + "/" + filename.substr (filename.rfind ('/')+1);

      if (s.verbose >= 3)
        clog << "Downloaded " + filepath << endl;
      std::FILE *File = std::fopen(filepath.c_str(), "wb");
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, File);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, http_client_backend::get_file);
    }
  curl_easy_setopt (curl, CURLOPT_HEADERDATA, lthis);
  curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, http_client_backend::get_header);

  CURLcode
    res = curl_easy_perform (curl);

  if (res != CURLE_OK)
    {
      fprintf (stderr, "curl_easy_perform() failed in %s: %s\n",
               __FUNCTION__, curl_easy_strerror (res));
      return false;

    }
  else
      return true;
}

void
http_client_backend::post (const std::string & url, const std::string & data)
{
  struct curl_slist *headers = NULL;

  curl_easy_setopt (curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt (curl, CURLOPT_POST, 1);
  curl_easy_setopt (curl, CURLOPT_POSTFIELDS, data.c_str ());
  headers = curl_slist_append (headers, "Accept: */*");
  headers =
    curl_slist_append (headers,
                       "Content-Type: application/x-www-form-urlencoded");
  curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform (curl);
  /* Check for errors */
  if (res != CURLE_OK)
    {
      fprintf (stderr, "curl_easy_perform() failed in %s: %s\n",
               __FUNCTION__, curl_easy_strerror (res));
    }
}


int
http_client_backend::initialize ()
{
  request_parameters.clear();
  request_files.clear();
  curl = curl_easy_init ();
  retry = 0;
  location = nullptr;
  return 0;
}

int
http_client_backend::package_request ()
{
  const std::string query_parts [] =
       {"kver=",
        "&arch="
       };
  struct stat buffer;

  query = query_parts[0] + s.release + query_parts[1] + s.release.substr (s.release.rfind ('.')+1);

  for (vector<std::tuple<std::string, std::string>>::const_iterator it = request_parameters.begin ();
      it != request_parameters.end ();
      ++it)
    {
      string parm_type = get<0>(*it);
      if (parm_type == "cmd_args" && stat (get<1>(*it).c_str(), &buffer) != 0)
        query = query + "&cmd_args=" + get<1>(*it);
    }

  if (s.verbose >= 3)
    clog << "Query: " + query << endl;

  return 0;
}

int
http_client_backend::find_and_connect_to_server ()
{
  for (vector<std::string>::const_iterator i = s.http_servers.begin ();
      i != s.http_servers.end ();
      ++i)
    if (download (*i + "/builds", json_type))
      {
        host = *i;
        post (host + "/builds", query);
      }

  std::string::size_type found = host.find ("/builds");
  std::string uri;
  if (found != std::string::npos)
    uri = host.substr (0, found) + header_values["Location"];
  else
    uri = host + header_values["Location"];

   while (true)
     {
       int retry = std::stoi (header_values["Retry-After"], nullptr, 10);
       if (s.verbose >= 2)
         clog << "Waiting " << retry << " seconds...";
       sleep (retry);
       if (download (host + header_values["Location"], json_type))
         {
           const Json::Value uuid = root["uuid"];

           for(Json::Value::iterator it = root["files"].begin();
               it != root["files"].end(); ++it)

             for(unsigned int index=0; index < (*it).size()-1;
                  ++index)
               download (host + (*it)["location"].asString(), file_type);

           if (s.verbose >= 5)
             for(Json::Value::iterator it = root.begin();
                 it != root.end(); ++it)
               {
                 Json::Value val = (*it);
                 std::cout << it.key().asString() << ':' << (*it) << '\n';
               }
           break;
         }
     }

  download (host + root["stderr_location"].asString(), file_type);
  std::ifstream ferr(s.tmpdir + "/stderr");
  if (ferr.is_open())
    std::cout << ferr.rdbuf() << endl;
  ferr.close();
  download (host + root["stdout_location"].asString(), file_type);
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
http_client_backend::include_file_or_directory (const std::string &,
						const std::string &)
{
  // FIXME: this is going to be interesting. We can't add a whole
  // directory at one shot, we'll have to traverse the directory and
  // add each file, preserving the directory structure somehow.
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
