#!/usr/bin/python

# Copyright (C) 2014 Red Hat Inc.
# 
# This file is part of systemtap, and is free software.  You can
# redistribute it and/or modify it under the terms of the GNU General
# Public License (GPL); either version 2, or (at your option) any
# later version.

import os
import sys
from config_opts import config_opts

def gen_files(dir, subset):
    f = open('%s/probes.stp' % dir, 'w')

    # Output the script
    i = 0
    while i < len(subset):
        print >>f, ("global called%d" % (i))
        i += 1
    print >>f, "probe begin(-1)"
    print >>f, "{"
    i = 0
    while i < len(subset):
        print >>f, ("\tcalled%d <<< 0" % (i))
        i += 1
    print >>f, "}"
    i = 0
    while i < len(subset):
        print >>f, ("probe kernel.function(\"%s\").call { called%d <<< 1 }" % (subset[i], i))
        i += 1
    print >>f, "probe end"
    print >>f, "{"
    i = 0
    while i < len(subset):
        print >>f, ("\tprintf(\"%%d %s\\n\", @sum(called%d))" % (subset[i], i))
        i += 1
    print >>f, "\tprintf(\"probe_module unloaded\\n\")"
    print >>f, "}"
    f.close()

def gen_module():
    f = open(config_opts['probes_current'])
    probes = f.readlines()
    f.close()
    if len(probes) == 0:
        print >>sys.stderr, ("Error: no probe points in %s"
                             % config_opts['probes_current'])
        return -1

    # Cleanup each probe by stripping whitespace
    i = 0
    while i < len(probes):
        probes[i] = probes[i].rstrip()
        i += 1

    # Generate necessary files
    gen_files(os.getcwd(), probes)

    # Try to build the module
    os.system('rm -f ./probe_module.ko')

    cmd = "stap -g --suppress-handler-errors --suppress-time-limits -vp4 -m probe_module probes.stp >build.log 2>&1"
    print "Running", cmd
    rc = os.system(cmd)
    if os.WEXITSTATUS(rc) != 0:
        print >>sys.stderr, "Error: stap failed, see build.log for details"
        return -1
    return 0

def main():
    rc = gen_module()
    sys.exit(rc)

if __name__ == "__main__":
    main()
