/*
  SSL server program listens on a port, accepts client connection, reads
  the data into a temporary file, calls the systemtap translator and
  then transmits the resulting file back to the client.

  Copyright (C) 2011-2014 Red Hat Inc.

  This file is part of systemtap, and is free software.  You can
  redistribute it and/or modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "config.h"

#include <fstream>
#include <string>
#include <cerrno>
#include <cassert>
#include <climits>
#include <iostream>
#include <map>

extern "C" {
#include <unistd.h>
#include <getopt.h>
#include <wordexp.h>
#include <glob.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <pwd.h>
#include <semaphore.h>

#include <nspr.h>
#include <ssl.h>
#include <nss.h>
#include <keyhi.h>
#include <regex.h>
#include <dirent.h>
#include <string.h>
#include <sys/ioctl.h>

#if HAVE_AVAHI
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>
#include <sys/inotify.h>
#endif
}

#include "util.h"
#include "nsscommon.h"
#include "cscommon.h"
#include "cmdline.h"

using namespace std;

static void cleanup ();
static PRStatus spawn_and_wait (const vector<string> &argv, int *result,
                                const char* fd0, const char* fd1, const char* fd2,
				const char *pwd, const vector<string>& envVec = vector<string> ());

#define MOK_PUBLIC_CERT_NAME "signing_key.x509"
#define MOK_PUBLIC_CERT_FILE "/" MOK_PUBLIC_CERT_NAME
#define MOK_PRIVATE_CERT_NAME "signing_key.priv"
#define MOK_PRIVATE_CERT_FILE "/" MOK_PRIVATE_CERT_NAME
#define MOK_CONFIG_FILE "/x509.genkey"
// MOK_CONFIG_TEXT is the default MOK config text used when creating
// new MOKs. This text is saved to the MOK config file. Once we've
// created it, the server administrator can modify it.
#define MOK_CONFIG_TEXT \
  "[ req ]\n"						\
  "default_bits = 4096\n"				\
  "distinguished_name = req_distinguished_name\n"	\
  "prompt = no\n"					\
  "x509_extensions = myexts\n"				\
  "\n"							\
  "[ req_distinguished_name ]\n"			\
  "O = Systemtap\n"					\
  "CN = Systemtap module signing key\n"			\
  "\n"							\
  "[ myexts ]\n"					\
  "basicConstraints=critical,CA:FALSE\n"		\
  "keyUsage=digitalSignature\n"				\
  "subjectKeyIdentifier=hash\n"				\
  "authorityKeyIdentifier=keyid\n"

/* getopt variables */
extern int optind;

/* File scope statics. Set during argument parsing and initialization. */
static bool use_db_password;
static unsigned short port;
static long max_threads;
static size_t max_uncompressed_req_size;
static size_t max_compressed_req_size;
static string cert_db_path;
static string stap_options;
static string uname_r;
static string kernel_build_tree;
static string arch;
static string cert_serial_number;
static string B_options;
static string I_options;
static string R_option;
static string D_options;
static bool   keep_temp;
static string mok_path;

sem_t sem_client;
static int pending_interrupts;
#define CONCURRENCY_TIMEOUT_S 3

// Message handling.
// Server_error messages are printed to stderr and logged, if requested.
static void
server_error (const string &msg, int logit = true)
{
  cerr << msg << endl << flush;
  // Log it, but avoid repeated messages to the terminal.
  if (logit && log_ok ())
    log (msg);
}

// client_error messages are treated as server errors and also printed to the client's stderr.
static void
client_error (const string &msg, string stapstderr)
{
  server_error (msg);
  if (! stapstderr.empty ())
    {
      ofstream errfile;
      errfile.open (stapstderr.c_str (), ios_base::app);
      if (! errfile.good ())
	server_error (_F("Could not open client stderr file %s: %s", stapstderr.c_str (),
			 strerror (errno)));
      else
	errfile << "Server: " << msg << endl;
      // NB: No need to close errfile
    }
}

// Messages from the nss common code are treated as server errors.
extern "C"
void
nsscommon_error (const char *msg, int logit)
{
  server_error (msg, logit);
}

// Fatal errors are treated as server errors but also result in termination
// of the server.
static void
fatal (const string &msg)
{
  server_error (msg);
  cleanup ();
  exit (1);
}

// Argument handling
static void
process_a (const string &arg)
{
  arch = arg;
  stap_options += " -a " + arg;
}

static void
process_r (const string &arg)
{
  if (arg[0] == '/') // fully specified path
    {
      kernel_build_tree = arg;
      uname_r = kernel_release_from_build_tree (arg);
    }
  else
    {
      kernel_build_tree = "/lib/modules/" + arg + "/build";
      uname_r = arg;
    }
  stap_options += " -r " + arg; // Pass the argument to stap directly.
}

static void
process_log (const char *arg)
{
  start_log (arg);
}

static void
parse_options (int argc, char **argv)
{
  // Examine the command line. This is the command line for us (stap-serverd) not the command
  // line for spawned stap instances.
  optind = 1;
  while (true)
    {
      char *num_endptr;
      long port_tmp;
      long maxsize_tmp;
      // NB: The values of these enumerators must not conflict with the values of ordinary
      // characters, since those are returned by getopt_long for short options.
      enum {
	LONG_OPT_PORT = 256,
	LONG_OPT_SSL,
	LONG_OPT_LOG,
	LONG_OPT_MAXTHREADS,
        LONG_OPT_MAXREQSIZE = 254,
        LONG_OPT_MAXCOMPRESSEDREQ = 255 /* need to set a value otherwise there are conflicts */
      };
      static struct option long_options[] = {
        { "port", 1, NULL, LONG_OPT_PORT },
        { "ssl", 1, NULL, LONG_OPT_SSL },
        { "log", 1, NULL, LONG_OPT_LOG },
        { "max-threads", 1, NULL, LONG_OPT_MAXTHREADS },
        { "max-request-size", 1, NULL, LONG_OPT_MAXREQSIZE},
        { "max-compressed-request", 1, NULL, LONG_OPT_MAXCOMPRESSEDREQ},
        { NULL, 0, NULL, 0 }
      };
      int grc = getopt_long (argc, argv, "a:B:D:I:kPr:R:", long_options, NULL);
      if (grc < 0)
        break;
      switch (grc)
        {
        case 'a':
	  process_a (optarg);
	  break;
	case 'B':
	  B_options += string (" -") + (char)grc + optarg;
	  stap_options += string (" -") + (char)grc + optarg;
	  break;
	case 'D':
	  D_options += string (" -") + (char)grc + optarg;
	  stap_options += string (" -") + (char)grc + optarg;
	  break;
	case 'I':
	  I_options += string (" -") + (char)grc + optarg;
	  stap_options += string (" -") + (char)grc + optarg;
	  break;
	case 'k':
	  keep_temp = true;
	  break;
	case 'P':
	  use_db_password = true;
	  break;
	case 'r':
	  process_r (optarg);
	  break;
	case 'R':
	  R_option = string (" -") + (char)grc + optarg;
	  stap_options += string (" -") + (char)grc + optarg;
	  break;
	case LONG_OPT_PORT:
	  port_tmp =  strtol (optarg, &num_endptr, 10);
	  if (*num_endptr != '\0')
	    fatal (_F("%s: cannot parse number '--port=%s'", argv[0], optarg));
	  else if (port_tmp < 0 || port_tmp > 65535)
	    fatal (_F("%s: invalid entry: port must be between 0 and 65535 '--port=%s'", argv[0],
		      optarg));
	  else
	    port = (unsigned short) port_tmp;
	  break;
	case LONG_OPT_SSL:
	  cert_db_path = optarg;
	  break;
	case LONG_OPT_LOG:
	  process_log (optarg);
	  break;
	case LONG_OPT_MAXTHREADS:
	  max_threads = strtol (optarg, &num_endptr, 0);
	  if (*num_endptr != '\0')
	    fatal (_F("%s: cannot parse number '--max-threads=%s'", argv[0], optarg));
	  else if (max_threads < 0)
	    fatal (_F("%s: invalid entry: max threads must not be negative '--max-threads=%s'",
		      argv[0], optarg));
	  break;
        case LONG_OPT_MAXREQSIZE:
          maxsize_tmp =  strtoul(optarg, &num_endptr, 0); // store as a long for now
	  if (*num_endptr != '\0')
	    fatal (_F("%s: cannot parse number '--max-request-size=%s'", argv[0], optarg));
          else if (maxsize_tmp < 1)
	    fatal (_F("%s: invalid entry: max (uncompressed) request size must be greater than 0 '--max-request-size=%s'",
		      argv[0], optarg));
          max_uncompressed_req_size = (size_t) maxsize_tmp; // convert the long to an unsigned
          break;
        case LONG_OPT_MAXCOMPRESSEDREQ:
          maxsize_tmp =  strtoul(optarg, &num_endptr, 0); // store as a long for now
	  if (*num_endptr != '\0')
	    fatal (_F("%s: cannot parse number '--max-compressed-request=%s'", argv[0], optarg));
          else if (maxsize_tmp < 1)
	    fatal (_F("%s: invalid entry: max compressed request size must be greater than 0 '--max-compressed-request=%s'",
		      argv[0], optarg));
          max_compressed_req_size = (size_t) maxsize_tmp; // convert the long to an unsigned
          break;
	case '?':
	  // Invalid/unrecognized option given. Message has already been issued.
	  break;
        default:
          // Reached when one added a getopt option but not a corresponding switch/case:
          if (optarg)
	    server_error (_F("%s: unhandled option '%c %s'", argv[0], (char)grc, optarg));
          else
	    server_error (_F("%s: unhandled option '%c'", argv[0], (char)grc));
	  break;
        }
    }

  for (int i = optind; i < argc; i++)
    server_error (_F("%s: unrecognized argument '%s'", argv[0], argv[i]));
}

static string
server_cert_file ()
{
  return server_cert_db_path () + "/stap.cert";
}

// Signal handling. When an interrupt is received, kill any spawned processes
// and exit.
extern "C"
void
handle_interrupt (int sig)
{
  pending_interrupts++;
  if(pending_interrupts >= 2)
    {
      log (_F("Received another signal %d, exiting (forced)", sig));
      _exit(0);
    }
  log (_F("Received signal %d, exiting", sig));
}

static void
setup_signals (sighandler_t handler)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigemptyset (&sa.sa_mask);
  if (handler != SIG_IGN)
    {
      sigaddset (&sa.sa_mask, SIGHUP);
      sigaddset (&sa.sa_mask, SIGPIPE);
      sigaddset (&sa.sa_mask, SIGINT);
      sigaddset (&sa.sa_mask, SIGTERM);
      sigaddset (&sa.sa_mask, SIGTTIN);
      sigaddset (&sa.sa_mask, SIGTTOU);
      sigaddset (&sa.sa_mask, SIGXFSZ);
      sigaddset (&sa.sa_mask, SIGXCPU);
    }
  sa.sa_flags = SA_RESTART;

  sigaction (SIGHUP, &sa, NULL);
  sigaction (SIGPIPE, &sa, NULL);
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);
  sigaction (SIGTTIN, &sa, NULL);
  sigaction (SIGTTOU, &sa, NULL);
  sigaction (SIGXFSZ, &sa, NULL);
  sigaction (SIGXCPU, &sa, NULL);
}

// Does the server contain a valid directory for the MOK fingerprint?
bool
mok_dir_valid_p (string mok_fingerprint, bool verbose)
{
  string mok_dir = mok_path + "/" + mok_fingerprint;
  DIR *dirp = opendir (mok_dir.c_str());
  if (dirp == NULL)
    {
      // We can't open the directory. Just quit.
      if (verbose)
	server_error (_F("Could not open server MOK fingerprint directory %s: %s",
		      mok_dir.c_str(), strerror(errno)));
      return false;
    }

  // Find both the x509 certificate and private key files.
  bool priv_found = false;
  bool cert_found = false;
  struct dirent *direntp;
  while ((direntp = readdir (dirp)) != NULL)
    {
      bool reg_file = false;

      if (direntp->d_type == DT_REG)
	reg_file = true;
      else if (direntp->d_type == DT_UNKNOWN)
        {
	  struct stat tmpstat;

	  // If the filesystem doesn't support d_type, we'll have to
	  // call stat().
	  int rc = stat((mok_dir + "/" + direntp->d_name).c_str (), &tmpstat);
	  if (rc == 0 && S_ISREG(tmpstat.st_mode))
	      reg_file = true;
        }
      
      if (! priv_found && reg_file
	  && strcmp (direntp->d_name, MOK_PRIVATE_CERT_NAME) == 0)
        {
	  priv_found = true;
	  continue;
	}
      if (! cert_found && reg_file
	  && strcmp (direntp->d_name, MOK_PUBLIC_CERT_NAME) == 0)
        {
	  cert_found = true;
	  continue;
	}
      if (priv_found && cert_found)
	break;
    }
  closedir (dirp);
  if (! priv_found || ! cert_found)
    {
      // We didn't find one (or both) of the required files. Quit.
      if (verbose)
	server_error (_F("Could not find server MOK files in directory %s",
			 mok_dir.c_str ()));
      return false;
    }

  // Grab info from the cert.
  string fingerprint;
  if (read_cert_info_from_file (mok_dir + MOK_PUBLIC_CERT_FILE, fingerprint)
      == SECSuccess)
    {
      // Make sure the fingerprint from the certificate matches the
      // directory name.
      if (fingerprint != mok_fingerprint)
        {
	  if (verbose)
	      server_error (_F("Server MOK directory name '%s' doesn't match fingerprint from certificate %s",
			       mok_dir.c_str(), fingerprint.c_str()));
	  return false;
	}
    }
  return true;
}

// Get the list of MOK fingerprints on the server. If
// 'only_one_needed' is true, just return the first MOK.
static void
get_server_mok_fingerprints(vector<string> &mok_fingerprints, bool verbose,
			    bool only_one_needed)
{
  DIR *dirp;
  struct dirent *direntp;
  vector<string> temp;

  // Clear the vector.
  mok_fingerprints.clear ();

  // The directory of machine owner keys (MOK) is optional, so if it
  // doesn't exist, we don't worry about it.
  dirp = opendir (mok_path.c_str ());
  if (dirp == NULL)
    {
      // If the error isn't ENOENT (Directory does not exist), we've got
      // a non-fatal error.
      if (errno != ENOENT)
	server_error (_F("Could not open server MOK directory %s: %s",
			 mok_path.c_str (), strerror (errno)));
      return;
    }

  // Create a regular expression object to verify MOK fingerprints
  // directory name.
  regex_t checkre;
  if ((regcomp (&checkre, "^[0-9a-f]{2}(:[0-9a-f]{2})+$",
		REG_EXTENDED | REG_NOSUB) != 0))
    {
      // Not fatal, just ignore the MOK fingerprints.
      server_error (_F("Error in MOK fingerprint regcomp: %s",
		       strerror (errno)));
      closedir (dirp);
      return;
    }

  // We've opened the directory, so read all the directory names from
  // it.
  while ((direntp = readdir (dirp)) != NULL)
    {
      // We're only interested in directories (of key files).
      if (direntp->d_type != DT_DIR)
        {
          if (direntp->d_type == DT_UNKNOWN)
            {
              // If the filesystem doesn't support d_type, we'll have to
              // call stat().
              struct stat tmpstat;
              int rc = stat((mok_path + "/" + direntp->d_name).c_str (), &tmpstat);
              if (rc || !S_ISDIR(tmpstat.st_mode))
                continue;
            }
          else
            continue;
        }

      // We've got a directory. If the directory name isn't in the right
      // format for a MOK fingerprint, skip it.
      if ((regexec (&checkre, direntp->d_name, (size_t) 0, NULL, 0) != 0))
	continue;

      // OK, we've got a directory name in the right format, so save it.
      temp.push_back (string (direntp->d_name));
    }
  regfree (&checkre);
  closedir (dirp);

  // At this point, we've got a list of directories with names in the
  // proper format. Make sure each directory contains a x509
  // certificate and private key file.
  vector<string>::const_iterator it;
  for (it = temp.begin (); it != temp.end (); it++)
    {
      if (mok_dir_valid_p (*it, true))
        {
	  // Save the info.
	  mok_fingerprints.push_back (*it);
	  if (verbose)
	    server_error (_F("Found MOK with fingerprint '%s'", it->c_str ()));
	  if (only_one_needed)
	    break;
	}
    }
  return;
}

#if HAVE_AVAHI
static AvahiEntryGroup *avahi_group = NULL;
static AvahiThreadedPoll *avahi_threaded_poll = NULL;
static char *avahi_service_name = NULL;
static const char * const avahi_service_tag = "_stap._tcp";
static AvahiClient *avahi_client = 0;
static int avahi_collisions = 0;
static int inotify_fd = -1;
static AvahiWatch *avahi_inotify_watch = NULL;

static void create_services (AvahiClient *c);

static int
rename_service ()
{
    /*
     * Each service must have a unique name on the local network.
     * When there is a collision, we try to rename the service.
     * However, we need to limit the number of attempts, since the
     * service namespace could be maliciously flooded with service
     * names designed to maximize collisions.
     * Arbitrarily choose a limit of 65535, which is the number of
     * TCP ports.
     */
    ++avahi_collisions;
    if (avahi_collisions >= 65535) {
      server_error (_F("Too many service name collisions for Avahi service %s",
		       avahi_service_tag));
      return -EBUSY;
    }

    /*
     * Use the avahi-supplied function to generate a new service name.
     */
    char *n = avahi_alternative_service_name(avahi_service_name);

    server_error (_F("Avahi service name collision, renaming service '%s' to '%s'",
		     avahi_service_name, n));
    avahi_free(avahi_service_name);
    avahi_service_name = n;

    return 0;
}

static void
entry_group_callback (
  AvahiEntryGroup *g,
  AvahiEntryGroupState state,
  AVAHI_GCC_UNUSED void *userdata
) {
  assert(g == avahi_group || avahi_group == NULL);
  avahi_group = g;

  // Called whenever the entry group state changes.
  switch (state)
    {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
      // The entry group has been established successfully.
      log (_F("Avahi service '%s' successfully established.", avahi_service_name));
      break;

    case AVAHI_ENTRY_GROUP_COLLISION:
      // A service name collision with a remote service happened.
      // Unfortunately, we don't know which entry collided.
      // We need to rename them all and recreate the services.
      if (rename_service () == 0)
	create_services (avahi_entry_group_get_client (g));
      break;

    case AVAHI_ENTRY_GROUP_FAILURE:
      // Some kind of failure happened.
      server_error (_F("Avahi entry group failure: %s",
		       avahi_strerror (avahi_client_errno (avahi_entry_group_get_client (g)))));
      break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
      break;
    }
}

static void
create_services (AvahiClient *c)
{
  assert (c);

  // Create a new entry group, if necessary, or reset the existing one.
  if (! avahi_group)
    {
      if (! (avahi_group = avahi_entry_group_new (c, entry_group_callback, NULL)))
	{
	  server_error (_F("avahi_entry_group_new () failed: %s",
			   avahi_strerror (avahi_client_errno (c))));
	  return;
	}
    }
  else
    avahi_entry_group_reset(avahi_group);

  // Contruct the information needed for our service.
  log (_F("Adding Avahi service '%s'", avahi_service_name));

  // Create the txt tags that will be registered with our service.
  string sysinfo = "sysinfo=" + uname_r + " " + arch;
  string certinfo = "certinfo=" + cert_serial_number;
  string version = string ("version=") + CURRENT_CS_PROTOCOL_VERSION;;
  string optinfo = "optinfo=";
  string separator;
  // These option strings already have a leading space.
  if (! R_option.empty ())
    {
      optinfo += R_option.substr(1);
      separator = " ";
    }
  if (! B_options.empty ())
    {
      optinfo += separator + B_options.substr(1);
      separator = " ";
    }
  if (! D_options.empty ())
    {
      optinfo += separator + D_options.substr(1);
      separator = " ";
    }
  if (! I_options.empty ())
    optinfo += separator + I_options.substr(1);

  // Create an avahi string list with the info we have so far.
  vector<string> mok_fingerprints;
  AvahiStringList *strlst = avahi_string_list_new(sysinfo.c_str (),
						  optinfo.c_str (),
						  version.c_str (),
						  certinfo.c_str (), NULL);
  if (strlst == NULL)
    {
      server_error (_("Failed to allocate string list"));
      goto fail;
    }

  // Add server MOK info, if available.
  get_server_mok_fingerprints (mok_fingerprints, true, false);
  if (! mok_fingerprints.empty())
    {
      vector<string>::const_iterator it;
      for (it = mok_fingerprints.begin(); it != mok_fingerprints.end(); it++)
        {
	  string tmp = "mok_info=" + *it;
	  strlst = avahi_string_list_add(strlst, tmp.c_str ());
	  if (strlst == NULL)
	    {
	      server_error (_("Failed to add a string to the list"));
	      goto fail;
	    }
	}
    }

  // We will now add our service to the entry group.
  // Loop until no collisions.
  int ret;
  for (;;) {
    ret = avahi_entry_group_add_service_strlst (avahi_group,
						AVAHI_IF_UNSPEC,
						AVAHI_PROTO_UNSPEC,
						(AvahiPublishFlags)0,
						avahi_service_name,
						avahi_service_tag,
						NULL, NULL, port, strlst);
    if (ret == AVAHI_OK)
      break; // success!

    if (ret == AVAHI_ERR_COLLISION)
      {
	// A service name collision with a local service happened.
	// Pick a new name.
	if (rename_service () < 0) {
	  // Too many collisions. Message already issued.
	  goto fail;
	}
	continue; // try again.
      }

      server_error (_F("Failed to add %s service: %s",
		       avahi_service_tag, avahi_strerror (ret)));
      goto fail;
  }

  // Tell the server to register the service.
  if ((ret = avahi_entry_group_commit (avahi_group)) < 0)
    {
      server_error (_F("Failed to commit avahi entry group: %s", avahi_strerror (ret)));
      goto fail;
    }

  avahi_string_list_free(strlst);
  return;

 fail:
  avahi_entry_group_reset (avahi_group);
  avahi_string_list_free(strlst);
}

static void avahi_cleanup_client () {
  // This also frees the entry group, if any
  if (avahi_client) {
    avahi_client_free (avahi_client);
    avahi_client = 0;
    avahi_group = 0;
  }
}
 
static void
client_callback (AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata)
{
  assert(c);

  // Called whenever the client or server state changes.
  switch (state)
    {
    case AVAHI_CLIENT_S_RUNNING:
      // The server has startup successfully and registered its host
      // name on the network, so it's time to create our services.
      create_services (c);
      break;

    case AVAHI_CLIENT_FAILURE:
      server_error (_F("Avahi client failure: %s", avahi_strerror (avahi_client_errno (c))));
      if (avahi_client_errno (c) == AVAHI_ERR_DISCONNECTED)
	{
	  // The client has been disconnected; probably because the avahi daemon has been
	  // restarted. We can free the client here and try to reconnect using a new one.
	  // Passing AVAHI_CLIENT_NO_FAIL allows the new client to be
	  // created, even if the avahi daemon is not running. Our service will be advertised
	  // if/when the daemon is started.
	  avahi_cleanup_client ();
	  int error;
	  avahi_client = avahi_client_new (avahi_threaded_poll_get (avahi_threaded_poll),
					   (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
					   client_callback, NULL, & error);
	}
      break;

    case AVAHI_CLIENT_S_COLLISION:
      // Let's drop our registered services. When the server is back
      // in AVAHI_SERVER_RUNNING state we will register them
      // again with the new host name.
      // Fall through ...
    case AVAHI_CLIENT_S_REGISTERING:
      // The server records are now being established. This
      // might be caused by a host name change. We need to wait
      // for our own records to register until the host name is
      // properly esatblished.
      if (avahi_group)
	avahi_entry_group_reset (avahi_group);
      break;

    case AVAHI_CLIENT_CONNECTING:
      // The avahi-daemon is not currently running. Our service will be advertised
      // if/when the deamon is started.
      server_error (_F("The Avahi daemon is not running. Avahi service '%s' will be established when the deamon is started", avahi_service_name));
      break;
    }
}

static void
inotify_callback (AvahiWatch *, int fd, AvahiWatchEvent, void *)
{
  struct inotify_event in_events[10];
  ssize_t rc;

  // Drain the inotify file. Notice we don't really care what changed,
  // we just needed to know that something changed.
  do
  {
    rc = read (fd, in_events, sizeof (in_events));
  } while (rc > 0);

  // Re-create the services.
  if (avahi_client && (avahi_client_get_state (avahi_client)
		       == AVAHI_CLIENT_S_RUNNING))
    create_services (avahi_client);
}

static void
avahi_cleanup ()
{
  if (avahi_service_name)
    log (_F("Removing Avahi service '%s'", avahi_service_name));

  // Stop the avahi client, if it's running
  if (avahi_threaded_poll)
    avahi_threaded_poll_stop (avahi_threaded_poll);

  // Clean up the avahi objects. The order of freeing these is significant.
  avahi_cleanup_client ();
  if (avahi_inotify_watch)
    {
      const AvahiPoll *poll = avahi_threaded_poll_get (avahi_threaded_poll);
      if (poll)
	poll->watch_free (avahi_inotify_watch);
      avahi_inotify_watch = NULL;
    }
  if (inotify_fd >= 0)
    {
      close (inotify_fd);
      inotify_fd = -1;
    }
  if (avahi_threaded_poll) {
    avahi_threaded_poll_free (avahi_threaded_poll);
    avahi_threaded_poll = 0;
  }
  if (avahi_service_name) {
    avahi_free (avahi_service_name);
    avahi_service_name = 0;
  }
}

// The entry point for the avahi client thread.
static void
avahi_publish_service (CERTCertificate *cert)
{
  // Get the certificate serial number.
  cert_serial_number = get_cert_serial_number (cert);

  // Construct the Avahi service name.
  char host[HOST_NAME_MAX + 1];
  gethostname (host, sizeof(host));
  host[sizeof(host) - 1] = '\0';
  string buf;
  buf = string ("Systemtap Compile Server on ") + host;

  // Make sure the service name is valid
  const char *initial_service_name = buf.c_str ();
  if (! avahi_is_valid_service_name (initial_service_name)) {
    // The only restriction on service names is that the buffer must not exceed
    // AVAHI_LABEL_MAX in size, which means that the name cannot be longer than
    // AVAHI_LABEL_MAX-1 in length.
    assert (strlen (initial_service_name) >= AVAHI_LABEL_MAX);
    buf = buf.substr (0, AVAHI_LABEL_MAX - 1);
    initial_service_name = buf.c_str ();
    assert (avahi_is_valid_service_name (initial_service_name));
  }
  avahi_service_name = avahi_strdup (initial_service_name);

  // Allocate main loop object.
  if (! (avahi_threaded_poll = avahi_threaded_poll_new ()))
    {
      server_error (_("Failed to create avahi threaded poll object."));
      return;
    }

  // Always allocate a new client. Passing AVAHI_CLIENT_NO_FAIL allows the client to be
  // created, even if the avahi daemon is not running. Our service will be advertised
  // if/when the daemon is started.
  int error;
  avahi_client = avahi_client_new (avahi_threaded_poll_get (avahi_threaded_poll),
				   (AvahiClientFlags)AVAHI_CLIENT_NO_FAIL,
				   client_callback, NULL, & error);
  // Check whether creating the client object succeeded.
  if (! avahi_client)
    {
      server_error (_F("Failed to create avahi client: %s", avahi_strerror(error)));
      return;
    }

  // Watch the server MOK directory for any changes.
#if defined(IN_CLOEXEC) && defined(IN_NONBLOCK)
  inotify_fd = inotify_init1 (IN_CLOEXEC|IN_NONBLOCK);
#else
  if ((inotify_fd = inotify_init ()) >= 0)
    {
      fcntl(inotify_fd, F_SETFD, FD_CLOEXEC);
      fcntl(inotify_fd, F_SETFL, O_NONBLOCK);
    }
#endif
  if (inotify_fd < 0)
    server_error (_F("Failed to initialize inotify: %s", strerror (errno)));
  else
    {
      // We want to watch for new or removed MOK directories
      // underneath mok_path. But, to do that, mok_path must exist.
      if (create_dir (mok_path.c_str (), 0755) != 0)
	server_error (_F("Unable to find or create the MOK directory %s: %s",
			 mok_path.c_str (), strerror (errno)));
      // Watch mok_path for changes.
      else if (inotify_add_watch (inotify_fd, mok_path.c_str (),
#ifdef IN_ONLYDIR
			     IN_ONLYDIR|
#endif
			     IN_CLOSE_WRITE|IN_DELETE|IN_DELETE_SELF|IN_MOVE)
	  < 0)
	server_error (_F("Failed to add inotify watch: %s", strerror (errno)));
      else
        {
	  // When mok_path changes, call inotify_callback().
	  const AvahiPoll *poll = avahi_threaded_poll_get (avahi_threaded_poll);
	  if (!poll
	      || ! (avahi_inotify_watch = poll->watch_new (poll, inotify_fd,
							   AVAHI_WATCH_IN,
							   inotify_callback,
							   NULL)))
	    server_error (_("Failed to create inotify watcher"));
	}
    }

  // Run the main loop.
  avahi_threaded_poll_start (avahi_threaded_poll);

  return;
}
#endif // HAVE_AVAHI

static void
advertise_presence (CERTCertificate *cert __attribute ((unused)))
{
#if HAVE_AVAHI
  avahi_publish_service (cert);
#else
  server_error (_("Unable to advertise presence on the network. Avahi is not available"));
#endif
}

static void
unadvertise_presence ()
{
#if HAVE_AVAHI
  avahi_cleanup ();
#endif
}




static void
initialize (int argc, char **argv) {
  pending_interrupts = 0;
  setup_signals (& handle_interrupt);

  // Seed the random number generator. Used to generate noise used during key generation.
  srand (time (NULL));

  // Initial values.
  use_db_password = false;
  port = 0;
  max_threads = sysconf( _SC_NPROCESSORS_ONLN ); // Default to number of processors
  max_uncompressed_req_size = 50000; // 50 KB: default max uncompressed request size
  max_compressed_req_size = 5000; // 5 KB: default max compressed request size
  keep_temp = false;
  struct utsname utsname;
  uname (& utsname);
  uname_r = utsname.release;
  kernel_build_tree = "/lib/modules/" + uname_r + "/build";
  arch = normalize_machine (utsname.machine);

  // Parse the arguments. This also starts the server log, if any, and should be done before
  // any messages are issued.
  parse_options (argc, argv);

  // PR11197: security prophylactics.
  // Reject use as root, except via a special environment variable.
  if (! getenv ("STAP_PR11197_OVERRIDE")) {
    if (geteuid () == 0)
      fatal ("For security reasons, invocation of stap-serverd as root is not supported.");
  }

  struct passwd *pw = getpwuid (geteuid ());
  if (! pw)
    fatal (_F("Unable to determine effective user name: %s", strerror (errno)));
  string username = pw->pw_name;
  pid_t pid = getpid ();
  log (_F("===== compile server pid %d starting as %s =====", pid, username.c_str ()));

  // Where is the ssl certificate/key database?
  if (cert_db_path.empty ())
    cert_db_path = server_cert_db_path ();

  // Make sure NSPR is initialized. Must be done before NSS is initialized
  PR_Init (PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
  /* Set the cert database password callback. */
  PK11_SetPasswordFunc (nssPasswordCallback);

  // Where are the optional machine owner keys (MOK) this server
  // knows about?
  mok_path = server_cert_db_path() + "/moks";
}

static void
cleanup ()
{
  unadvertise_presence ();
  end_log ();
}

/* Function:  readDataFromSocket()
 *
 * Purpose:  Read data from the socket into a temporary file.
 *
 */
static PRInt32
readDataFromSocket(PRFileDesc *sslSocket, const char *requestFileName)
{
  PRFileDesc *local_file_fd = 0;
  PRInt32     numBytesExpected;
  PRInt32     numBytesRead;
  PRInt32     numBytesWritten;
  PRInt32     totalBytes = 0;
#define READ_BUFFER_SIZE 4096
  char        buffer[READ_BUFFER_SIZE];

  // Read the number of bytes to be received.
  numBytesRead = PR_Read_Complete (sslSocket, & numBytesExpected,
				   (PRInt32)sizeof (numBytesExpected));
  if (numBytesRead == 0) /* EOF */
    {
      server_error (_("Error reading size of request file"));
      goto done;
    }
  if (numBytesRead < 0)
    {
      server_error (_("Error in PR_Read"));
      nssError ();
      goto done;
    }

  /* Convert numBytesExpected from network byte order to host byte order.  */
  numBytesExpected = ntohl (numBytesExpected);

  /* If 0 bytes are expected, then we were contacted only to obtain our certificate.
     There is no client request. */
  if (numBytesExpected == 0)
    return 0;

  /* Impose a limit to prevent disk space consumption DoS */
  if (numBytesExpected > (PRInt32) max_compressed_req_size)
    {
      server_error (_("Error size of (compressed) request file is too large"));
      goto done;
    }

  /* Open the output file.  */
  local_file_fd = PR_Open(requestFileName, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
			  PR_IRUSR | PR_IWUSR);
  if (local_file_fd == NULL)
    {
      server_error (_F("Could not open output file %s", requestFileName));
      nssError ();
      return -1;
    }

  // Read until EOF or until the expected number of bytes has been read.
  for (totalBytes = 0; totalBytes < numBytesExpected; totalBytes += numBytesRead)
    {
      // No need for PR_Read_Complete here, since we're already managing multiple
      // reads to a fixed size buffer.
      numBytesRead = PR_Read (sslSocket, buffer, READ_BUFFER_SIZE);
      if (numBytesRead == 0)
	break;	/* EOF */
      if (numBytesRead < 0)
	{
	  server_error (_("Error in PR_Read"));
	  nssError ();
	  goto done;
	}

      /* Write to the request file. */
      numBytesWritten = PR_Write(local_file_fd, buffer, numBytesRead);
      if (numBytesWritten < 0 || (numBytesWritten != numBytesRead))
        {
          server_error (_F("Could not write to output file %s", requestFileName));
	  nssError ();
	  goto done;
        }
    }

  if (totalBytes != numBytesExpected)
    {
      server_error (_F("Expected %d bytes, got %d while reading client request from socket",
			  numBytesExpected, totalBytes));
      goto done;
    }

 done:
  if (local_file_fd)
    PR_Close (local_file_fd);
  return totalBytes;
}

/* Function:  setupSSLSocket()
 *
 * Purpose:  Configure a socket for SSL.
 *
 *
 */
static PRFileDesc * 
setupSSLSocket (PRFileDesc *tcpSocket, CERTCertificate *cert, SECKEYPrivateKey *privKey)
{
  PRFileDesc *sslSocket;
  SSLKEAType  certKEA;
  SECStatus   secStatus;

  /* Inport the socket into SSL.  */
  sslSocket = SSL_ImportFD (NULL, tcpSocket);
  if (sslSocket == NULL)
    {
      server_error (_("Could not import socket into SSL"));
      nssError ();
      return NULL;
    }
   
  /* Set the appropriate flags. */
  secStatus = SSL_OptionSet (sslSocket, SSL_SECURITY, PR_TRUE);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error setting SSL security for socket"));
      nssError ();
      return NULL;
    }

  secStatus = SSL_OptionSet(sslSocket, SSL_HANDSHAKE_AS_SERVER, PR_TRUE);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error setting handshake as server for socket"));
      nssError ();
      return NULL;
    }

  secStatus = SSL_OptionSet(sslSocket, SSL_REQUEST_CERTIFICATE, PR_FALSE);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error setting SSL client authentication mode for socket"));
      nssError ();
      return NULL;
    }

  secStatus = SSL_OptionSet(sslSocket, SSL_REQUIRE_CERTIFICATE, PR_FALSE);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error setting SSL client authentication mode for socket"));
      nssError ();
      return NULL;
    }

  /* Set the appropriate callback routines. */
#if 0 /* use the default */
  secStatus = SSL_AuthCertificateHook (sslSocket, myAuthCertificate, CERT_GetDefaultCertDB());
  if (secStatus != SECSuccess)
    {
      nssError ();
      server_error (_("Error in SSL_AuthCertificateHook"));
      return NULL;
    }
#endif
#if 0 /* Use the default */
  secStatus = SSL_BadCertHook(sslSocket, (SSLBadCertHandler)myBadCertHandler, &certErr);
  if (secStatus != SECSuccess)
    {
      nssError ();
      server_error (_("Error in SSL_BadCertHook"));
      return NULL;
    }
#endif
#if 0 /* no handshake callback */
  secStatus = SSL_HandshakeCallback(sslSocket, myHandshakeCallback, NULL);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error in SSL_HandshakeCallback"));
      nssError ();
      return NULL;
    }
#endif

  certKEA = NSS_FindCertKEAType (cert);

  secStatus = SSL_ConfigSecureServer (sslSocket, cert, privKey, certKEA);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error configuring SSL server"));
      nssError ();
      return NULL;
    }

  return sslSocket;
}

#if 0 /* No client authentication (for now) and not authenticating after each transaction.  */
/* Function:  authenticateSocket()
 *
 * Purpose:  Perform client authentication on the socket.
 *
 */
static SECStatus
authenticateSocket (PRFileDesc *sslSocket, PRBool requireCert)
{
  CERTCertificate *cert;
  SECStatus secStatus;

  /* Returns NULL if client authentication is not enabled or if the
   * client had no certificate. */
  cert = SSL_PeerCertificate(sslSocket);
  if (cert)
    {
      /* Client had a certificate, so authentication is through. */
      CERT_DestroyCertificate(cert);
      return SECSuccess;
    }

  /* Request client to authenticate itself. */
  secStatus = SSL_OptionSet(sslSocket, SSL_REQUEST_CERTIFICATE, PR_TRUE);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error in SSL_OptionSet:SSL_REQUEST_CERTIFICATE"));
      nssError ();
      return SECFailure;
    }

  /* If desired, require client to authenticate itself.  Note
   * SSL_REQUEST_CERTIFICATE must also be on, as above.  */
  secStatus = SSL_OptionSet(sslSocket, SSL_REQUIRE_CERTIFICATE, requireCert);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error in SSL_OptionSet:SSL_REQUIRE_CERTIFICATE"));
      nssError ();
      return SECFailure;
    }

  /* Having changed socket configuration parameters, redo handshake. */
  secStatus = SSL_ReHandshake(sslSocket, PR_TRUE);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error in SSL_ReHandshake"));
      nssError ();
      return SECFailure;
    }

  /* Force the handshake to complete before moving on. */
  secStatus = SSL_ForceHandshake(sslSocket);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error in SSL_ForceHandshake"));
      nssError ();
      return SECFailure;
    }

  return SECSuccess;
}
#endif /* No client authentication and not authenticating after each transaction.  */

/* Function:  writeDataToSocket
 *
 * Purpose:  Write the server's response back to the socket.
 *
 */
static SECStatus
writeDataToSocket(PRFileDesc *sslSocket, const char *responseFileName)
{
  PRFileDesc *local_file_fd = PR_Open (responseFileName, PR_RDONLY, 0);
  if (local_file_fd == NULL)
    {
      server_error (_F("Could not open input file %s", responseFileName));
      nssError ();
      return SECFailure;
    }

  /* Transmit the local file across the socket.
   */
  int numBytes = PR_TransmitFile (sslSocket, local_file_fd, 
				  NULL, 0,
				  PR_TRANSMITFILE_KEEP_OPEN,
				  PR_INTERVAL_NO_TIMEOUT);

  /* Error in transmission. */
  SECStatus secStatus = SECSuccess;
  if (numBytes < 0)
    {
      server_error (_("Error writing response to socket"));
      nssError ();
      secStatus = SECFailure;
    }

  PR_Close (local_file_fd);
  return secStatus;
}

static void
get_stap_locale (const string &staplang, vector<string> &envVec, string stapstderr, cs_protocol_version *client_version)
{
  // If the client version is < 1.6, then no file containing environment
  // variables defining the locale has been passed.
  if (*client_version < "1.6")
    return;

  /* Go through each line of the file, verify it, then add it to the vector */
  ifstream langfile;
  langfile.open(staplang.c_str());
  if (!langfile.is_open())
    {
      // Not fatal. Proceed with the environment we have.
      server_error(_F("Unable to open file %s for reading: %s", staplang.c_str(),
			 strerror (errno)));
      return;
    }

  /* Unpackage internationalization variables and verify their contents */
  map<string, string> envMap; /* To temporarily store the entire array of strings */
  string line;
  const set<string> &locVars = localization_variables();

  /* Copy the global environ variable into the map */
   if(environ != NULL)
     {
      for (unsigned i=0; environ[i]; i++)
        {
          string line = (string)environ[i];

          /* Find the first '=' sign */
          size_t pos = line.find("=");

          /* Make sure it found an '=' sign */
          if(pos != string::npos)
            /* Everything before the '=' sign is the key, and everything after is the value. */ 
            envMap[line.substr(0, pos)] = line.substr(pos+1); 
        }
     }

  /* Create regular expression objects to verify lines read from file. Should not allow
     spaces, ctrl characters, etc */
  regex_t checkre;
  if ((regcomp(&checkre, "^[a-zA-Z0-9@_.=-]*$", REG_EXTENDED | REG_NOSUB) != 0))
    {
      // Not fatal. Proceed with the environment we have.
      server_error(_F("Error in regcomp: %s", strerror (errno)));
      return;
    }

  while (1)
    {
      getline(langfile, line);
      if (!langfile.good())
	break;

      /* Extract key and value from the line. Note: value may contain "=". */
      string key;
      string value;
      size_t pos;
      pos = line.find("=");
      if (pos == string::npos)
        {
          client_error(_F("Localization key=value line '%s' cannot be parsed", line.c_str()), stapstderr);
	  continue;
        }
      key = line.substr(0, pos);
      pos++;
      value = line.substr(pos);

      /* Make sure the key is found in the localization variables global set */
      if (locVars.find(key) == locVars.end())
	{
	  // Not fatal. Just ignore it.
	  client_error(_F("Localization key '%s' not found in global list", key.c_str()), stapstderr);
	  continue;
	}

      /* Make sure the value does not contain illegal characters */
      if ((regexec(&checkre, value.c_str(), (size_t) 0, NULL, 0) != 0))
	{
	  // Not fatal. Just ignore it.
	  client_error(_F("Localization value '%s' contains illegal characters", value.c_str()), stapstderr);
	  continue;
	}

      /* All is good, copy line into envMap, replacing if already there */
      envMap[key] = value;
    }

  if (!langfile.eof())
    {
      // Not fatal. Proceed with what we have.
      server_error(_F("Error reading file %s: %s", staplang.c_str(), strerror (errno)));
    }

  regfree(&checkre);

  /* Copy map into vector */
  for (map<string, string>::iterator it = envMap.begin(); it != envMap.end(); it++)
    envVec.push_back(it->first + "=" + it->second);
}

static void
get_client_mok_fingerprints (const string &filename,
			     vector<string> &mok_fingerprints,
			     string stapstderr,
			     cs_protocol_version *client_version)
{
  // If the client version is < 1.6, then no file containing MOK
  // fingerprints could have been passed.
  if (*client_version < "1.6") {
    return;
  }

  // Go through each line of the file and add it to the vector.
  ifstream file;
  file.open(filename.c_str());
  if (! file.is_open())
    // If the file isn't present, that's fine. It just means that the
    // module doesn't need to be signed.
    return;

  // Create a regular expression object to verify lines read from the
  // file.
  regex_t checkre;
  if (regcomp(&checkre, "^([0-9a-f]{2}(:[0-9a-f]{2})+)$", REG_EXTENDED)
      != 0)
    {
      // Not fatal, just ignore the MOK fingerprints.
      server_error(_F("Error in MOK fingerprint regcomp: %s",
		      strerror (errno)));
      return;
    }

  // Unpack the MOK fingerprints. Notice we make sure the fingerprint
  // is in the right format, but that's all we can do at this
  // point. Later we'll check this client list against our server
  // list.
  string line;
  regmatch_t matches[3];
  while (getline (file, line))
    {
      string fingerprint;

      if ((regexec(&checkre, line.c_str(), 3, matches, 0) != 0))
        {
	  // Not fatal. Just ignore it.
	  client_error(_F("MOK fingerprint value '%s' isn't in the correct forma",
			  line.c_str()), stapstderr);
	  continue;
	}

      // Save the fingerprint:
      //   matches[0] is the range of the entire match
      //   matches[1] is the entire fingerprint
      //   matches[2] is a portion of the fingerprint
      if (matches[1].rm_so >= 0)
	fingerprint = line.substr(matches[1].rm_so,
				  matches[1].rm_eo - matches[1].rm_so);
      if (! fingerprint.empty())
	mok_fingerprints.push_back(fingerprint);
    }
  regfree(&checkre);
}

bool
mok_sign_file (std::string &mok_fingerprint,
	       const std::string &kernel_build_tree,
	       const std::string &name,
	       std::string stapstderr)
{
  vector<string> cmd;
  int rc;
  string mok_directory = mok_path + "/" + mok_fingerprint;

  cmd.clear();
  cmd.push_back (kernel_build_tree + "/scripts/sign-file");
  cmd.push_back ("sha512");
  cmd.push_back (mok_directory + MOK_PRIVATE_CERT_FILE);
  cmd.push_back (mok_directory + MOK_PUBLIC_CERT_FILE);
  cmd.push_back (name);

  rc = stap_system (0, cmd);
  if (rc != 0) 
    {
      client_error (_F("Running sign-file failed, rc = %d", rc), stapstderr);
      return false;
    }
  else
    {
      client_error (_F("Module signed with MOK, fingerprint \"%s\"",
		       mok_fingerprint.c_str()), stapstderr);
      return true;
    }
}

// Filter paths prefixed with the server's home directory from the given file.
//
static void
filter_response_file (const string &file_name, const string &responseDirName)
{
  vector<string> cmd;

  // Filter the server's home directory name
  cmd.clear();
  cmd.push_back ("sed");
  cmd.push_back ("-i");
  cmd.push_back (string ("s,") + get_home_directory () + ",<server>,g");
  cmd.push_back (file_name);
  (void) stap_system (0, cmd);

  // Filter the server's response directory name
  cmd.clear();
  cmd.push_back ("sed");
  cmd.push_back ("-i");
  cmd.push_back (string ("s,") + responseDirName + ",<server>,g");
  cmd.push_back (file_name);
  (void) stap_system (0, cmd);
}

static privilege_t
getRequestedPrivilege (const vector<string> &stapargv)
{
  // The purpose of this function is to find the --privilege or --unprivileged option specified
  // by the user on the client side. We need to parse the command line completely, but we can
  // exit when we find the first --privilege or --unprivileged option, since stap does not allow
  // multiple privilege levels to specified on the same command line.
  //
  // Note that we need not do any options consistency checking since our spawned stap instance
  // will do that.
  //
  // Create an argv/argc for use by getopt_long.
  int argc = stapargv.size();
  char ** argv = new char *[argc + 1];
  for (unsigned i = 0; i < stapargv.size(); ++i)
    argv[i] = (char *)stapargv[i].c_str();
  argv[argc] = NULL;

  privilege_t privilege = pr_highest; // Until specified otherwise.
  optind = 1;
  while (true)
    {
      // We need only allow getopt to parse the options until we find a
      // --privilege or --unprivileged option.
      int grc = getopt_long (argc, argv, STAP_SHORT_OPTIONS, stap_long_options, NULL);
      if (grc < 0)
        break;
      switch (grc)
        {
	default:
	  // We can ignore all options other than --privilege and --unprivileged.
	  break;
	case LONG_OPT_PRIVILEGE:
	  if (strcmp (optarg, "stapdev") == 0)
	    privilege = pr_stapdev;
	  else if (strcmp (optarg, "stapsys") == 0)
	    privilege = pr_stapsys;
	  else if (strcmp (optarg, "stapusr") == 0)
	    privilege = pr_stapusr;
	  else
	    {
	      server_error (_F("Invalid argument '%s' for --privilege", optarg));
	      privilege = pr_highest;
	    }
	  // We have discovered the client side --privilege option. We can exit now since
	  // stap only tolerates one privilege setting option.
	  goto done; // break 2 switches and a loop
	case LONG_OPT_UNPRIVILEGED:
	  privilege = pr_unprivileged;
	  // We have discovered the client side --unprivileged option. We can exit now since
	  // stap only tolerates one privilege setting option.
	  goto done; // break 2 switches and a loop
	}
    }
 done:
  delete[] argv;
  return privilege;
}

static void
generate_mok(string &mok_fingerprint)
{
  vector<string> cmd;
  int rc;
  char tmpdir[PATH_MAX] = { '\0' };
  string public_cert_path, private_cert_path, destdir;
  mode_t old_umask;

  mok_fingerprint.clear ();

  // Set umask so that everything is private.
  old_umask = umask(077);

  // Make sure the config file exists. If not, create it with default
  // contents.
  string config_path = mok_path + MOK_CONFIG_FILE;
  if (! file_exists (config_path))
    {
      ofstream config_stream;
      config_stream.open (config_path.c_str ());
      if (! config_stream.good ())
        {
	  server_error (_F("Could not open MOK config file %s: %s",
			   config_path.c_str (), strerror (errno)));
	  goto cleanup;
	}
      config_stream << MOK_CONFIG_TEXT;
      config_stream.close ();
    }

  // Make a temporary directory to store results in.
  snprintf (tmpdir, PATH_MAX, "%s/stap-server.XXXXXX", mok_path.c_str ());
  if (mkdtemp (tmpdir) == NULL)
    {
      server_error (_F("Could not create temporary directory %s: %s", tmpdir, 
		       strerror (errno)));
      tmpdir[0] = '\0';
      goto cleanup;
    }

  // Actually generate key using openssl.
  public_cert_path = tmpdir + string (MOK_PUBLIC_CERT_FILE);
  private_cert_path = tmpdir + string (MOK_PRIVATE_CERT_FILE);
  cmd.push_back ("openssl");
  cmd.push_back ("req");
  cmd.push_back ("-new");
  cmd.push_back ("-nodes");
  cmd.push_back ("-utf8");
  cmd.push_back ("-sha256");
  cmd.push_back ("-days");
  cmd.push_back ("36500");
  cmd.push_back ("-batch");
  cmd.push_back ("-x509");
  cmd.push_back ("-config");
  cmd.push_back (config_path);
  cmd.push_back ("-outform");
  cmd.push_back ("DER");
  cmd.push_back ("-out");
  cmd.push_back (public_cert_path);
  cmd.push_back ("-keyout");
  cmd.push_back (private_cert_path);
  rc = stap_system (0, cmd);
  if (rc != 0) 
    {
      server_error (_F("Generating MOK failed, rc = %d", rc));
      goto cleanup;
    }

  // Grab the fingerprint from the cert.
  if (read_cert_info_from_file (public_cert_path, mok_fingerprint)
      != SECSuccess)
    goto cleanup;

  // Once we know the fingerprint, rename the temporary directory.
  destdir = mok_path + "/" + mok_fingerprint;
  if (rename (tmpdir, destdir.c_str ()) < 0)
    {
      server_error (_F("Could not rename temporary directory %s to %s: %s",
		       tmpdir, destdir.c_str (), strerror (errno)));
      goto cleanup;
    }

  // Restore the old umask.
  umask(old_umask);

  return;

cleanup:
  // Remove the temporary directory.
  cmd.clear ();
  cmd.push_back ("rm");
  cmd.push_back ("-rf");
  cmd.push_back (tmpdir);
  rc = stap_system (0, cmd);
  if (rc != 0)
    server_error (_("Error in tmpdir cleanup"));
  mok_fingerprint.clear ();

  // Restore the old umask.
  umask(old_umask);
  return;
}

/* Run the translator on the data in the request directory, and produce output
   in the given output directory. */
static void
handleRequest (const string &requestDirName, const string &responseDirName, string stapstderr)
{
  vector<string> stapargv;
  cs_protocol_version client_version = "1.0"; // Assumed until discovered otherwise
  int rc;
  wordexp_t words;
  unsigned u;
  unsigned i;
  FILE* f;

  // Save the server version. Do this early, so the client knows what version of the server
  // it is dealing with, even if the request is not fully completed.
  string stapversion = responseDirName + "/version";
  f = fopen (stapversion.c_str (), "w");
  if (f) 
    {
      fputs (CURRENT_CS_PROTOCOL_VERSION, f);
      fclose(f);
    }
  else
    server_error (_F("Unable to open client version file %s", stapversion.c_str ()));

  // Get the client version. The default version is already set. Use it if we fail here.
  string filename = requestDirName + "/version";
  if (file_exists (filename))
    read_from_file (filename, client_version);
  log (_F("Client version is %s", client_version.v));

  // The name of the translator executable.
  stapargv.push_back ((char *)(getenv ("SYSTEMTAP_STAP") ?: STAP_PREFIX "/bin/stap"));

  /* Transcribe stap_options.  We use plain wordexp(3), since these
     options are coming from the local trusted user, so malicious
     content is not a concern. */
  // TODO: Use tokenize here.
  rc = wordexp (stap_options.c_str (), & words, WRDE_NOCMD|WRDE_UNDEF);
  if (rc)
    {
      server_error (_("Cannot parse stap options"));
      return;
    }

  for (u=0; u<words.we_wordc; u++)
    stapargv.push_back (words.we_wordv[u]);

  /* Process the saved command line arguments.  Avoid quoting/unquoting errors by
     transcribing literally. */
  string new_staptmpdir = responseDirName + "/stap000000";
  rc = mkdir(new_staptmpdir.c_str(), 0700);
  if (rc)
    server_error(_F("Could not create temporary directory %s", new_staptmpdir.c_str()));

  stapargv.push_back("--tmpdir=" + new_staptmpdir);

  stapargv.push_back ("--client-options");

  string stap_opts = "";
  for (i=1 ; ; i++)
    {
      char stapargfile[PATH_MAX];
      FILE* argfile;
      struct stat st;
      char *arg;

      snprintf (stapargfile, PATH_MAX, "%s/argv%d", requestDirName.c_str (), i);

      rc = stat(stapargfile, & st);
      if (rc) break;

      arg = (char *)malloc (st.st_size+1);
      if (!arg)
        {
          server_error (_("Out of memory"));
          return;
        }

      argfile = fopen(stapargfile, "r");
      if (! argfile)
        {
          free(arg);
          server_error (_F("Error opening %s: %s", stapargfile, strerror (errno)));
          return;
        }

      rc = fread(arg, 1, st.st_size, argfile);
      if (rc != st.st_size)
        {
          free(arg);
          fclose(argfile);
          server_error (_F("Error reading %s: %s", stapargfile, strerror (errno)));
          return;
        }

      arg[st.st_size] = '\0';
      stapargv.push_back (arg);
      stap_opts.append(arg);
      stap_opts.append(" ");
      free (arg);
      fclose (argfile);
    }
  log(_F("Options passed from the client: %s", stap_opts.c_str()));

  string stapstdout = responseDirName + "/stdout";

  // NB: Before, when we did not fully parse the client's command line using getopt_long,
  // we used to insert a --privilege=XXX option here in case some other argument was mistaken
  // for a --privilege or --unprivileged option by our spawned stap. Since we now parse
  // the client's command line using getopt_long and share the getopt_long options
  // string and table with stap, this is no longer necessary. stap will parse the
  // command line identically to the way we have parsed it and will discover the same
  // privilege-setting option.

  // Environment variables (possibly empty) to be passed to spawn_and_wait().
  string staplang = requestDirName + "/locale";
  vector<string> envVec;
  get_stap_locale (staplang, envVec, stapstderr, &client_version);

  // Machine owner keys (MOK) fingerprints (possibly nonexistent), to
  // be used as a list of valid keys that the module must be signed with.
  vector<string> client_mok_fingerprints;
  get_client_mok_fingerprints(requestDirName + "/mok_fingerprints",
			      client_mok_fingerprints, stapstderr,
			      &client_version);


  // If the client sent us MOK fingerprints, see if we have a matching
  // MOK on the server.
  string mok_fingerprint;
  if (! client_mok_fingerprints.empty())
    {
      // See if any of the client MOK fingerprints exist on the server.
      vector<string>::const_iterator it;
      for (it = client_mok_fingerprints.begin();
	   it != client_mok_fingerprints.end(); it++)
        {
	  if (mok_dir_valid_p (*it, false))
	    {
	      mok_fingerprint = *it;
	      break;
	    }
	}

      // If the client requires signing, but we couldn't find a
      // matching machine owner key installed on the server, we can't
      // build a signed module. But, the client may not have asked us
      // to create a module (for instance, the user could have done
      // 'stap -L syscall.open'). So, keep going until we know we need
      // to sign a module.
  }

  /* All ready, let's run the translator! */
  int staprc;
  rc = spawn_and_wait(stapargv, &staprc, "/dev/null", stapstdout.c_str (),
                      stapstderr.c_str (), requestDirName.c_str (), envVec);
  if (rc != PR_SUCCESS)
    {
      server_error(_("Failed spawning translator"));
      return;
    }

  // In unprivileged modes, if we have a module built, we need to sign
  // the sucker.  We also might need to sign the module for secure
  // boot purposes.
  privilege_t privilege = getRequestedPrivilege (stapargv);
  if (staprc == 0 && (pr_contains (privilege, pr_stapusr)
		      || pr_contains (privilege, pr_stapsys)
		      || ! client_mok_fingerprints.empty ()))
    {
      glob_t globber;
      char pattern[PATH_MAX];
      snprintf (pattern, PATH_MAX, "%s/*.ko", new_staptmpdir.c_str());
      rc = glob (pattern, GLOB_ERR, NULL, &globber);
      if (rc)
        server_error (_F("Unable to find a module in %s", new_staptmpdir.c_str()));
      else if (globber.gl_pathc != 1)
        server_error (_F("Too many modules (%zu) in %s", globber.gl_pathc, new_staptmpdir.c_str()));
      else
        {
	  if (pr_contains (privilege, pr_stapusr)
	      || pr_contains (privilege, pr_stapsys))
	    sign_file (cert_db_path, server_cert_nickname(),
		       globber.gl_pathv[0],
		       string(globber.gl_pathv[0]) + ".sgn");
	  if (! mok_fingerprint.empty ())
	    {
	      // If we signing the module failed, change the staprc to
	      // 1, so that the client won't try to run the resulting
	      // module, which wouldn't work.
	      if (! mok_sign_file (mok_fingerprint, kernel_build_tree,
				   globber.gl_pathv[0], stapstderr))
		staprc = 1;
	    }
	  else if (! client_mok_fingerprints.empty ())
	    {
	      // If we're here, the client sent us MOK fingerprints
	      // (since client_mok_fingerprints isn't empty), but we
	      // don't have a matching MOK on the server (since
	      // mok_fingerprint is empty). So, we can't sign the
	      // module.
	      client_error (_("No matching machine owner key (MOK) available on the server to sign the\n module."), stapstderr);

	      // Since we can't sign the module, send the client one
	      // of our MOKs. If we don't have any, create one.
	      vector<string> mok_fingerprints;
	      get_server_mok_fingerprints(mok_fingerprints, false, true);
	      if (mok_fingerprints.empty ())
	        {
		  // Generate a new MOK.
		  generate_mok(mok_fingerprint);
		}
	      else
	        {
		  // At this point we have at least one MOK on the
		  // server. Send the public key down to the
		  // client.
		  mok_fingerprint = *mok_fingerprints.begin ();
		}
	      
	      if (! mok_fingerprint.empty ())
	        {
		  // Copy the public cert file to the response directory.
		  string mok_directory = mok_path + "/" + mok_fingerprint;
		  string src = mok_directory + MOK_PUBLIC_CERT_FILE;
		  string dst = responseDirName + MOK_PUBLIC_CERT_FILE;
		  if (copy_file (src, dst, true))
		    client_error ("The server has no machine owner key (MOK) in common with this\nsystem. Use the following command to import a server MOK into this\nsystem, then reboot:\n\n\tmokutil --import signing_key.x509", stapstderr);
		  else
		    client_error ("The server has no machine owner key (MOK) in common with this\nsystem. The server failed to return a certificate.", stapstderr);
		}
	      else
	        {
		  client_error ("The server has no machine owner keys (MOK) in common with this\nsystem. The server could not generate a new MOK.", stapstderr);
		}

	      // If we couldn't sign the module, let's change the
	      // staprc to 1, so that the client won't try to run the
	      // resulting module, which wouldn't work.
	      staprc = 1;
	  }
        }
    }

  // Save the RC (which might have gotten changed above).
  ofstream ofs((responseDirName + "/rc").c_str());
  ofs << staprc;
  ofs.close();

  /* If uprobes.ko is required, it will have been built or cache-copied into
   * the temp directory.  We need to pack it into the response where the client
   * can find it, and sign, if necessary, for unprivileged users.
   */
  string uprobes_ko = new_staptmpdir + "/uprobes/uprobes.ko";
  if (get_file_size(uprobes_ko) > 0)
    {
      /* uprobes.ko is required.
       *
       * It's already underneath the stap tmpdir, but older stap clients
       * don't know to look for it there, so, for these clients, we end up packing uprobes twice
       * into the zip.  We could move instead of symlink.
       */
      string uprobes_response;
      if (client_version < "1.6")
	{
	  uprobes_response = (string)responseDirName + "/uprobes.ko";
	  rc = symlink(uprobes_ko.c_str(), uprobes_response.c_str());
	  if (rc != 0)
	    server_error (_F("Could not link to %s from %s",
			     uprobes_ko.c_str(), uprobes_response.c_str()));
	}
      else
	uprobes_response = uprobes_ko;

      /* In unprivileged mode, we need a signature on uprobes as well. */
      if (! pr_contains (privilege, pr_stapdev))
        {
          sign_file (cert_db_path, server_cert_nickname(),
                     uprobes_response, uprobes_response + ".sgn");
        }

      // Notice we're not giving an error message here if the client
      // requires signed modules. The error will have been generated
      // above on the systemtap module itself.
      if (! mok_fingerprint.empty ())
	mok_sign_file (mok_fingerprint, kernel_build_tree, uprobes_response,
		       stapstderr);
    }

  /* Free up all the arg string copies.  Note that the first few were alloc'd
     by wordexp(), which wordfree() frees; others were hand-set to literal strings. */
  wordfree (& words);

  // Filter paths prefixed with the server's home directory from the stdout and stderr
  // files in the response.
  filter_response_file (stapstdout, responseDirName);
  filter_response_file (stapstderr, responseDirName);

  /* Sorry about the inconvenience.  C string/file processing is such a pleasure. */
}


/* A front end for stap_spawn that handles stdin, stdout, stderr, switches to a working
   directory and returns overall success or failure. */
static PRStatus
spawn_and_wait (const vector<string> &argv, int *spawnrc,
		const char* fd0, const char* fd1, const char* fd2,
		const char *pwd, const vector<string>& envVec)
{ 
  pid_t pid;
  int rc;
  posix_spawn_file_actions_t actions;
  int dotfd = -1;

#define CHECKRC(msg) do { if (rc) { server_error (_(msg)); return PR_FAILURE; } } while (0)

  rc = posix_spawn_file_actions_init (& actions);
  CHECKRC ("Error in spawn file actions ctor");
  if (fd0) {
    rc = posix_spawn_file_actions_addopen(& actions, 0, fd0, O_RDONLY, 0600);
    CHECKRC ("Error in spawn file actions fd0");
  }
  if (fd1) {
    rc = posix_spawn_file_actions_addopen(& actions, 1, fd1, O_WRONLY|O_CREAT, 0600);
    CHECKRC ("Error in spawn file actions fd1");
  }
  if (fd2) { 
    // Use append mode for stderr because it gets written to in other places in the server.
    rc = posix_spawn_file_actions_addopen(& actions, 2, fd2, O_WRONLY|O_APPEND|O_CREAT, 0600);
    CHECKRC ("Error in spawn file actions fd2");
  }

  /* change temporarily to a directory if requested */
  if (pwd)
    {
      dotfd = open (".", O_RDONLY);
      if (dotfd < 0)
        {
          server_error (_("Error in spawn getcwd"));
          return PR_FAILURE;
        }

      rc = chdir (pwd);
      if (rc)
        {
          close(dotfd);
          server_error(_("Error in spawn chdir"));
          return PR_FAILURE;
        }
    }

  pid = stap_spawn (0, argv, & actions, envVec);
  /* NB: don't react to pid==-1 right away; need to chdir back first. */

  if (pwd && dotfd >= 0)
    {
      int subrc;
      subrc = fchdir (dotfd);
      subrc |= close (dotfd);
      if (subrc) 
        server_error (_("Error in spawn unchdir"));
    }

  if (pid == -1)
    {
      server_error (_F("Error in spawn: %s", strerror (errno)));
      return PR_FAILURE;
    }

  *spawnrc = stap_waitpid (0, pid);
  if (*spawnrc == -1) // something wrong with waitpid() call itself
    {
      server_error (_("Error in waitpid"));
      return PR_FAILURE;
    }

  rc = posix_spawn_file_actions_destroy (&actions);
  CHECKRC ("Error in spawn file actions dtor");

  return PR_SUCCESS;
#undef CHECKRC
}

/* Given the path to the compressed request file, return 0 if the size of the
 * uncompressed request is within the determined limit. */
int
check_uncompressed_request_size (const char * zip_file)
{
  vector<string> args;
  ostringstream result;

  // Generate the command to heck the uncompressed size
  args.push_back("unzip");
  args.push_back("-Zt");
  args.push_back(zip_file);

  int rc = stap_system_read (0, args, result);
  if (rc != 0)
    {
    server_error (_F("Unable to check the zipefile size. Error code: %d .", rc));
    return rc;
    }

  // Parse the result from the unzip call, looking for the third token
  vector<string> toks;
  tokenize(result.str(), toks, " ");
  if (toks.size() < 3)
    {
      // Something went wrong and the format is probably not what we expect.
      server_error("Unable to check the uncompressed zipfile size. Output came in an unexpected format.");
      return -1;
    }

  long uncomp_size = atol(toks[2].c_str());
  if (uncomp_size < 1 || (unsigned)uncomp_size > max_uncompressed_req_size)
    {
      server_error(_F("Uncompressed request size of %ld bytes is not within the expected range of 1 to %zu bytes.",
                      uncomp_size,max_uncompressed_req_size));
      return -1;
    }

  return 0; // If it got to this point, everthing went well.
}

/* Function:  void *handle_connection()
 *
 * Purpose: Handle a connection to a socket.  Copy in request zip
 * file, process it, copy out response.  Temporary directories are
 * created & destroyed here.
 */

void *
handle_connection (void *arg)
{
  PRFileDesc *       sslSocket = NULL;
  SECStatus          secStatus = SECFailure;
  PRStatus           prStatus;
  int                rc;
  char              *rc1;
  char               tmpdir[PATH_MAX];
  char               requestFileName[PATH_MAX];
  char               requestDirName[PATH_MAX];
  char               responseDirName[PATH_MAX];
  char               responseFileName[PATH_MAX];
  string stapstderr; /* Cannot be global since we need a unique
                        copy for each connection.*/
  vector<string>     argv;
  PRInt32            bytesRead;

  /* Detatch to avoid a memory leak */
  if(max_threads > 0)
    pthread_detach(pthread_self());

  /* Unpack the arg */
  thread_arg *t_arg = (thread_arg *) arg;
  PRFileDesc *tcpSocket = t_arg->tcpSocket;
  CERTCertificate *cert = t_arg->cert;
  SECKEYPrivateKey *privKey = t_arg->privKey;
  PRNetAddr addr = t_arg->addr;

  tmpdir[0]='\0'; /* prevent cleanup-time /bin/rm of uninitialized directory */

#if 0 // already done on the listenSocket
  /* Make sure the socket is blocking. */
  PRSocketOptionData socketOption;
  socketOption.option             = PR_SockOpt_Nonblocking;
  socketOption.value.non_blocking = PR_FALSE;
  PR_SetSocketOption (tcpSocket, &socketOption);
#endif
  secStatus = SECFailure;
  sslSocket = setupSSLSocket (tcpSocket, cert, privKey);
  if (sslSocket == NULL)
    {
      // Message already issued.
      goto cleanup;
    }

  secStatus = SSL_ResetHandshake(sslSocket, /* asServer */ PR_TRUE);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error resetting SSL handshake"));
      nssError ();
      goto cleanup;
    }

#if 0 // The client authenticates the server, so the client initiates the handshake
  /* Force the handshake to complete before moving on. */
  secStatus = SSL_ForceHandshake(sslSocket);
  if (secStatus != SECSuccess)
    {
      server_error (_("Error forcing SSL handshake"));
      nssError ();
      goto cleanup;
    }
#endif

  secStatus = SECFailure;
  snprintf(tmpdir, PATH_MAX, "%s/stap-server.XXXXXX", getenv("TMPDIR") ?: "/tmp");
  rc1 = mkdtemp(tmpdir);
  if (! rc1)
    {
      server_error (_F("Could not create temporary directory %s: %s", tmpdir, strerror(errno)));
      tmpdir[0]=0; /* prevent /bin/rm */
      goto cleanup;
    }

  /* Create a temporary files names and directories.  */
  snprintf (requestFileName, PATH_MAX, "%s/request.zip", tmpdir);

  snprintf (requestDirName, PATH_MAX, "%s/request", tmpdir);
  rc = mkdir(requestDirName, 0700);
  if (rc)
    {
      server_error (_F("Could not create temporary directory %s: %s", requestDirName, strerror (errno)));
      goto cleanup;
    }

  snprintf (responseDirName, PATH_MAX, "%s/response", tmpdir);
  rc = mkdir(responseDirName, 0700);
  if (rc)
    {
      server_error (_F("Could not create temporary directory %s: %s", responseDirName, strerror (errno)));
      goto cleanup;
    }
  // Set this early, since it gets used for errors to be returned to the client.
  stapstderr = string(responseDirName) + "/stderr";

  snprintf (responseFileName, PATH_MAX, "%s/response.zip", tmpdir);

  /* Read data from the socket.
   * If the user is requesting/requiring authentication, authenticate
   * the socket.  */
  bytesRead = readDataFromSocket(sslSocket, requestFileName);
  if (bytesRead < 0) // Error
    goto cleanup;
  if (bytesRead == 0) // No request -- not an error
    {
      secStatus = SECSuccess;
      goto cleanup;
    }

#if 0 /* Don't authenticate after each transaction */
  if (REQUEST_CERT_ALL)
    {
      secStatus = authenticateSocket(sslSocket);
      if (secStatus != SECSuccess)
	goto cleanup;
    }
#endif

  /* Just before we do any kind of processing, we want to check that the request there will
   * be enough memory to unzip the file. */
  if (check_uncompressed_request_size(requestFileName))
    {
      goto cleanup;
    }

  /* Unzip the request. */
  secStatus = SECFailure;
  argv.push_back ("unzip");
  argv.push_back ("-q");
  argv.push_back ("-d");
  argv.push_back (requestDirName);
  argv.push_back (requestFileName);
  rc = stap_system (0, argv);
  if (rc != 0)
    {
      server_error (_("Unable to extract client request"));
      goto cleanup;
    }

  /* Handle the request zip file.  An error therein should still result
     in a response zip file (containing stderr etc.) so we don't have to
     have a result code here.  */
  handleRequest(requestDirName, responseDirName, stapstderr);

  /* Zip the response. */
  int ziprc;
  argv.clear ();
  argv.push_back ("zip");
  argv.push_back ("-q");
  argv.push_back ("-r");
  argv.push_back (responseFileName);
  argv.push_back (".");
  rc = spawn_and_wait (argv, &ziprc, NULL, NULL, NULL, responseDirName);
  if (rc != PR_SUCCESS || ziprc != 0)
    {
      server_error (_("Unable to compress server response"));
      goto cleanup;
    }

  secStatus = writeDataToSocket (sslSocket, responseFileName);

cleanup:
  if (sslSocket)
    if (PR_Close (sslSocket) != PR_SUCCESS)
      {
	server_error (_("Error closing ssl socket"));
	nssError ();
      }

  if (tmpdir[0]) 
    {
      // Remove the whole tmpdir and all that lies beneath, unless -k was specified.
      if (keep_temp) 
	log (_F("Keeping temporary directory %s", tmpdir));
      else
	{
	  argv.clear ();
	  argv.push_back ("rm");
	  argv.push_back ("-r");
	  argv.push_back (tmpdir);
	  rc = stap_system (0, argv);
	  if (rc != 0)
	    server_error (_("Error in tmpdir cleanup"));
	}
    }

  if (secStatus != SECSuccess)
    server_error (_("Error processing client request"));

  // Log the end of the request.
  char buf[1024];
  prStatus = PR_NetAddrToString (& addr, buf, sizeof (buf));
  if (prStatus == PR_SUCCESS)
    {
      if (addr.raw.family == PR_AF_INET)
	log (_F("Request from %s:%d complete", buf, addr.inet.port));
      else if (addr.raw.family == PR_AF_INET6)
	log (_F("Request from [%s]:%d complete", buf, addr.ipv6.port));
    }

  /* Increment semephore to indicate this thread is finished. */
  free(t_arg);
  if (max_threads > 0)
    {
      sem_post(&sem_client);
      pthread_exit(0);
    }
  else
    return 0;
}

/* Function:  int accept_connection()
 *
 * Purpose:  Accept a connection to the socket.
 *
 */
static SECStatus
accept_connections (PRFileDesc *listenSocket, CERTCertificate *cert)
{
  PRNetAddr   addr;
  PRFileDesc *tcpSocket;
  PRStatus    prStatus;
  SECStatus   secStatus;
  CERTCertDBHandle *dbHandle;
  pthread_t tid;
  thread_arg *t_arg;


  dbHandle = CERT_GetDefaultCertDB ();

  // cert_db_path gets passed to nssPasswordCallback.
  SECKEYPrivateKey *privKey = PK11_FindKeyByAnyCert (cert, (void*)cert_db_path.c_str ());
  if (privKey == NULL)
    {
      server_error (_("Unable to obtain certificate private key"));
      nssError ();
      return SECFailure;
    }

  while (pending_interrupts == 0)
    {
      /* Accept a connection to the socket. */
      tcpSocket = PR_Accept (listenSocket, &addr, PR_INTERVAL_MIN);
      if (tcpSocket == NULL)
        {
          if(PR_GetError() == PR_IO_TIMEOUT_ERROR)
            continue;
          else
            {
              server_error (_("Error accepting client connection"));
              break;
            }
        }

      /* Log the accepted connection.  */
      char buf[1024];
      prStatus = PR_NetAddrToString (&addr, buf, sizeof (buf));
      if (prStatus == PR_SUCCESS)
	{
	  if (addr.raw.family == PR_AF_INET)
	    log (_F("Accepted connection from %s:%d", buf, addr.inet.port));
	  else if (addr.raw.family == PR_AF_INET6)
	    log (_F("Accepted connection from [%s]:%d", buf, addr.ipv6.port));
	}

      /* XXX: alarm() or somesuch to set a timeout. */

      /* Accepted the connection, now handle it. */

      /* Wait for a thread to finish if there are none available */
      if(max_threads >0)
        {
          int idle_threads;
          sem_getvalue(&sem_client, &idle_threads);
          if(idle_threads <= 0)
            log(_("Server is overloaded. Processing times may be longer than normal."));
          else if (idle_threads == max_threads)
            log(_("Processing 1 request..."));
          else
            log(_F("Processing %d concurrent requests...", ((int)max_threads - idle_threads) + 1));

          sem_wait(&sem_client);
        }

      /* Create the argument structure to pass to pthread_create
       * (or directly to handle_connection if max_threads == 0 */
      t_arg = (thread_arg *)malloc(sizeof(*t_arg));
      if (t_arg == 0)
        fatal(_("No memory available for new thread arg!"));
      t_arg->tcpSocket = tcpSocket;
      t_arg->cert = cert;
      t_arg->privKey = privKey;
      t_arg->addr = addr;

      /* Handle the conncection */
      if (max_threads > 0)
        /* Create the worker thread and handle the connection. */
        pthread_create(&tid, NULL, handle_connection, t_arg);
      else
        /* Since max_threads == 0, don't spawn a new thread,
         * just handle in the current thread. */
        handle_connection(t_arg);

      // If our certificate is no longer valid (e.g. has expired), then exit.
      secStatus = CERT_VerifyCertNow (dbHandle, cert, PR_TRUE/*checkSig*/,
				      certUsageSSLServer, NULL/*wincx*/);
      if (secStatus != SECSuccess)
	{
	  // Not an error. Exit the loop so a new cert can be generated.
	  break;
	}
    }

  SECKEY_DestroyPrivateKey (privKey);
  return SECSuccess;
}

/* Function:  void server_main()
 *
 * Purpose:  This is the server's main function.  It configures a socket
 *			 and listens to it.
 *
 */
static SECStatus
server_main (PRFileDesc *listenSocket)
{
  int idle_threads;
  int timeout = 0;

  // Initialize NSS.
  SECStatus secStatus = nssInit (cert_db_path.c_str ());
  if (secStatus != SECSuccess)
    {
      // Message already issued.
      return secStatus;
    }

  // Preinitialized here due to jumps to the label 'done'.
  CERTCertificate *cert = NULL;
  bool serverCacheConfigured = false;

  // Enable all cipher suites.
  // NB: The NSS docs say that SSL_ClearSessionCache is required for the new settings to take
  // effect, however, calling it puts NSS in a state where it will not shut down cleanly.
  // We need to be able to shut down NSS cleanly if we are to generate a new certificate when
  // ours expires. It should be noted however, thet SSL_ClearSessionCache only clears the
  // client cache, and we are a server.
  /* Some NSS versions don't do this correctly in NSS_SetDomesticPolicy. */
  do {
    const PRUint16 *cipher;
    for (cipher = SSL_GetImplementedCiphers(); *cipher != 0; ++cipher)
      SSL_CipherPolicySet(*cipher, SSL_ALLOWED);
  } while (0);
  //      SSL_ClearSessionCache ();

  // Configure the SSL session cache for a single process server with the default settings.
  secStatus = SSL_ConfigServerSessionIDCache (0, 0, 0, NULL);
  if (secStatus != SECSuccess)
    {
      server_error (_("Unable to configure SSL server session ID cache"));
      nssError ();
      goto done;
    }
  serverCacheConfigured = true;

  /* Get own certificate. */
  cert = PK11_FindCertFromNickname (server_cert_nickname (), NULL);
  if (cert == NULL)
    {
      server_error (_F("Unable to find our certificate in the database at %s", 
			  cert_db_path.c_str ()));
      nssError ();
      goto done;
    }

  // Tell the world that we're listening.
  advertise_presence (cert);

  /* Handle connections to the socket. */
  secStatus = accept_connections (listenSocket, cert);

  // Tell the world we're no longer listening.
  unadvertise_presence ();

  sem_getvalue(&sem_client, &idle_threads);

  /* Wait for requests to finish or the timeout to be reached.
   * If we got here from an interrupt, exit immediately if
   * the timeout is reached. Otherwise, wait indefinitiely
   * until the threads exit (or an interrupt is recieved).*/
  if(idle_threads < max_threads)
    log(_F("Waiting for %d outstanding requests to complete...", (int)max_threads - idle_threads));
  while(idle_threads < max_threads)
    {
      if(pending_interrupts && timeout++ > CONCURRENCY_TIMEOUT_S)
        {
          log(_("Timeout reached, exiting (forced)"));
          kill_stap_spawn (SIGTERM);
          cleanup ();
          _exit(0);
        }
      sleep(1);
      sem_getvalue(&sem_client, &idle_threads);
    }

 done:
  // Clean up
  if (cert)
    CERT_DestroyCertificate (cert);

  // Shutdown NSS
  if (serverCacheConfigured && SSL_ShutdownServerSessionIDCache () != SECSuccess)
    {
      server_error (_("Unable to shut down server session ID cache"));
      nssError ();
    }
  nssCleanup (cert_db_path.c_str ());

  return secStatus;
}

static void
listen ()
{
  // Create a new socket.
  PRFileDesc *listenSocket = PR_OpenTCPSocket (PR_AF_INET6); // Accepts IPv4 too
  if (listenSocket == NULL)
    {
      server_error (_("Error creating socket"));
      nssError ();
      return;
    }

  // Set socket to be blocking - on some platforms the default is nonblocking.
  PRSocketOptionData socketOption;
  socketOption.option = PR_SockOpt_Nonblocking;
  socketOption.value.non_blocking = PR_FALSE;
  PRStatus prStatus = PR_SetSocketOption (listenSocket, & socketOption);
  if (prStatus != PR_SUCCESS)
    {
      server_error (_("Error setting socket properties"));
      nssError ();
      goto done;
    }

  // Allow the socket address to be reused, in case we want the same port across a
  // 'service stap-server restart'
  socketOption.option = PR_SockOpt_Reuseaddr;
  socketOption.value.reuse_addr = PR_TRUE;
  prStatus = PR_SetSocketOption (listenSocket, & socketOption);
  if (prStatus != PR_SUCCESS)
    {
      server_error (_("Error setting socket properties"));
      nssError ();
      goto done;
    }

  // Configure the network connection.
  PRNetAddr addr;
  memset (& addr, 0, sizeof(addr));
  prStatus = PR_InitializeNetAddr (PR_IpAddrAny, port, & addr);
  addr.ipv6.family = PR_AF_INET6;
#if 0
  // addr.inet.ip = PR_htonl(PR_INADDR_ANY);
  PR_StringToNetAddr ("::", & addr);
  // PR_StringToNetAddr ("fe80::5eff:35ff:fe07:55ca", & addr);
  // PR_StringToNetAddr ("::1", & addr);
  addr.ipv6.port = PR_htons (port);
#endif

  // Bind the socket to an address. Retry if the selected port is busy, unless the port was
  // specified directly.
  for (;;)
    {
      /* Bind the address to the listener socket. */
      prStatus = PR_Bind (listenSocket, & addr);
      if (prStatus == PR_SUCCESS)
	break;

      // If the selected port is busy. Try another, but only if a specific port was not specified.
      PRErrorCode errorNumber = PR_GetError ();
      switch (errorNumber)
	{
	case PR_ADDRESS_NOT_AVAILABLE_ERROR:
	  if (port == 0)
	    {
	      server_error (_F("Network port %hu is unavailable. Trying another port", port));
	      continue;
	    }
	  break;
	case PR_ADDRESS_IN_USE_ERROR:
	  if (port == 0)
	    {
	      server_error (_F("Network port %hu is busy. Trying another port", port));
	      continue;
	    }
	  break;
	default:
	  break;
	}
      server_error (_("Error setting socket address"));
      nssError ();
      goto done;
    }

  // Query the socket for the port that was assigned.
  prStatus = PR_GetSockName (listenSocket, &addr);
  if (prStatus != PR_SUCCESS)
    {
      server_error (_("Unable to obtain socket address"));
      nssError ();
      goto done;
    }
  char buf[1024];
  prStatus = PR_NetAddrToString (&addr, buf, sizeof (buf));
  port = PR_ntohs (addr.ipv6.port);
  log (_F("Using network address [%s]:%hu", buf, port));

  if (max_threads > 0)
    log (_F("Using a maximum of %ld threads", max_threads));
  else
    log (_("Concurrency disabled"));

  // Listen for connection on the socket.  The second argument is the maximum size of the queue
  // for pending connections.
  prStatus = PR_Listen (listenSocket, 5);
  if (prStatus != PR_SUCCESS)
    {
      server_error (_("Error listening on socket"));
      nssError ();
      goto done;
    }

  /* Initialize semephore with the maximum number of threads
   * defined by --max-threads. If it is not defined, the
   * default is the number of processors */
  sem_init(&sem_client, 0, max_threads);

  // Loop forever. We check our certificate (and regenerate, if necessary) and then start the
  // server. The server will go down when our certificate is no longer valid (e.g. expired). We
  // then generate a new one and start the server again.
  while(!pending_interrupts)
    {
      // Ensure that our certificate is valid. Generate a new one if not.
      if (check_cert (cert_db_path, server_cert_nickname (), use_db_password) != 0)
	{
	  // Message already issued
	  goto done;
	}

      // Ensure that our certificate is trusted by our local client.
      // Construct the client database path relative to the server database path.
      SECStatus secStatus = add_client_cert (server_cert_file (),
					     local_client_cert_db_path ());
      if (secStatus != SECSuccess)
	{
	  // Not fatal. Other clients may trust the server and trust can be added
	  // for the local client in other ways.
	  server_error (_("Unable to authorize certificate for the local client"));
	}

      // Launch the server.
      secStatus = server_main (listenSocket);
    } // loop forever

 done:
  sem_destroy(&sem_client); /*Not really necessary, as we are shutting down...but for correctness */
  if (PR_Close (listenSocket) != PR_SUCCESS)
    {
      server_error (_("Error closing listen socket"));
      nssError ();
    }
}

int
main (int argc, char **argv) {
  initialize (argc, argv);
  listen ();
  cleanup ();
  return 0;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
