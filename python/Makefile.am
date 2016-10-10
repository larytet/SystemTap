# Makefile.am --- automake input file

AUTOMAKE_OPTIONS = subdir-objects

# Any script in the following variable will get byte-compiled at
# install time.
pkglibexecpython_PYTHON =

pkglibexecpython_DATA =

pkglibexecpythondir = $(pkglibexecdir)/python

if HAVE_PYTHON_PROBES

# Why aren't we putting stap-resolve-module-function.py into
# pkglibexecpython_PYTHON? stap-resolve-module-function.py is a
# python script, but it will be run by the translator as either a
# python version 2 or python version 3 script. So, it doesn't need to
# be executable (since we'll be calling "python
# stap-resolve-module-function.py ARGS" or "python3
# stap-resolve-module-function.y ARGS."
pkglibexecpython_DATA += stap-resolve-module-function.py

all-local:
if HAVE_PYTHON2_PROBES
	(cd $(srcdir); $(PYTHON) setup.py build \
         --build-base $(shell readlink -f $(builddir))/py2build \
         --verbose)
endif
if HAVE_PYTHON3_PROBES
# FIXME: Shared library module doesn't build with python3 yet.
#	(cd $(srcdir); $(PYTHON3) setup.py build \
#         --build-base $(shell readlink -f $(builddir))/py3build \
#         --verbose)
endif


install-exec-local:
if HAVE_PYTHON2_PROBES
	(cd $(srcdir); $(PYTHON) setup.py build \
         --build-base $(shell readlink -f $(builddir))/py2build \
	 install --prefix $(DESTDIR)$(prefix) \
	 --single-version-externally-managed \
	 --record $(shell readlink -f $(builddir))/py2build/install_files.txt \
         --verbose)
endif
if HAVE_PYTHON3_PROBES
# FIXME: Shared library module doesn't build with python3 yet.
endif

clean-local:
	rm -rf py2build py3build

endif
