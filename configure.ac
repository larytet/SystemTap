dnl configure.ac --- autoconf input file for systemtap
dnl Process this file with autoconf to produce a configure script.

AC_INIT([systemtap], 3.2, systemtap@sourceware.org, systemtap)
dnl                  ^^^ see also NEWS, systemtap.spec, testsuite/configure.ac
dnl                      doc/SystemTap_Beginners_Guide/en-US/Book_Info.xml

dnl Get the target arch for libHelperSDT.so
AC_CANONICAL_TARGET

AC_PREREQ(2.63)
dnl We don't maintain a ChangeLog, which makes us non-GNU -> foreign.
AM_INIT_AUTOMAKE([no-dist foreign])
AM_MAINTAINER_MODE

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_MKDIR_P
AC_SUBST(MKDIR_P)
AC_PROG_LN_S
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AM_PROG_CC_C_O
AC_PROG_RANLIB
AC_OBJEXT
AC_EXEEXT
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_SUBST(CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SYS_LARGEFILE
AC_CHECK_FUNCS(ppoll)
AC_CHECK_FUNCS(openat)
AM_GNU_GETTEXT(external)
AM_GNU_GETTEXT_VERSION([0.19.4])

if test "x$GMSGFMT" = "x:"; then
   AC_MSG_ERROR([missing gnu /usr/bin/msgfmt])
fi

# We want the 'PYTHON' varible to be python version 2. We also want
# our custom 'PYTHON3' varible to be python version 3.
#
# Note that the python2/python3 code was inspired by code in the
# 'abrt' configure:
#   <https://github.com/abrt/abrt/blob/master/configure.ac>
#
# First, figure out what version of python is in the executable named
# 'python'. On most systems that is python version 2, but on arch
# linux that is python version 3.
AC_PATH_PROG([PYTHON_UNKNOWN], [python], [no])
if test "x$PYTHON_UNKNOWN" != "xno"; then
   # OK, we found 'python'. What version is it?
   AC_CACHE_CHECK([whether $PYTHON_UNKNOWN is version 2 or 3],
		 [ac_cv_python_unknown_version],
		 [ac_cv_python_unknown_version=`$PYTHON_UNKNOWN -c "import sys; sys.stdout.write(sys.version[[:3]][[0]])"`])
fi
  
# Now we'll update the _AM_PYTHON_INTERPRETER_LIST variable (which
# AM_PATH_PYTHON uses) to only be version 2 versions of python. Note
# that the m4_define happens when autoconf is run, but the
# FIRST_PYTHON_INTERPRETER variable expansion will happen when the
# user runs configure.
if test "x$PYTHON_UNKNOWN" != "xno" -a "x$ac_cv_python_unknown_version" = "x2"; then
   FIRST_PYTHON_INTERPRETER=python
else
   FIRST_PYTHON_INTERPRETER=python2
fi
m4_define([_AM_PYTHON_INTERPRETER_LIST],
	  [$FIRST_PYTHON_INTERPRETER python2 python2.7 python2.6])

# Now we can call AM_PATH_PYTHON to find python version 2.6+ (and
# version 2 only).
AM_PATH_PYTHON([2.6], [], [:])
python_basename=$(basename "$PYTHON")
AC_DEFINE_UNQUOTED([PYTHON_BASENAME], "${python_basename}",
		   [Base name of the python2 interpreter binary.])
# AM_PATH_PYTHON defines 'pyexecdir'. Make sure the python and
# pyexecdir variables get sent down to the subconfigure in the
# testsuite directory.
AS_VAR_APPEND([ac_configure_args], [" python='$PYTHON' pyexecdir='$pyexecdir'"])

# Now let's try to find python version 3.
if test "x$PYTHON_UNKNOWN" != "xno" -a "x$python_unknown_version" = "x3"; then
   AC_PATH_PROG([PYTHON3], [python])
else
   AC_PATH_PROGS([PYTHON3],
		 [python3 python3.5 python3.4 python3.3 python3.2 python3.1 python3.0])
fi
python3_basename=$(basename "$PYTHON3")
AC_DEFINE_UNQUOTED([PYTHON3_BASENAME], "${python3_basename}",
		   [Base name of the python3 interpreter binary.])

# If we found python version 3, set up the other variables for python
# version 3 that AM_PATH_PYTHON sets up for python version 2.
if test -n "$PYTHON3"; then
   AC_CACHE_CHECK([for python3 version], [ac_cv_python3_version],
   		  [ac_cv_python3_version=`$PYTHON3 -c "import sys; sys.stdout.write(sys.version[[:3]])"`])
   AC_SUBST([PYTHON3_VERSION], [$ac_cv_python3_version])
   AC_SUBST([PYTHON3_PREFIX], ['${prefix}'])
   AC_SUBST([PYTHON3_EXEC_PREFIX], ['${exec_prefix}'])
   AC_CACHE_CHECK([for python3 platform], [ac_cv_python3_platform],
		  [ac_cv_python3_platform=`$PYTHON3 -c "import sys; sys.stdout.write(sys.platform)"`])
   AC_SUBST([PYTHON3_PLATFORM], [$ac_cv_python3_platform])

   AC_CACHE_CHECK([for python3 script directory], [ac_cv_python3_dir],
		  [ac_cv_python3_dir=`$PYTHON3 -c "import distutils.sysconfig; print(distutils.sysconfig.get_python_lib(0,0,prefix='$PYTHON3_PREFIX'))"`])
   AC_SUBST([python3dir], [$ac_cv_python3_dir])

   AC_CACHE_CHECK([for python3 extension module directory],
   		  [ac_cv_py3execdir],
		  [ac_cv_py3execdir=`$PYTHON3 -c "import distutils.sysconfig; print(distutils.sysconfig.get_python_lib(1,0,prefix='$PYTHON3_EXEC_PREFIX'))"`])
   AC_SUBST([py3execdir], [$ac_cv_py3execdir])

   # Make sure the python3 and py3execdir variables get sent down to
   # the subconfigure in the testsuite directory.
   AS_VAR_APPEND([ac_configure_args], [" python3='$PYTHON3' py3execdir='$ac_cv_py3execdir'"])
fi

dnl Handle the prologues option.
dnl
dnl If the user didn't specify --enable-prologues/--disable-prologues
dnl and the x86 system has a version of gcc less than version 4,
dnl automatically enable prologues.
if test "${enable_prologues+set}" != set; then
  AC_MSG_CHECKING([to see if prologue searching should be the default])
  if { echo '#if __i386__ == 1 && __GNUC__ < 4'
       echo ' yes '
       echo '#endif'; } | ${CC} -E - | grep yes > /dev/null; then
    enable_prologues=yes
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
fi
AC_ARG_ENABLE([prologues],
  AS_HELP_STRING([--enable-prologues], [make -P prologue-searching default]),
  [
if test "$enable_prologues" = yes; then
  AC_DEFINE([ENABLE_PROLOGUES],[],[make -P prologue-searching default])
fi])

dnl Handle the disable-sdt-probes option.
dnl
dnl Default to --disable-sdt-probes if --enable-sdt-probes/--disable-prologues
dnl was not specified and the gcc version is less than version 4,

if test "${enable_sdt_probes+set}" != set; then
  AC_MSG_CHECKING([to see if sdt probes should be the default])
  if { echo '#if __GNUC__ < 4'
       echo ' yes '
       echo '#endif'; } | ${CC} -E - | grep yes > /dev/null; then
    enable_sdt_probes=no
    AC_MSG_RESULT([no])
  else
    AC_MSG_RESULT([yes])
  fi
fi
AC_ARG_ENABLE([sdt-probes],
  [AS_HELP_STRING([--disable-sdt-probes],
                  [disable process.mark probes in stap, staprun, stapio])])
AS_IF([test "x$enable_sdt_probes" != xno], [
  AC_DEFINE([ENABLE_SDT_PROBES], [1],
            [Define to 1 to enable process.mark probes in stap, staprun, stapio.])
])

dnl We're now requiring *some* C++11, using RHEL6 (gcc 4.4.7) as a baseline.
dnl This is too strict about __cplusplus for gcc < 4.7, so we have a manual fallback.
AX_CXX_COMPILE_STDCXX(11, noext, optional)
AS_IF([test "x$HAVE_CXX11" != x1],[
  AC_LANG_PUSH(C++)
  AX_CHECK_COMPILE_FLAG([-std=c++0x], [
      AC_MSG_NOTICE([Compiling with -std=c++0x])
      CXX="$CXX -std=c++0x"
      HAVE_CXX11=1
    ], [
      AC_MSG_ERROR([A compiler with C++11 support is required.])
    ])
  AC_LANG_POP(C++)
  ])

AC_ARG_ENABLE([ssp],
  [AS_HELP_STRING([--disable-ssp], [disable gcc stack-protector])])
AS_IF([test "x$enable_ssp" != xno],[
   save_CFLAGS="$CFLAGS"
   save_CXXFLAGS="$CXXFLAGS"
   CXXFLAGS="$CXXFLAGS -Werror -fstack-protector-all -D_FORTIFY_SOURCE=2"
   CFLAGS="$CFLAGS -Werror -fstack-protector-all -D_FORTIFY_SOURCE=2"
   AC_COMPILE_IFELSE([AC_LANG_SOURCE([int something ();])], [
       AC_MSG_NOTICE([Compiling with gcc -fstack-protector-all et al.])
	CFLAGS="$save_CFLAGS -fstack-protector-all -D_FORTIFY_SOURCE=2"
	CXXFLAGS="$save_CXXFLAGS -fstack-protector-all -D_FORTIFY_SOURCE=2"],[
       AC_MSG_NOTICE([Compiler does not support -fstack-protector-all et al.])
       CFLAGS="$save_CFLAGS"
       CXXFLAGS="$save_CXXFLAGS"])])


dnl Link with gold if possible
dnl but: https://bugzilla.redhat.com/show_bug.cgi?id=636603
dnl
dnl AC_PATH_PROG(GOLD, [ld.gold], [no])
dnl if test "x$GOLD" != "xno"
dnl then
dnl   mkdir -p Bdir
dnl   ln -sf $GOLD Bdir/ld
dnl   LDFLAGS="$LDFLAGS -B`pwd`/Bdir/"
dnl   AC_MSG_NOTICE([using ld.gold to link])
dnl fi


# Compiling without fPIE by default (see PR 9922)
AC_ARG_ENABLE([pie],
  [AS_HELP_STRING([--enable-pie], [enable position-independent-executable])])
AS_IF([test "x$enable_pie" == xyes],[
   PIECFLAGS='-fPIE'
   PIECXXFLAGS='-fPIE'
   PIELDFLAGS='-pie -Wl,-z,relro -Wl,-z,now'
   save_CFLAGS="$CFLAGS"
   save_CXXFLAGS="$CXXFLAGS"
   save_LDFLAGS="$LDFLAGS"
   CFLAGS="$CFLAGS $PIECFLAGS"
   CXXFLAGS="$CXXFLAGS $PIECXXFLAGS"
   LDFLAGS="$LDFLAGS $PIELDFLAGS"
   AC_LINK_IFELSE([AC_LANG_SOURCE([void main () {}])], [
       AC_MSG_NOTICE([Compiling with gcc pie et al.])
   ], [
       AC_MSG_NOTICE([Compiler does not support -pie et al.])
       PIECFLAGS=""
       PIECXXFLAGS=""
       PIELDFLAGS=""
   ])
   CFLAGS="$save_CFLAGS"
   CXXFLAGS="$save_CXXFLAGS"
   LDFLAGS="$save_LDFLAGS"
])
AC_SUBST(PIELDFLAGS)
AC_SUBST(PIECFLAGS)
AC_SUBST(PIECXXFLAGS)

dnl Handle optional sqlite support.  If enabled/disabled by the user,
dnl do the right thing.  If not specified by the user, use it if
dnl present.
AC_ARG_ENABLE([sqlite],
  AS_HELP_STRING([--enable-sqlite], [build with sqlite support]),
  [],					dnl ACTION-IF-GIVEN
  [enable_sqlite=check])		dnl ACTION-IF-NOT-GIVEN
sqlite3_LIBS=
AS_IF([test "x$enable_sqlite" != xno],
  [PKG_CHECK_MODULES([sqlite3], [sqlite3 > 3.7],
    [AC_DEFINE([HAVE_LIBSQLITE3], [1], [Define to 1 if you have the 'sqlite3' library (-lsqlite3).])],
    [if test "x$enable_sqlite" != xcheck; then
      AC_MSG_FAILURE([--enable-sqlite was given, but test for sqlite failed])
     fi])])

dnl Handle the option to only build runtime
AC_ARG_ENABLE([translator],
  AS_HELP_STRING([--disable-translator], [build only runtime utilities]),
  [],
  [enable_translator="yes"])
AM_CONDITIONAL([BUILD_TRANSLATOR], [test "$enable_translator" == "yes"])

dnl Handle the option to build the crash extension
AC_ARG_ENABLE([crash],
  AS_HELP_STRING([--enable-crash@<:@=DIRECTORY@:>@],
    [enable crash extension (default is disabled).  Optional DIRECTORY
    is the path to the crash header file (needed if installed in a
    non-standard location).]),
  [if test "$enable_crash" != "no"; then
    dnl Handle custom install dir (if needed)
    save_CPPFLAGS="$CPPFLAGS"
    if test "$enable_crash" != "yes"; then
      staplog_CPPFLAGS=-I$enable_crash
      CPPFLAGS="${staplog_CPPFLAGS} $CPPFLAGS"
      AC_SUBST([staplog_CPPFLAGS])
    fi
    AC_CHECK_HEADERS([crash/defs.h], [],
     [AC_MSG_ERROR([cannot find required crash header (crash-devel may need to be installed)])],
     [
#define NR_CPUS 256
     ])
    CPPFLAGS="$save_CPPFLAGS"
  fi],
  [enable_crash="no"])
AM_CONDITIONAL([BUILD_CRASHMOD], [test "$enable_crash" != "no"])

dnl Handle the option to build the documentation
building_docs="no"
AC_ARG_ENABLE([docs],
  AS_HELP_STRING([--enable-docs],
    [enable building documentation (default on if latex etc. found).]),
  [enable_docs=$enableval],
  [enable_docs="check"])
AC_CHECK_PROG(have_latex, latex, yes, no)
AC_CHECK_PROG(have_dvips, dvips, yes, no)
AC_CHECK_PROG(have_ps2pdf, ps2pdf, yes, no)
if test "x${have_latex}${have_dvips}${have_ps2pdf}" != "xyesyesyes"; then
   if test "$enable_docs" == "yes"; then
      AC_MSG_ERROR([cannot find all tools for building documentation])
   fi
  if test "$enable_docs" == "check"; then
    AC_MSG_WARN([will not build documentation, cannot find all tools])
  fi
fi
if test "x${have_latex}${have_dvips}${have_ps2pdf}" == "xyesyesyes" -a "$enable_docs" != "no"; then
   building_docs="yes"
fi
AM_CONDITIONAL([BUILD_DOCS], [test "$building_docs" == "yes"])


dnl Handle the option to build the reference documentation
building_refdocs="no"
AC_ARG_ENABLE([refdocs],
  AS_HELP_STRING([--enable-refdocs],
    [enable building reference documentation (default on if other documentation built).]),
  [enable_refdocs=$enableval],
  [enable_refdocs="check"])
if test "$building_docs" == "no" -a "$enable_refdocs" == "yes" ; then
   AC_MSG_ERROR([must use --enable-docs with --enable-refdocs])
fi
if test "$enable_refdocs" != "no" -a "${building_docs}" == "yes"; then
   building_refdocs="yes"
fi
AM_CONDITIONAL([BUILD_REFDOCS], [test "$building_refdocs" == "yes"])

AC_CHECK_PROG(have_xmlto, xmlto, yes, no)
AC_CHECK_PROG(have_fop, fop, yes, no)
if test "x${have_fop}" == "xyes"; then
   # Due to rhbz505364 / 830266, we must actually test-run fop, not just
   # hope that it works.
   AC_MSG_CHECKING([to see if xmlto --with-fop actually works])
   if xmlto --with-fop pdf ${srcdir}/doc/SystemTap_Tapset_Reference/dummy-tapsets.xml >/dev/null 2>&1; then
      AC_MSG_RESULT([yes])
   else
      AC_MSG_RESULT([it's dead, Jim])
      have_fop="broken"
   fi
   rm -f dummy-tapsets.pdf
fi
AM_CONDITIONAL([HAVE_FOP], [test "$have_fop" == "yes"])
AM_CONDITIONAL([HAVE_XMLTO], [test "$have_xmlto" == "yes"])

dnl Handle the option to build the html documentation
building_htmldocs="no"
AC_ARG_ENABLE([htmldocs],
  AS_HELP_STRING([--enable-htmldocs],
    [enable building html documentation (default off).]),
    [building_htmldocs=$enableval],
    [building_htmldocs="no"])
if test "$have_xmlto" == "no" -a "$building_htmldocs" == "yes"; then
   AC_MSG_ERROR([xmlto required for building html documentation])
fi

AM_CONDITIONAL([BUILD_HTMLDOCS], [test "$building_htmldocs" == "yes"])

dnl There is a strange bug in older versions of xmlto when generating pdf.
dnl https://bugzilla.redhat.com/show_bug.cgi?id=526273
dnl So make sure to have a chapter title starting with L plus an refentry.
dnl This will make sure the xmlto pdf support test fails on buggy versions.
cat > conftest.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.1.2//EN"
"http://www.oasis-open.org/docbook/xml/4.1.2/docbookx.dtd" []>
   <book lang="en">
   <bookinfo><title>foo</title></bookinfo>
   <chapter id="logging.stp"><title>Logging Tapset</title>
   <refentry id="API-log"><refnamediv><refname>log</refname>
   <refpurpose>logging</refpurpose></refnamediv>
   <refsect1><title>Description</title>
   <para>baz</para></refsect1></refentry></chapter>
   </book>
EOF

if test "x${have_xmlto}" == "xyes"; then
AC_MSG_CHECKING([for xmlto --stringparam support])
   xmlto --stringparam man.authors.section.enabled=0 html-nochunks conftest.xml >/dev/null 2>&1
   if test $? == 0; then
      have_xmlto_stringparam="yes"
      AC_MSG_RESULT([yes])
   else
      AC_MSG_RESULT([no])
   fi
   rm -f conftest.html
fi
AM_CONDITIONAL([XMLTO_STRINGPARAM], [test "$have_xmlto_stringparam" == "yes"])

rm -f conftest.xml

dnl See if we have the nss/nspr headers and libraries
AC_ARG_WITH([nss],
  AS_HELP_STRING([--without-nss],
    [Do not use NSS even if present]))

AS_IF([test "x$with_nss" != "xno"], [
  PKG_CHECK_MODULES([nss], [nss >= 3],
    [have_nss=yes
     AC_DEFINE([HAVE_NSS], [1], [Define to 1 if you have the nss libraries.])
    ], [have_nss=no])
], [have_nss=no])

AM_CONDITIONAL([HAVE_NSS], [test "${have_nss}" = "yes"])

dnl Handle dracut directory configuration.  Note we hard-code /usr by default,
dnl instead of $prefix, because dracut is a system service that doesn't listen
dnl at relocatable directories.
AC_ARG_WITH([dracutstap],
  AS_HELP_STRING([--with-dracutstap=DIR],
    [Install dracut module files in DIR]))
AS_IF([test "x$with_dracutstap" != "x"],[dracutstap="$with_dracutstap"],
            [dracutstap=/usr/lib/dracut/modules.d/99stap])
AC_MSG_NOTICE([using dracut module directory $dracutstap])
AC_SUBST(dracutstap)

dnl PR20850 Fix the boot time probing feature for fedora too. Typical rhel
dnl location is /sbin, typical fedora location is /usr/bin.
AC_ARG_WITH([dracutbindir],
  AS_HELP_STRING([--with-dracutbindir=DIR],
    [Use the dracut binary located in DIR]))
AS_IF([test "x$with_dracutbindir" != "x"],[dracutbindir="$with_dracutbindir"],
            [dracutbindir=/sbin])
AC_MSG_NOTICE([using dracut binary $dracutbindir])
AC_SUBST(dracutbindir)

dnl Handle the option to build the compile server.
AC_ARG_ENABLE([server],
  AS_HELP_STRING([--enable-server],
    [enable building of stap-server (default on if nss etc. found).]),
  [enable_server=$enableval],
  [enable_server="check"])

if test "$enable_server" != "no"; then
dnl See if we have enough libraries and tools to build the compile server
  if test "x${have_nss}" != "xyes"; then
    AC_MSG_WARN([will not build systemtap compile server, cannot find nss headers])
  fi		       
fi
AM_CONDITIONAL([BUILD_SERVER], [test "${have_nss}" == "yes" -a "$enable_server" != "no"])

if test "${have_nss}" != "yes"; then
  AC_MSG_WARN([compile-server client functionality will be disabled, cannot find nss development files])
fi		       

dnl See if we have the avahi libraries and headers
AC_ARG_WITH([avahi],
  AS_HELP_STRING([--without-avahi],
    [Do not use Avahi even if present]))

AS_IF([test "x$with_avahi" != "xno"], [
  PKG_CHECK_MODULES([avahi], [avahi-client],
    [have_avahi=yes
     AC_DEFINE([HAVE_AVAHI], [1], [Define to 1 if you have the avahi libraries.])
    ], [have_avahi=no])
], [have_avahi=no])

AM_CONDITIONAL([HAVE_AVAHI], [test "${have_avahi}" = "yes"])

if test "${have_avahi}" != "yes"; then
  AC_MSG_WARN([some compile-server functionality will be restricted, cannot find avahi development files])
fi


dnl Look for librpm.
AC_ARG_WITH([rpm],
  [AS_HELP_STRING([--with-rpm],
                  [query rpm database for missing debuginfos])], [], [with_rpm="auto"])
if test "$with_rpm" != "no"; then
  AC_CHECK_LIB(rpm, rpmtsInitIterator, [
                    AC_DEFINE([HAVE_LIBRPM],[1],[have librpm])
                    stap_LIBS="$stap_LIBS -lc -lrpm"
                    have_librpm="yes"], [have_librpm="no"])
  dnl explicit -lrpmdb is a separate requirement on some older librpms
  AC_CHECK_LIB(rpmdb, rpmdbNextIterator, [
                    stap_LIBS="$stap_LIBS -lrpmdb"])
  AC_CHECK_LIB(rpmio, rpmFreeCrypto, [
                    AC_DEFINE([HAVE_LIBRPMIO],[1],[have librpmio])
                    stap_LIBS="$stap_LIBS -lc -lrpmio"
                    have_librpmio="yes"], [have_librpmio="no"])
  if test "x$have_librpm" != "xyes" -a "$with_rpm" == "yes"; then
     AC_MSG_ERROR([cannot find librpm])
  fi
  if test "x$have_librpmio" != "xyes" -a "$with_rpm" == "yes"; then
     AC_MSG_WARN([cannot find librpmio])
  fi
fi

dnl Look for readline.
dnl
dnl First save the orignal value of LIBS.
LIBS_no_readline=$LIBS

dnl Check how for readline presence and how to link with it. On some
dnl systems you need to add a termcap compatible library.
have_libreadline="no"
AC_MSG_CHECKING([how to link readline libs])
for libtermcap in "" tinfo ncursesw ncurses curses termcap; do
  if test -z "$libtermcap"; then
    READLINE_LIBS="-lreadline"
  else
    READLINE_LIBS="-lreadline -l$libtermcap"
  fi
  LIBS="$READLINE_LIBS $LIBS_no_readline"
  AC_LINK_IFELSE(
    [AC_LANG_CALL([],[readline])],
    [have_libreadline="yes"])
  if test "$have_libreadline" = "yes"; then
    break
  fi
done
if test "$have_libreadline" = "no"; then
  AC_MSG_RESULT([none])
  READLINE_LIBS=""
else
  AC_MSG_RESULT([$READLINE_LIBS])
  AC_DEFINE(HAVE_LIBREADLINE, [1],
	    [Define if you have the readline library (-lreadline).])
fi
AC_SUBST([READLINE_LIBS])
AM_CONDITIONAL([HAVE_LIBREADLINE], [test "$have_libreadline" == "yes"])

dnl End of readline checks: restore LIBS
LIBS=$LIBS_no_readline

dnl Allow user to choose python3 for /usr/bin/dtrace
AC_ARG_WITH([python3],
  AS_HELP_STRING([--with-python3],[prefer /usr/bin/python3]))
AS_IF([test "x$with_python3" = "xyes"],
      [AS_IF([test -z "$PYTHON3"],
             [AC_MSG_ERROR([python version 3 is required])],
	     [AC_SUBST(preferred_python,[$PYTHON3])])],
      [AS_IF([test "x$PYTHON" = "x:"],
             [AC_MSG_ERROR([python version 2 is required])],
	     [AC_SUBST(preferred_python,[$PYTHON])])])
      
dnl Handle elfutils.  If '--with-elfutils=DIR' wasn't specified, used
dnl the system's elfutils.
build_elfutils=no
AC_ARG_WITH([elfutils],
  AS_HELP_STRING([--with-elfutils=DIRECTORY],
    [find elfutils source code in DIRECTORY]),
  [
case "$with_elfutils" in
yes) AC_MSG_ERROR([--with-elfutils requires an argument]) ;;
''|no) ;;
*) build_elfutils=yes ;;
esac])
AM_CONDITIONAL(BUILD_ELFUTILS, test $build_elfutils = yes)
AC_SUBST(elfutils_abs_srcdir, `AS_IF([test $build_elfutils = yes],
				     [cd $with_elfutils && pwd])`)

if test $enable_translator = yes; then
  if test $build_elfutils = no; then
    # Need libdwfl-capable recent elfutils http://elfutils.org/

    # On modern debian/ubuntu, libebl has been merged into libdw
    # http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=457543
    save_LIBS="$LIBS"
    AC_CHECK_LIB(ebl, ebl_get_elfmachine,[ebl_LIBS=-lebl],[ebl_LIBS=])
    LIBS="$save_LIBS"

    save_LIBS="$LIBS"
    AC_CHECK_LIB(dw, dwfl_module_getsym,[],[
      AC_MSG_ERROR([missing elfutils development headers/libraries (install elfutils-devel, libebl-dev, libdw-dev and/or libebl-devel)])],
      [-Wl,--start-group -ldw $ebl_LIBS -Wl,--end-group -lelf])
    AC_CHECK_LIB(dw, dwarf_next_unit,[],[
      AC_MSG_ERROR([elfutils, libdw too old, need 0.148+])],
      [-Wl,--start-group -ldw $ebl_LIBS -Wl,--end-group -lelf])
    stap_LIBS="$stap_LIBS -Wl,--start-group -ldw $ebl_LIBS -Wl,--end-group -lelf"
    LIBS="$save_LIBS"
  else
    # We built our own and stap_LDFLAGS points at the install.
    stap_LIBS="$stap_LIBS -Wl,--start-group -ldw -lebl -Wl,--end-group -lelf"
  fi
fi

AC_SUBST(stap_LIBS)
AC_MSG_NOTICE([stap will link $stap_LIBS])

# staprun has more modest libelf needs
if test $build_elfutils = no; then
  save_LIBS="$LIBS"
  dnl this will only succeed with elfutils 0.142+
  AC_CHECK_LIB(elf,elf_getshdrstrndx,[],[
    AC_MSG_FAILURE([libelf too old, need 0.142+])])
  staprun_LIBS="$staprun_LIBS -lelf"
  LIBS="$save_LIBS"
else
  # We built our own and staprun_LDFLAGS points at the install.
  staprun_LIBS="$staprun_LIBS -lelf"
fi

AC_SUBST(staprun_LIBS)
AC_MSG_NOTICE([staprun will link $staprun_LIBS])


dnl Plop in the build (configure) date
date=`date +%Y-%m-%d`
AC_DEFINE_UNQUOTED(DATE, "$date", [Configuration/build date])
AC_SUBST(DATE, "$date")

# Before PR4037, we used to arrange to pass CFLAGS+=-m64 for a staprun
# being compiled on 32-bit userspace but running against 64-bit kernels.
# This is no longer necessary.

AC_LANG_PUSH(C++)
# Use boost::string_ref if available
AC_CHECK_HEADERS([boost/utility/string_ref.hpp])
AC_LANG_POP(C++)


# Check for Dyninst headers and libraries
AC_ARG_WITH([dyninst],
  AS_HELP_STRING([--with-dyninst=DIRECTORY],
    [find dyninst headers/libraries in DIRECTORY]))

case "$with_dyninst" in
no) ;;
''|yes) # Try a simple-minded distro search
     DYNINST_CXXFLAGS="-I/usr/include/dyninst"
     DYNINST_LDFLAGS="-L/usr/lib64/dyninst -L/usr/lib/dyninst" # XXX both lib paths!?!
     ;;
*)  # Use paths in the user-specified prefix
    DYNINST_CXXFLAGS="-I$with_dyninst/include"
    DYNINST_LDFLAGS="-L$with_dyninst/lib/" # XXX need lib64 too?
    ;;
esac
if test "$with_dyninst" != "no"; then
  AC_LANG_PUSH(C++)
  old_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $DYNINST_CXXFLAGS"
  AC_MSG_NOTICE([checking dyninst support])
  AC_CHECK_HEADERS([BPatch_object.h], [
      AC_SUBST(DYNINST_CXXFLAGS)
      AC_SUBST(DYNINST_LDFLAGS)
      AC_DEFINE([HAVE_DYNINST],[1],[Define to 1 if Dyninst is enabled])
      have_dyninst=yes
      AC_MSG_NOTICE([dyninst support available])])
  if test -n "$with_dyninst" -a "$have_dyninst" != "yes"; then
    AC_MSG_ERROR([Dyninst does not appear to be usable])
  fi
  CPPFLAGS="$old_CPPFLAGS"
  AC_LANG_POP(C++)
fi
AM_CONDITIONAL([HAVE_DYNINST], [test "${have_dyninst}" = "yes"])


dnl Check for the libvirt and libxml2 devel packages

AC_ARG_ENABLE([virt],
  AS_HELP_STRING([--enable-virt],
    [enable building of stapvirt support (default on if libvirt etc. found).]),
  [enable_virt=$enableval],
  [enable_virt="check"])

dnl We require libvirt >= 1.0.2 because stapvirt relies on the
dnl virDomainOpenChannel function, which was implemented in 1.0.2.
PKG_CHECK_MODULES([libvirt], [libvirt >= 1.0.2], [
   have_libvirt=yes
   AC_DEFINE([HAVE_LIBVIRT],[1],[Define to 1 if libvirt development libraries are installed])
   ], [have_libvirt=no])
AM_CONDITIONAL([HAVE_LIBVIRT], [test "${have_libvirt}" = "yes"])
PKG_CHECK_MODULES([libxml2], [libxml-2.0], [
   have_libxml2=yes
   AC_DEFINE([HAVE_LIBXML2],[1],[Define to 1 if libxml2 development libraries are installed])
   ], [have_libxml2=no])
AM_CONDITIONAL([HAVE_LIBXML2], [test "${have_libxml2}" = "yes"])

if test "$enable_virt" != "no"; then
dnl See if we have enough libraries and tools to build the virt server
  if test "x${have_libvirt}" != "xyes"; then
    AC_MSG_WARN([will not build systemtap virt support, cannot find libvirt headers])
  fi		       
  if test "x${have_libxml2}" != "xyes"; then
    AC_MSG_WARN([will not build systemtap virt support, cannot find xml2 headers])
  fi		       
fi
AM_CONDITIONAL([BUILD_VIRT], [test "${have_libvirt}" == "yes" -a "${have_libxml2}" == "yes" -a "$enable_virt" != "no"])

dnl Handle the option to build python2 probe support
AC_ARG_WITH([python2-probes],
  AS_HELP_STRING([--without-python2-probes],
    [Disable building python version 2 probe support, even if it is available]))
have_python2_support=no
AS_IF([test "x$with_python2_probes" != "xno"],
  [dnl First, check to make sure we have the 'python-config' script.
   pathless_python=$(basename "$PYTHON")
   AC_PATH_PROG([PYTHON_CONFIG], [${pathless_python}-config], [no])
   AS_IF([test "x$PYTHON_CONFIG" != "xno"],
	 [dnl Use the 'python-config' script to find the python
	  dnl include directories.
	  PYTHON_CPPFLAGS=`$PYTHON_CONFIG --includes 2> /dev/null`
	  AC_SUBST([PYTHON_CPPFLAGS], [$PYTHON_CPPFLAGS])
	  save_CPPFLAGS="$CPPFLAGS"
	  CPPFLAGS="$PYTHON_CPPFLAGS $CPPFLAGS"
	  dnl Now make sure we can find the python 2 Python.h file. We
	  dnl need to clear the cached result, since this might be a
	  dnl cached result from the python 3 header file (since they
	  dnl share the same name).
	  AS_UNSET([ac_cv_header_Python_h])
	  AC_CHECK_HEADERS([Python.h], [have_python2_support=yes])
	  CPPFLAGS="$save_CPPFLAGS"])])
AS_IF([test "x$have_python2_support" = "xyes"],
  [AC_DEFINE([HAVE_PYTHON2_PROBES], [1],
    [Define to 1 to enable python version 2 probe support in systemtap.])],
  [AS_IF([test "x$with_python2_probes" = "xyes"],
    [AC_MSG_ERROR([python version 2 probe support requested but not found])])
])
AM_CONDITIONAL([HAVE_PYTHON2_PROBES], [test "x$have_python2_support" = "xyes"])

dnl Handle the option to build python3 probe support
AC_ARG_WITH([python3-probes],
  AS_HELP_STRING([--without-python3-probes],
    [Disable building python version 3 probe support, even if it is available]))
have_python3_support=no
AS_IF([test "x$with_python3_probes" != "xno"],
  [dnl First, check to make sure we have the 'python3-config' script.
   pathless_python3=$(basename "$PYTHON3")
   if test "x${pathless_python3}" == "x"; then
	pathless_python3="python3"   
   fi
   AC_PATH_PROG([PYTHON3_CONFIG], [${pathless_python3}-config], [no])
   AS_IF([test "x$PYTHON3_CONFIG" != "xno"],
	 [dnl Use the 'python3-config' script to find the python
	  dnl include directories.
	  PYTHON3_CPPFLAGS=`$PYTHON3_CONFIG --includes 2> /dev/null`
	  AC_SUBST([PYTHON3_CPPFLAGS], [$PYTHON3_CPPFLAGS])
	  save_CPPFLAGS="$CPPFLAGS"
	  CPPFLAGS="$PYTHON3_CPPFLAGS $CPPFLAGS"
	  dnl Now make sure we can find the python 3 Python.h
	  dnl file. However, we've got a problem. AC_CHECK_HEADERS
	  dnl was called above for the python 2 version of
	  dnl Python.h. But, autoconf doesn't really know the
	  dnl difference between the two files, since they both have
	  dnl the same name. So, we'll unset the cache variable.
	  AS_UNSET([ac_cv_header_Python_h])
	  AC_CHECK_HEADERS([Python.h], [have_python3_support=yes])
	  CPPFLAGS="$save_CPPFLAGS"])])
AS_IF([test "x$have_python3_support" = "xyes"],
  [AC_DEFINE([HAVE_PYTHON3_PROBES], [1],
    [Define to 1 to enable python version 3 probe support in systemtap.])],
  [AS_IF([test "x$with_python3_probes" = "xyes"],
    [AC_MSG_ERROR([python version 3 probe support requested but not found])])
])
AM_CONDITIONAL([HAVE_PYTHON3_PROBES], [test "x$have_python3_support" = "xyes"])

dnl We want either (or both) python probe support.
AM_CONDITIONAL([HAVE_PYTHON_PROBES],
  [test "x$have_python2_support" = "xyes" -o "x$have_python3_support" = "xyes"])

dnl Check for presence of json-c and ncurses for use in monitor mode
PKG_CHECK_MODULES([jsonc], [json-c >= 0.12], [have_jsonc=yes], [have_jsonc=no])
PKG_CHECK_MODULES([ncurses], [ncurses], [have_ncurses=yes], [have_ncurses=no])
AM_CONDITIONAL([HAVE_MONITOR_LIBS], [test "${have_jsonc}" == "yes" -a "${have_ncurses}" == "yes"])
if test "${have_jsonc}" == "yes" -a "${have_ncurses}" == yes; then
  AC_DEFINE([HAVE_MONITOR_LIBS],[1],[Define to 1 if json-c and ncurses libraries are installed])
fi

AC_CACHE_CHECK([for assembler .section "?" flags support], stap_cv_sectionq, [
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wa,--fatal-warnings"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([asm(".section .note.foo,\"?\",\"note\"\n"
		       ".byte 1, 2, 3\n"
		       ".previous\n"
		       ".section .text,\"axG\",\"progbits\",foogroup,comdat\n"
		       ".byte 1\n"
		       ".pushsection .note.foo,\"?\",\"note\"\n"
		       ".byte 4, 5, 6\n"
		       ".popsection\n"
		       ".byte 2\n");])],
		  stap_cv_sectionq=yes, stap_cv_sectionq=no)
CFLAGS="$old_CFLAGS"])
AC_SUBST(support_section_question)
support_section_question=0
if test $stap_cv_sectionq = yes; then
  support_section_question=1
fi
AC_CONFIG_FILES([includes/sys/sdt-config.h po/Makefile.in])

if test $build_elfutils = yes -a $enable_translator = yes; then
  here=`pwd`
  case "$with_elfutils" in
  /*) elfutils_srcdir="$with_elfutils" ;;
  *) elfutils_srcdir="${here}/$with_elfutils" ;;
  esac

  if ! test -f ${elfutils_srcdir}/configure; then
    AC_MSG_ERROR([No ${elfutils_srcdir}/configure, forgot to run autoreconf -i?])
  fi
  AC_MSG_NOTICE([running ${elfutils_srcdir}/configure])

  save_CFLAGS="$CFLAGS"
  save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -fgnu89-inline"
  CFLAGS="$CFLAGS -fgnu89-inline"
  gnu89_inline_flag=""
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([int something ();])], [
    AC_MSG_NOTICE([Compiling elfutils with gcc -fgnu89-inline])
    gnu89_inline_flag="-fgnu89-inline"],[
    AC_MSG_NOTICE([Compiler does not support -fgnu89-inline])])
  CFLAGS="$save_CFLAGS"
  CXXFLAGS="$save_CXXFLAGS"

  # Our libdw.so's libebl will look in $ORIGIN/../lib/... but that
  # $ORIGIN is where libdw.so resides, which is not where there is a ../lib.
  # Note that $libdir might be using a quoted use of $exec_prefix or $prefix.
  # So we must make sure to pass those settings to elfutils configure.
  elfutils_rpath="-Wl,--enable-new-dtags,-rpath,${libdir}/${PACKAGE_NAME}"
  # Check whether this is a source release, or a source checkout.
  # We need --enable-maintainer-mode if it isn't a source release.
  # elfutils.spec is only distributed in source releases.
  if test -f ${elfutils_srcdir}/elfutils.spec; then
	need_maintainer_option=""
  else
	need_maintainer_option="--enable-maintainer-mode"
  fi
  (mkdir -p build-elfutils && cd build-elfutils &&
   bash ${elfutils_srcdir}/configure --enable-libebl-subdir=${PACKAGE_NAME} \
				--includedir="${here}/include-elfutils" \
				--libdir="${here}/lib-elfutils" \
				--exec-prefix="$exec_prefix" \
				--prefix="$prefix" \
				--enable-dwz \
				${need_maintainer_option} \
				CFLAGS="${CFLAGS/-Wall/} $gnu89_inline_flag -fexceptions" \
				LDFLAGS="$LDFLAGS $elfutils_rpath" &&
   if test -f ${elfutils_srcdir}/config/version.h.in; then
        echo Found elfutils/version.h header...
   else
	echo elfutils/version.h header not found, need elfutils 0.142+...
	exit -1
   fi
  )
  if test $? != 0; then exit -1; fi
fi

dnl Optional libselinux support allows stapdyn to check
dnl for booleans that would prevent Dyninst from working.
AC_ARG_WITH([selinux],
  AS_HELP_STRING([--without-selinux],
    [Do not use libselinux even if present]))

AS_IF([test "x$with_selinux" != "xno"], [
  PKG_CHECK_MODULES([selinux], [libselinux],
    [have_selinux=yes
     AC_DEFINE([HAVE_SELINUX], [1], [Define to 1 if you have the SELinux libraries.])
    ], [have_selinux=no])
], [have_selinux=no])

AM_CONDITIONAL([HAVE_SELINUX], [test "${have_selinux}" = "yes"])

dnl Used in monitor mode. Only available on kernel versions >= 2.6.35
AC_CHECK_DECL([F_SETPIPE_SZ],
              [AC_DEFINE(HAVE_F_SETPIPE_SZ,[1], Define to 1 if F_SETPIPE_SZ is available.)],
              [],
              [#include <fcntl.h>])

dnl Handle java+byteman support
AC_CHECK_PROG(have_javac, javac, yes, no)
AC_CHECK_PROG(have_jar, jar, yes, no)
if test "$have_javac" != no -a "$have_jar" != no; then
echo java found, will try to configure Byteman support

AC_ARG_WITH([java],
  [AS_HELP_STRING([--with-java=DIRECTORY],
     [Specify JDK directory to compile libHelperSDT.so against (default is /usr/lib/jvm/java)])],
  [],
  [with_java=/usr/lib/jvm/java])
dnl don't use AC_CHECK_FILE here, as that blocks cross-compiling
if test ! -d "$with_java"; then
   with_java=no
fi
AC_SUBST(JAVADIR, "${with_java}") # always needed to compile

if test "$with_java" != "no"; then
AC_DEFINE_UNQUOTED(HAVE_JAVA, "1", [Flag indicating that libHelperSDT.so is available (can be found in PKGLIBDIR)])
fi

AM_CONDITIONAL([HAVE_JAVA], [test "$with_java" != "no"])

else
AC_MSG_WARN([will not run per-method java probing, missing byteman or java requirements])
AM_CONDITIONAL([HAVE_JAVA],false)
fi # java+byteman support

AC_SUBST(ENABLE_NLS, "$USE_NLS")
AC_SUBST(localedir, "$localedir")
AC_SUBST(LOCALEDIR, "$localedir")

AC_ARG_WITH([extra-version],
  AS_HELP_STRING([--with-extra-version=STRING],
    [Add STRING to stap -V version]))
AS_IF([test "x$with_extra_version" != "xno"], [
    stap_extra_version="$with_extra_version"
    AC_MSG_NOTICE([Adding extra version $stap_extra_version])], [
    stap_extra_version=""])
AC_DEFINE_UNQUOTED(STAP_EXTRA_VERSION, "$stap_extra_version", [extra stap version code])
AC_SUBST(STAP_EXTRA_VERSION, "$stap_extra_version")

dnl Handle the option to build httpd web compiliation service
AC_ARG_ENABLE([http],
  AS_HELP_STRING([--disable-http],
    [Disable building http web compilation service, even if we could]))
have_http_support=no
AS_IF([test "x$enable_http" != "xno"],
  [dnl Do we have the microhttpd library?
   PKG_CHECK_MODULES([libmicrohttpd], [libmicrohttpd > 0.9.0], [have_libmicrohttpd=true], [have_libmicrohttpd=false])

   dnl Do we have the uuid library?
   PKG_CHECK_MODULES([uuid], [uuid >= 2.17.0], [have_libuuid=true], [have_libuuid=false])

   dnl If we have both libraries, we could build the httpd web
   dnl complilation service.
   AS_IF([test "x$have_libmicrohttpd" == "xtrue" -a "x$have_libuuid" == "xtrue"], [have_http_support=yes])])

AS_IF([test "x$have_http_support" = "xyes"],
  [AC_DEFINE([HAVE_HTTP_SUPPORT], [1],
    [Define to 1 to enable http web service support in systemtap.])],
  [AS_IF([test "x$enable_http" = "xyes"],
    [AC_MSG_ERROR([http service support requested but not found])])
])
AM_CONDITIONAL([HAVE_HTTP_SUPPORT], [test "x$have_http_support" = "xyes"])

dnl This is here mainly to make sure that configure --prefix=... changes
dnl the config.h files so files depending on it are recompiled
dnl prefix is passed through indirectly in the Makefile.am AM_CPPFLAGS.
dnl Formerly: Don't use this directly (when not given it is set to NONE).
dnl Currently: inline autoconf's later defaulting

stap_prefix=$prefix
test "$stap_prefix" = NONE && stap_prefix=$ac_default_prefix
AC_DEFINE_UNQUOTED(STAP_PREFIX, "$stap_prefix", [configure prefix location])
AC_SUBST(STAP_PREFIX, "$stap_prefix")

AC_CONFIG_HEADERS([config.h:config.in])
AC_CONFIG_FILES([Makefile doc/Makefile man/Makefile man/cs/Makefile \
doc/beginners/Makefile doc/SystemTap_Tapset_Reference/Makefile \
man/stap.1 man/stappaths.7 man/systemtap.8 \
man/cs/stap.1 man/cs/stappaths.7 man/cs/systemtap.8 \
initscript/config.systemtap initscript/config.stap-server \
initscript/systemtap initscript/stap-server \
initscript/99stap/module-setup.sh \
initscript/99stap/install \
initscript/99stap/check ])
AC_CONFIG_SUBDIRS(testsuite)
if test $enable_translator == "yes"; then
	AC_CONFIG_FILES([run-stap], [chmod +x run-stap])
fi
AC_CONFIG_FILES([dtrace], [chmod +x dtrace])
AC_CONFIG_FILES(stapdyn/Makefile)
AC_CONFIG_FILES(java/Makefile)
AC_CONFIG_FILES([java/stapbm], [chmod +x java/stapbm])
AC_CONFIG_FILES(python/Makefile)
AC_CONFIG_FILES(staprun/Makefile)
AC_CONFIG_FILES(httpd/Makefile)
AC_CONFIG_FILES([staprun/run-staprun], [chmod +x staprun/run-staprun])
AC_CONFIG_FILES([staprun/guest/stapshd], [chmod +x staprun/guest/stapshd])
AC_CONFIG_FILES([staprun/guest/stapsh-daemon], [chmod +x staprun/guest/stapsh-daemon])
AC_CONFIG_FILES([staprun/guest/stapsh@.service])

# Setup "shadow" directory doc/beginners that has the basic directories  setup for
# xmlto in one directory (through directory links if necessary).
# It would be nice to use AC_CONFIG_LINKS, but automake complains when
# the src is a directory and not a file.
AC_CONFIG_COMMANDS([doc/beginners],
                   [rm -f $ac_abs_top_builddir/doc/beginners/en-US $ac_abs_top_builddir/doc/beginners/build/en-US/testsuite && mkdir -p $ac_abs_top_builddir/doc/beginners/build/en-US && ln -s $ac_abs_top_srcdir/doc/SystemTap_Beginners_Guide/en-US $ac_abs_top_builddir/doc/beginners/en-US && ln -s $ac_abs_top_srcdir/testsuite $ac_abs_top_builddir/doc/beginners/build/en-US/testsuite])

AC_OUTPUT

if test "${prefix}" = "/usr/local"; then
   AC_MSG_NOTICE([])
   AC_MSG_NOTICE([For a private or temporary build of systemtap, we recommend])
   AC_MSG_NOTICE([configuring with a prefix.  For example, try])
   AC_MSG_NOTICE([$0 $ac_configure_args --prefix=$HOME/systemtap-${PACKAGE_VERSION}-$$])
   AC_MSG_NOTICE([Running systemtap uninstalled, entirely out of the build tree,])
   AC_MSG_NOTICE([is not supported.])
fi
