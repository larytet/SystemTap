# Makefile.am --- automake input file

AUTOMAKE_OPTIONS = subdir-objects

pkglibexecpython_DATA =

pkglibexecpythondir = $(pkglibexecdir)/python

if HAVE_PYTHON_PROBES

# Why aren't we putting stap-resolve-module-function.py into
# pkglibexecpython_SCRIPTS? stap-resolve-module-function.py is a
# python script, but it will be run by the translator as either a
# python version 2 or python version 3 script. So, it doesn't need to
# be executable (since we'll be calling "python
# stap-resolve-module-function.py ARGS" or "python3
# stap-resolve-module-function.y ARGS."
pkglibexecpython_DATA += stap-resolve-module-function.py

endif
