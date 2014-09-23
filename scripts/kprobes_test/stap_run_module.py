#!/usr/bin/python

# Copyright (C) 2014 Red Hat Inc.
# 
# This file is part of systemtap, and is free software.  You can
# redistribute it and/or modify it under the terms of the GNU General
# Public License (GPL); either version 2, or (at your option) any
# later version.

import sys
import os
import os.path
import time
import select
import signal
from config_opts import config_opts

def run_module():
    logfile = 'data.log'
    os.system('rm -f ./data.log')

    # Insert the module
    print "Inserting module..."
    stap_cmd = [ "staprun", "-o", "data.log", "probe_module.ko" ]
    stap_pid = os.spawnvp(os.P_NOWAIT, stap_cmd[0], stap_cmd)
    print "stap pid is", stap_pid

    # Run the load generate commands (if any).  Note we ignore the
    # return values, since we don't really care if the commands
    # succeed or not.
    if config_opts.has_key('load_cmds'):
        print "Running commands..."

        # Redirect the output to 'run_module.log' by modifying
        # what stdout/stderr points to.
        sys.stdout.flush()
        old_stdout = os.dup(sys.stdout.fileno())
        sys.stderr.flush()
        old_stderr = os.dup(sys.stderr.fileno())
        fd = os.open('run_module.log', os.O_CREAT | os.O_WRONLY)
        os.dup2(fd, sys.stdout.fileno())
        os.dup2(fd, sys.stderr.fileno())

        # Run the commands.
        pid_list = list()
        for cmd in config_opts['load_cmds']:
            pid = os.spawnvp(os.P_NOWAIT, cmd[0], cmd)
            pid_list.append(pid)
        for pid in pid_list:
            (childpid, status) = os.waitpid(pid, 0)

        # Restore old value of stdout/stderr.
        os.close(fd)
        os.dup2(old_stdout, sys.stdout.fileno())
        os.dup2(old_stderr, sys.stderr.fileno())
    else:
        time.sleep(10)

    # Remove the module
    print "Removing module..."
    os.kill(stap_pid, signal.SIGINT)
    (childpid, rc) = os.waitpid(stap_pid, 0)
    #if os.WEXITSTATUS(rc) != 0:
    #    print >>sys.stderr, "Error: rmmod failed"
    #    return -1

    # Now we have to wait until everything is flushed to the logfile
    print "Looking for output..."
    rc = 0
    f = open(config_opts['probes_result'], 'w')
    attempts = 0
    start_pos = 0
    while 1:
        l = open(logfile, 'r')

        # Find the ending size of the logfile
        end_pos = os.path.getsize(logfile)

        # Try to wait until data is available
        while 1:
            try:
                input, output, exc = select.select([l.fileno()], [], [], 60)
                break
            except select.error, err:
                if err[0] != EINTR:
                    raise

        # Get the new stuff logged
        data = l.read(end_pos - start_pos + 1)
        if not data:
            # ignore EOF
            time.sleep(2)
            attempts += 1
            if attempts < 30:
                l.close()
                continue
            else:
                print >>sys.stderr, "Error: Couldn't find data"
                rc = -1
                break;

        # Write results data
        f.write(data)

        # See if we can find 'probe_module unloaded' in the data we
        # just read.
        if data.find('probe_module unloaded') == -1:
            start_pos = end_pos
        else:
            break

    l.close()
    f.close()
    return rc

def main():
    rc = run_module()
    sys.exit(rc)

if __name__ == "__main__":
    main()
