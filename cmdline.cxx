// Shared data for parsing the stap command line
// Copyright (C) 2014 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include <cstdlib>
#include "cmdline.h"

// NB: when adding new options, consider very carefully whether they
// should be restricted from stap clients (after --client-options)!
struct option stap_long_options[] = {
  { "skip-badvars",                no_argument,       NULL, LONG_OPT_SKIP_BADVARS },
  { "vp",                          required_argument, NULL, LONG_OPT_VERBOSE_PASS },
  { "unprivileged",                no_argument,       NULL, LONG_OPT_UNPRIVILEGED },
  { "client-options",              no_argument,       NULL, LONG_OPT_CLIENT_OPTIONS },
  { "help",                        no_argument,       NULL, LONG_OPT_HELP },
  { "disable-cache",               no_argument,       NULL, LONG_OPT_DISABLE_CACHE },
  { "poison-cache",                no_argument,       NULL, LONG_OPT_POISON_CACHE },
  { "clean-cache",                 no_argument,       NULL, LONG_OPT_CLEAN_CACHE },
  { "compatible",                  required_argument, NULL, LONG_OPT_COMPATIBLE },
  { "ldd",                         no_argument,       NULL, LONG_OPT_LDD },
  { "use-server",                  optional_argument, NULL, LONG_OPT_USE_SERVER },
  { "list-servers",                optional_argument, NULL, LONG_OPT_LIST_SERVERS },
  { "trust-servers",               optional_argument, NULL, LONG_OPT_TRUST_SERVERS },
  { "use-server-on-error",         optional_argument, NULL, LONG_OPT_USE_SERVER_ON_ERROR },
  { "all-modules",                 no_argument,       NULL, LONG_OPT_ALL_MODULES },
  { "remote",                      required_argument, NULL, LONG_OPT_REMOTE },
  { "remote-prefix",               no_argument,       NULL, LONG_OPT_REMOTE_PREFIX },
  { "check-version",               no_argument,       NULL, LONG_OPT_CHECK_VERSION },
  { "version",                     no_argument,       NULL, LONG_OPT_VERSION },
  { "tmpdir",                      required_argument, NULL, LONG_OPT_TMPDIR },
  { "download-debuginfo",          optional_argument, NULL, LONG_OPT_DOWNLOAD_DEBUGINFO },
  { "dump-probe-types",            no_argument,       NULL, LONG_OPT_DUMP_PROBE_TYPES },
  { "dump-probe-aliases",          no_argument,       NULL, LONG_OPT_DUMP_PROBE_ALIASES },
  { "dump-functions",              no_argument,       NULL, LONG_OPT_DUMP_FUNCTIONS },
  { "privilege",                   required_argument, NULL, LONG_OPT_PRIVILEGE },
  { "suppress-handler-errors",     no_argument,       NULL, LONG_OPT_SUPPRESS_HANDLER_ERRORS },
  { "modinfo",                     required_argument, NULL, LONG_OPT_MODINFO },
  { "rlimit-as",                   required_argument, NULL, LONG_OPT_RLIMIT_AS },
  { "rlimit-cpu",                  required_argument, NULL, LONG_OPT_RLIMIT_CPU },
  { "rlimit-nproc",                required_argument, NULL, LONG_OPT_RLIMIT_NPROC },
  { "rlimit-stack",                required_argument, NULL, LONG_OPT_RLIMIT_STACK },
  { "rlimit-fsize",                required_argument, NULL, LONG_OPT_RLIMIT_FSIZE },
  { "sysroot",                     required_argument, NULL, LONG_OPT_SYSROOT },
  { "sysenv",                      required_argument, NULL, LONG_OPT_SYSENV },
  { "suppress-time-limits",        no_argument,       NULL, LONG_OPT_SUPPRESS_TIME_LIMITS },
  { "runtime",                     required_argument, NULL, LONG_OPT_RUNTIME },
  { "dyninst",                     no_argument,       NULL, LONG_OPT_RUNTIME_DYNINST },
  { "benchmark-sdt",               no_argument,       NULL, LONG_OPT_BENCHMARK_SDT },
  { "benchmark-sdt-loops",         required_argument, NULL, LONG_OPT_BENCHMARK_SDT_LOOPS },
  { "benchmark-sdt-threads",       required_argument, NULL, LONG_OPT_BENCHMARK_SDT_THREADS },
  { "color",                       optional_argument, NULL, LONG_OPT_COLOR_ERRS },
  { "colour",                      optional_argument, NULL, LONG_OPT_COLOR_ERRS },
  { "prologue-searching",          optional_argument, NULL, LONG_OPT_PROLOGUE_SEARCHING },
  { "save-uprobes",                no_argument,       NULL, LONG_OPT_SAVE_UPROBES },
  { "target-namespaces",           required_argument, NULL, LONG_OPT_TARGET_NAMESPACES },
  { "monitor",                     optional_argument, NULL, LONG_OPT_MONITOR },
  { NULL, 0, NULL, 0 }
};
