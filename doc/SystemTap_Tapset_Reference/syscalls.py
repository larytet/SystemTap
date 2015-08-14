#!/usr/bin/env python

import os
import sys
import subprocess

if len(sys.argv) != 4:
    print >>sys.stderr, "Usage: {0} <stap_binary> <tapset_location> <output_type>".format(sys.argv[0])
    sys.exit(1)

OUTPUT_TYPE=sys.argv[3] # {xml,man}
STAP_BINARY=sys.argv[1]
TAPSET_LOC=sys.argv[2]

if OUTPUT_TYPE not in ['xml', 'man']:
    print >>sys.stderr, "Output type must be one of 'xml' or 'man'."
    sys.exit(1)

os.environ['SYSTEMTAP_TAPSET']=TAPSET_LOC

SYSCALLS_DESC = """Following is an overview of available syscall probes and
convenience variables they offer. By default, each syscall probe has name and
argstr convenience variables, which are not included in the overview in order
to keep it short. Non dwarf-based nd_syscall probes are supposed to have the
same convenience variables. """

def execShellCmd(cmd):
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
    out, err = p.communicate()
    sys.stderr.write(err)
    return [line for line in out.split('\n') if line]

def printManHeader():
    print """.\" -*- nroff -*-
.TH tapset::task 3stap --- IBM
.SH NAME
tapset::syscall \- systemtap syscall tapset

.SH DESCRIPTION
{0}
.TP
.P
.TP
""".format(SYSCALLS_DESC)

def printManFooter():
    print """
.SH SEE ALSO
.BR
.IR \%stap (1),
.IR \%stapprobes (3stap)
"""

def printSyscallsList():
    cmd = [STAP_BINARY, '-L', 'nd_syscall.*']
    out = execShellCmd(cmd)
    for line in sorted(out):
        tokens = line.split(' ')
        tokens.reverse()
        scname = tokens.pop().replace('nd_syscall.', '')
        tokens = [t.split(':')[0] for t in sorted(tokens)]
        tokens = [t for t in tokens if t not in ['argstr', 'name']]
        tokens = [t for t in tokens if t[:2] != '__']
        scargs = ", ".join(tokens)
        if OUTPUT_TYPE=="xml":
            print """<row><entry>
    {0}
</entry><entry>
    {1}
</entry></row>
""".format(scname, scargs)
        else: # OUTPUT_TYPE=="man"
            print """.P
.TP
.B syscall.{0}
{1}
""".format(scname, scargs)

def main():
    if OUTPUT_TYPE=="xml":
        for line in sys.stdin:
            if "!SyscallsDesc" in line:
                print SYSCALLS_DESC
            elif "!SyscallsList" in line:
                printSyscallsList()
            else:
                print line,
    else: # OUTPUT_TYPE=="man"
        printManHeader()
        printSyscallsList()
        printManFooter()

if __name__ == "__main__":
    main()

