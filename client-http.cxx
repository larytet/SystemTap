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
#include "elaborate.h"

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
#include <ftw.h>
#include <rpm/rpmlib.h>
#include <rpm/header.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <search.h>
#include <elfutils/libdwfl.h>
#include <elfutils/libdw.h>
#include <fcntl.h>
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
    location(nullptr) { }
  ~http_client () {if (curl) curl_easy_cleanup(curl);};

  json_object *root;
  std::string host;
  std::map<std::string, std::string> header_values;
  enum download_type {json_type, file_type};

  bool download (const std::string & url, enum download_type type);
  bool post (const std::string & url, std::vector<std::tuple<std::string, std::string>> &request_parameters);
  void add_script_file (std::string script_type, std::string script_file);
  void add_module (std::string module);
  void get_header_field (const std::string & data, const std::string & field);
  static size_t get_data_shim (void *ptr, size_t size, size_t nitems, void *client);
  static size_t get_file (void *ptr, size_t size, size_t nitems, FILE * stream);
  static size_t get_header_shim (void *ptr, size_t size, size_t nitems, void *client);
  std::string get_rpmname (std::string & pathname);
  void get_buildid (string fname);
  void get_kernel_buildid (void);
  long get_response_code (void);

private:
  size_t get_header (void *ptr, size_t size, size_t nitems);
  size_t get_data (void *ptr, size_t size, size_t nitems);
  static int process_buildid_shim (Dwfl_Module *dwflmod, void **userdata, const char *name,
      Dwarf_Addr base, void *client);
  int process_buildid (Dwfl_Module *dwflmod);
  std::vector<std::string> script_files;
  std::vector<std::string> modules;
  std::vector<std::tuple<std::string, std::string>> buildids;
  systemtap_session &s;
  void *curl;
  int retry;
  std::string *location;
  std::string buildid;
};

// TODO is there a better way than making this static?
static http_client *http;


size_t
http_client::get_data_shim (void *ptr, size_t size, size_t nitems, void *client)
{
  http_client *http = static_cast<http_client *>(client);

  return http->get_data (ptr, size, nitems);
}

// Parse the json data at PTR having SIZE and NITEMS into root

size_t
http_client::get_data (void *ptr, size_t size, size_t nitems)
{
  string data ((const char *) ptr, (size_t) size * nitems);

  if (data.front () == '{')
    {
      enum json_tokener_error json_error;
      root = json_tokener_parse_verbose (data.c_str(), &json_error);

      if (s.verbose >= 3)
        clog << json_object_to_json_string (root) << endl;
      if (root == NULL)
        throw SEMANTIC_ERROR (json_tokener_error_desc (json_error));
    }
  return size * nitems;
}


size_t
http_client::get_header_shim (void *ptr, size_t size, size_t nitems, void *client)
{
  http_client *http = static_cast<http_client *>(client);

  return http->get_header (ptr, size, nitems);
}


// Extract header values at PTR having SIZE and NITEMS into header_values

size_t
http_client::get_header (void *ptr, size_t size, size_t nitems)
{
  string data ((const char *) ptr, (size_t) size * nitems);

  unsigned long colon = data.find(':');
  if (colon != string::npos)
    {
      string key = data.substr (0, colon);
      string value = data.substr (colon + 2, data.length() - colon - 4);
      header_values[key] = value;
    }

  return size * nitems;
}


// Put the  data, e.g. <module>.ko at PTR having SIZE and NITEMS into STREAM

size_t
http_client::get_file (void *ptr, size_t size, size_t nitems, std::FILE * stream)
{
  size_t written;
  written = fwrite (ptr, size, nitems, stream);
  std::fflush (stream);
  return written;
}


// Do a download of type TYPE from URL

bool
http_client::download (const std::string & url, http_client::download_type type)
{
  struct curl_slist *headers = NULL;

  if (curl)
    curl_easy_reset (curl);
  curl = curl_easy_init ();
  curl_global_init (CURL_GLOBAL_ALL);
  curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
  curl_easy_setopt (curl, CURLOPT_NOSIGNAL, 1); //Prevent "longjmp causes uninitialized stack frame" bug
  curl_easy_setopt (curl, CURLOPT_ACCEPT_ENCODING, "deflate");
  headers = curl_slist_append (headers, "Accept: */*");
  headers = curl_slist_append (headers, "Content-Type: text/html");
  curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt (curl, CURLOPT_HTTPGET, 1);

  if (type == json_type)
    {
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, http);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, http_client::get_data_shim);
    }
  else if (type == file_type)
    {
      std::string filename = url;
      std::string filepath;

      if (filename.back() == '/')
        filename.erase(filename.length()-1);
      filepath = s.tmpdir + "/" + filename.substr (filename.rfind ('/')+1);

      if (s.verbose >= 3)
	clog << "Downloaded " + filepath << endl;
      std::FILE *File = std::fopen (filepath.c_str(), "wb");
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, File);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, http_client::get_file);
    }
  curl_easy_setopt (curl, CURLOPT_HEADERDATA, http);
  curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, http_client::get_header_shim);

  CURLcode res = curl_easy_perform (curl);

  if (res != CURLE_OK)
    {
      clog << "curl_easy_perform() failed: " << curl_easy_strerror (res) << endl;
      return false;
    }
  else
      return true;
}


// Get the rpm corresponding to SEARCH_FILE

std::string
http_client::get_rpmname (std::string &search_file)
{
    rpmts ts = NULL;
    Header hdr;
    rpmdbMatchIterator mi;
    rpmtd td;

    td = rpmtdNew ();
    ts = rpmtsCreate ();

    rpmReadConfigFiles (NULL, NULL);

    int metrics[] =
      { RPMTAG_ARCH, RPMTAG_EVR, RPMTAG_FILENAMES, RPMTAG_NAME };

    struct
    {
      string arch;
      string evr;
      string filename;
      string name;
    } rpmhdr;

    mi = rpmtsInitIterator (ts, RPMDBI_PACKAGES, NULL, 0);
    while (NULL != (hdr = rpmdbNextIterator (mi)))
      {
        for (unsigned int i = 0; i < (sizeof (metrics) / sizeof (int)); i++)
          {
            headerGet (hdr, metrics[i], td, HEADERGET_EXT);
            switch (td->type)
              {
              case RPM_STRING_TYPE:
                {
                  const char *rpmval = rpmtdGetString (td);
                  switch (metrics[i])
                    {
                    case RPMTAG_ARCH:
                      rpmhdr.arch = strdup (rpmval);
                      break;
                    case RPMTAG_NAME:
                      rpmhdr.name = strdup (rpmval);
                      break;
                    case RPMTAG_EVR:
                      rpmhdr.evr = strdup (rpmval);
                    }
                  break;
                }
              case RPM_STRING_ARRAY_TYPE:
                {
                  char **strings;
                  strings = (char**)td->data;
                  rpmhdr.filename = "";

                  for (unsigned int idx = 0; idx < td->count; idx++)
                    {
                      if (strcmp (strings[idx], search_file.c_str()) == 0)
                        rpmhdr.filename = strdup (strings[idx]);
                    }
                  free (td->data);
                  break;
                }
              }

            if (metrics[i] == RPMTAG_EVR && rpmhdr.filename.length())
              {
                rpmdbFreeIterator (mi);
                rpmtsFree (ts);
                return rpmhdr.name + "-" + rpmhdr.evr + "." + rpmhdr.arch;
              }

            rpmtdReset (td);
          }
      }

    rpmdbFreeIterator (mi);
    rpmtsFree (ts);

    return search_file;
}


// Put the buildid for DWFLMOD into buildids

int
http_client::process_buildid (Dwfl_Module *dwflmod)
{
  const char *fname;
  dwfl_module_info (dwflmod, NULL, NULL, NULL, NULL, NULL, &fname, NULL);

  GElf_Addr bias;
  int build_id_len = 0;
  unsigned char *build_id_bits;
  GElf_Addr build_id_vaddr;
  string build_id;
  char *result = NULL;
  int code;

  dwfl_module_getelf (dwflmod, &bias);
  build_id_len = dwfl_module_build_id (dwflmod,
      (const unsigned char **)&build_id_bits,
      &build_id_vaddr);

  for (int i = 0; i < build_id_len; i++)
    {
      if (result)
        code = asprintf (&result, "%s%02x", result, *(build_id_bits+i));
      else
        code = asprintf (&result, "%02x", *(build_id_bits+i));
      if (code < 0)
        return 1;
    }

  http->buildids.push_back(make_tuple(fname, result));

  return DWARF_CB_OK;
}


int
http_client::process_buildid_shim (Dwfl_Module *dwflmod,
                 void **userdata __attribute__ ((unused)),
                 const char *name __attribute__ ((unused)),
                 Dwarf_Addr base __attribute__ ((unused)),
                 void *client)
{
  http_client *http = static_cast<http_client *>(client);

  return http->process_buildid (dwflmod);
}


// Do the setup for getting the buildid for FNAME

void
http_client::get_buildid (string fname)
{
  int fd;

  if ((fd = open (fname.c_str(), O_RDONLY)) < 0)
    {
      clog << "can't open " << fname;
      return;
    }

  static const Dwfl_Callbacks callbacks =
    {
      dwfl_build_id_find_elf,
      dwfl_standard_find_debuginfo,
      dwfl_offline_section_address,
      NULL
    };
  Dwfl *dwfl = dwfl_begin (&callbacks);

  if (dwfl == NULL)
    return;

  if (dwfl_report_offline (dwfl, fname.c_str(), fname.c_str(), fd) == NULL)
    return;
  else
    {
      dwfl_report_end (dwfl, NULL, NULL);
      dwfl_getmodules (dwfl, process_buildid_shim, http, 0);
    }
  dwfl_end (dwfl);
  close (fd);
}


void
http_client::get_kernel_buildid (void)
{
  const char *notesfile = "/sys/kernel/notes";
  int fd = open (notesfile, O_RDONLY);
  if (fd < 0)
    return;

  union
  {
      GElf_Nhdr nhdr;
      unsigned char data[8192];
  } buf;

  ssize_t n = read (fd, buf.data, sizeof buf);
  close (fd);

  if (n <= 0)
    return;

  unsigned char *p = buf.data;
  while (p < &buf.data[n])
    {
      /* No translation required since we are reading the native kernel.  */
      GElf_Nhdr *nhdr = (GElf_Nhdr *) p;
      p += sizeof *nhdr;
      unsigned char *name = p;
      p += (nhdr->n_namesz + 3) & -4U;
      unsigned char *bits = p;
      p += (nhdr->n_descsz + 3) & -4U;

      if (p <= &buf.data[n]
          && nhdr->n_type == NT_GNU_BUILD_ID
          && nhdr->n_namesz == sizeof "GNU"
          && !memcmp (name, "GNU", sizeof "GNU"))
        {
          char *result = NULL;
          int code;

          for (unsigned int i = 0; i < nhdr->n_descsz; i++)
            {
              if (result)
                code = asprintf (&result, "%s%02x", result, *(bits+i));
              else
                code = asprintf (&result, "%02x", *(bits+i));
              if (code < 0)
                return;
            }
          http->buildids.push_back(make_tuple("kernel", result));
          break;
        }
    }
}


long
http_client::get_response_code (void)
{
  long response_code = 0;
  curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &response_code);
  return response_code;
}


// Post REQUEST_PARAMETERS, script_files, modules, buildids to URL

bool
http_client::post (const std::string & url,
                   std::vector<std::tuple<std::string, std::string>> &request_parameters)
{
  struct curl_slist *headers=NULL;
  int still_running = false;
  struct curl_httppost *formpost=NULL;
  struct curl_httppost *lastptr=NULL;
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
      string script_base = basename (script_file.c_str());

      curl_formadd (&formpost,
		    &lastptr,
		    CURLFORM_COPYNAME, script_base.c_str(),
		    CURLFORM_FILE, script_file.c_str(),
		    CURLFORM_END);

      curl_formadd (&formpost,
                    &lastptr,
                    CURLFORM_COPYNAME, "files",
                    CURLFORM_COPYCONTENTS, script_file.c_str(),
                    CURLFORM_END);

      // FIXME: There is no guarantee that the first file in
      // script_files is a script - "stap -I foo/ -e '...script...'".
      // 
      // script name is not in cmd_args so add it manually
      if (it == script_files.begin())
        curl_formadd (&formpost,
                      &lastptr,
                      CURLFORM_COPYNAME, "cmd_args",
                      CURLFORM_COPYCONTENTS, script_base.c_str(),
                      CURLFORM_END);
    }

  int bid_idx = 0;

  for (vector<std::string>::const_iterator it = modules.begin ();
      it != modules.end ();
      ++it)
    {
      string module = (*it);
      std::stringstream ss;

      string bid_module = std::get<0>(buildids[bid_idx]);
      string buildid = std::get<1>(buildids[bid_idx]);

      ss  << "{"
          << "\"package\" : \"" << module
          << "\", \"filename\" : \"" << bid_module
          << "\", \"id\" : \"" << buildid
          << "\"}";
      curl_formadd (&formpost,
          &lastptr,
          CURLFORM_COPYNAME, "package info",
          CURLFORM_CONTENTTYPE, "application/json",
          CURLFORM_COPYCONTENTS, ss.str().c_str(),
          CURLFORM_END);
      bid_idx += 1;
    }

  multi_handle = curl_multi_init();

  curl_easy_setopt (curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt (curl, CURLOPT_HTTPPOST, formpost);

  // Mostly for debugging
  if (s.verbose >= 4)
    {
      curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);
      CURL *db_curl = curl_easy_init();
      clog << "BEGIN dump post data" << endl;
      curl_easy_setopt(db_curl, CURLOPT_URL, "http://httpbin.org/post");
      curl_easy_setopt (db_curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt (db_curl, CURLOPT_HTTPPOST, formpost);
      CURLcode res = curl_easy_perform (db_curl);
      if (res != CURLE_OK)
        clog << "curl_easy_perform() failed: " << curl_easy_strerror (res) << endl;
      clog << "END dump post data" << endl;
      curl_easy_cleanup (db_curl);
    }

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


//  Add SCRIPT_FILE having SCRIPT_TYPE to script_files

void
http_client::add_script_file (std::string script_type, std::string script_file)
{
  if (script_type == "tapset")
    script_files.push_back (script_file);
  else
    script_files.insert(script_files.begin(), script_file);
}


// Add MODULE to modules

void
http_client::add_module (std::string module)
{
  modules.push_back (module);
}


http_client_backend::http_client_backend (systemtap_session &s)
  : client_backend(s)
{
  server_tmpdir = s.tmpdir;
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
  http->add_module ("kernel-" + s.kernel_release);
  http->get_kernel_buildid ();

  for (set<std::string>::const_iterator i = s.unwindsym_modules.begin();
      i != s.unwindsym_modules.end();
      ++i)
    {
      string module = (*i);
      if (module != "kernel")
        {
	  string rpmname = http->get_rpmname (module);
	  http->get_buildid (module);
	  http->add_module (rpmname);
	}
    }

  for (vector<std::string>::const_iterator i = s.http_servers.begin ();
      i != s.http_servers.end ();
      ++i)
    {
      // Try to connect to the server.
      if (http->download (*i + "/builds", http->json_type))
        {
	  if (http->post (*i + "/builds", request_parameters))
	    {
	      s.winning_server = *i;
	      http->host = *i;
	      return 0;
	    }
	}
    }

  return 1;
}

int
http_client_backend::unpack_response ()
{
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

  if (s.verbose >= 2)
    clog << "Initial response code: " << http->get_response_code() << endl;
  while (true)
    {
      auto it = http->header_values.find("Retry-After");
      if (it == http->header_values.end())
        {
	  clog << "No retry-after?" << endl;
	  break;
	}
      int retry = std::stoi(http->header_values["Retry-After"], nullptr, 10);
      if (s.verbose >= 2)
	clog << "Waiting " << retry << " seconds" << endl;
      sleep (retry);
      if (http->download (http->host + http->header_values["Location"],
			  http->json_type))
        {
	  // We need to wait until we get a 303 (See Other)
	  long response_code = http->get_response_code();
	  if (s.verbose >= 2)
	    clog << "Response code: " << response_code << endl;
	  if (response_code == 200)
	    continue;
	  else if (response_code == 303)
	    break;
	  else
	    {
	      clog << "Received a unhandled response code "
		   << response_code << endl;
	      return 1;
	    }
	}
    }

  // If we're here, we got a '303' (See Other). Read the "other"
  // location, which should contain our results.
  if (! http->download (http->host + http->header_values["Location"],
			http->json_type))
    {
      clog << "Couldn't read result information" << endl;
      return 1;
    }

  // Get the server version number.
  json_object *ver_obj;
  json_bool jfound = json_object_object_get_ex (http->root, "version",
						&ver_obj);
  if (jfound)
    {
      server_version = json_object_get_string(ver_obj);
    }
  else
    {
      clog << "Couldn't find 'version' in JSON results data" << endl;
      return 1;
    }

  // Get the return code information.
  json_object *rc_obj;
  jfound = json_object_object_get_ex (http->root, "rc", &rc_obj);
  if (jfound)
    {
      int rc = json_object_get_int(rc_obj);
      write_to_file(s.tmpdir + "/rc", rc);
    }
  else
    {
      clog << "Couldn't find 'rc' in JSON results data" << endl;
      return 1;
    }

  // Download each item in the optional 'files' array. This is
  // optional since not all stap invocations produce an output file
  // (like a module).
  json_object *files;
  json_object_object_get_ex (http->root, "files", &files);
  if (files)
    {
      for (int k = 0; k < json_object_array_length (files); k++)
        {
	  json_object *files_element = json_object_array_get_idx (files, k);
	  json_object *loc;
	  jfound = json_object_object_get_ex (files_element, "location", &loc);
	  string location = json_object_get_string (loc);
	  http->download (http->host + location, http->file_type);
	}
    }

  // Output stdout and stderr.
  json_object *loc_obj;
  jfound = json_object_object_get_ex (http->root, "stderr_location", &loc_obj);
  if (jfound)
    {
      string loc_str = json_object_get_string (loc_obj);
      http->download (http->host + loc_str, http->file_type);
    }
  else
    {
      clog << "Couldn't find 'stderr' in JSON results data" << endl;
      return 1;
    }

  jfound = json_object_object_get_ex (http->root, "stdout_location", &loc_obj);
  if (jfound)
    {
      string loc_str = json_object_get_string (loc_obj);
      http->download (http->host + loc_str, http->file_type);
    }
  else
    {
      clog << "Couldn't find 'stdout' in JSON results data" << endl;
      return 1;
    }
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
  request_parameters.push_back(make_tuple("kver", s.kernel_release));
  request_parameters.push_back(make_tuple("arch", s.architecture));

  vector<string> distro_info;

  get_distro_info (distro_info);
  if (! distro_info.empty())
    {
      std::replace(distro_info[0].begin(), distro_info[0].end(), '\n', ' ');
      std::replace(distro_info[1].begin(), distro_info[1].end(), '\n', ' ');
      request_parameters.push_back(make_tuple("distro_name", distro_info[0]));
      request_parameters.push_back(make_tuple("distro_version", distro_info[1]));
    }
  return 0;
}


int
add_tapsets (const char *name, const struct stat *status __attribute__ ((unused)), int type)
{
  if (type == FTW_F)
    http->add_script_file ("tapset", name);

  return 0;
}


int
http_client_backend::include_file_or_directory (const std::string &script_type,
						const std::string &script_file)
{
  // FIXME: this is going to be interesting. We can't add a whole
  // directory at one shot, we'll have to traverse the directory and
  // add each file, preserving the directory structure somehow.
  if (script_type == "tapset")
    ftw (script_file.c_str(), add_tapsets, 1);
  else
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
http_client_backend::add_localization_variable (const std::string &,
					        const std::string &)
{
  // FIXME: We'll probably just add to the request_parameters here.
  return;
}

void
http_client_backend::add_mok_fingerprint (const std::string &)
{
  // FIXME: We'll probably just add to the request_parameters here.
  return;
}

#endif /* HAVE_HTTP_SUPPORT */
