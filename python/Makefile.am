# Makefile.am --- automake input file

AUTOMAKE_OPTIONS = subdir-objects

# Note that we have to use 'abs_builddir' here since we change
# directories back to the source directory when building the python
# extension.
AM_CPPFLAGS = -I$(srcdir)/../includes
AM_CPPFLAGS += -I$(abs_builddir)/../includes/sys

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
	(cd $(srcdir); CFLAGS="$(AM_CPPFLAGS)" $(PYTHON) setup.py build \
         --build-base $(shell readlink -f $(builddir))/py2build \
         --verbose)
endif
if HAVE_PYTHON3_PROBES
	(cd $(srcdir); CFLAGS="$(AM_CPPFLAGS)" $(PYTHON3) setup.py build \
         --build-base $(shell readlink -f $(builddir))/py3build \
         --verbose)
endif


# Note that we're rebuilding here, then installing. This is necessary
# since only the build command has the '--build-base' directory option
# and we need to keep separate build directories for python 2 and 3.
install-exec-local:
if HAVE_PYTHON2_PROBES
	(cd $(srcdir); CFLAGS="$(AM_CPPFLAGS)" $(PYTHON) setup.py build \
         --build-base $(shell readlink -f $(builddir))/py2build \
	 install --prefix $(DESTDIR)$(prefix) \
	 --single-version-externally-managed \
	 --record $(shell readlink -f $(builddir))/py2build/install_files.txt \
         --verbose)
endif
if HAVE_PYTHON3_PROBES
	(cd $(srcdir); CFLAGS="$(AM_CPPFLAGS)" $(PYTHON3) setup.py build \
         --build-base $(shell readlink -f $(builddir))/py3build \
	 install --prefix $(DESTDIR)$(prefix) \
	 --single-version-externally-managed \
	 --record $(shell readlink -f $(builddir))/py3build/install_files.txt \
         --verbose)
endif

clean-local:
	rm -rf py2build py3build

endif
