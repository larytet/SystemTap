# Makefile.am --- automake input file for systemtap

AUTOMAKE_OPTIONS = subdir-objects

AM_CFLAGS = -Wall -Wextra -Werror -Wunused -W -Wformat=2
AM_CXXFLAGS = -Wall -Wextra -Werror -Wunused -W -Wformat=2
AM_CPPFLAGS = -D_GNU_SOURCE
AM_CPPFLAGS += -I$(srcdir)/../includes
AM_CPPFLAGS += -I$(builddir)/../includes/sys
AM_CPPFLAGS += -DBINDIR='"$(bindir)"' -DSYSCONFDIR='"$(sysconfdir)"' -DPKGDATADIR='"${pkgdatadir}"' -DPKGLIBDIR='"$(pkglibexecdir)"' -DLOCALEDIR='"$(localedir)"'

AM_CFLAGS += @PIECFLAGS@
AM_CXXFLAGS += @PIECXXFLAGS@
AM_LDFLAGS = @PIELDFLAGS@

if HAVE_HTTP_SUPPORT
pkglibexec_PROGRAMS = stap-httpd

stap_httpd_SOURCES = main.cxx server.cxx api.cxx ../util.cxx
stap_httpd_CFLAGS = $(AM_CFLAGS)
stap_httpd_CXXFLAGS = $(AM_CXXFLAGS)
stap_httpd_CPPFLAGS = $(AM_CPPFLAGS)
stap_httpd_LDADD = -lpthread -lmicrohttpd -luuid -ljson-c
stap_httpd_LDFLAGS =  $(AM_LDFLAGS)
endif

BUILT_SOURCES =
CLEANFILES =

# Arrange for the top-level git_version.h to be regenerated at every "make".
BUILT_SOURCES += git_version.stamp
git_version.stamp ../git_version.h:
	$(MAKE) -C .. $(notdir $@)
