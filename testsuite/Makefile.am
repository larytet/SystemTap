# Makefile.am --- automake input file for systemtap testsuite
## process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = dejagnu no-dist subdir-objects

all-local:
	@echo Run \"make check\" or \"make installcheck\".
	@if test -n "$(DEJAZILLA)"; then echo Test results will be emailed to $(DEJAZILLA); fi

clean-local:
	-rm -f ./stap site.exp systemtap.sum systemtap.log
	-rm -f systemtap.temp*
	-rm -rf .systemtap* .cache_test* 2>/dev/null
	-rm -f ./stap_*.ko
	-rm -f flightrec*
	-rm -f *.so
	-rm -f uprobe_*
	-rm -rf artifacts

DEJAZILLA=@dejazilla@
TESTAPPS=@enable_testapps@
TOOL_OPTS=

# The SYSTEMTAP_TESTSUITE_RESUME environmental variable will turn on the
# "resume mode" of the systemtap testsuite if set to some non-zero value.  In
# this mode, tests, that have already been run, are not being re-executed, and
# their existing test results are being reused and merged instead.  This can be
# useful e.g. when some of the tests causes kernel panic or stall.  Using the
# "resume mode", it is possible to reboot the testing box, and then re-run the
# testsuite skipping already executed tests.  In this case, the problematic
# testcase is left in an "unfinished" state. See "make list-unfinished".  In
# case the testsuite was running in the parallel testing mode, more than one
# test will be left unfinished of course.  Unfinished tests need separate
# investigation.  The "resume mode" can be used with "make installcheck", or
# "make installcheck-parallel" targets.
SKIPTESTS=--ignore \'$(shell \
	test -z $(SYSTEMTAP_TESTSUITE_RESUME) || \
	  grep -r 'Running.*testsuite.*exp\ ...' * 2>/dev/null |\
	  grep -o '[^\/]*\.exp' |\
	  sort -u |\
	  tr '\n' ' ' |\
	  sed 's/\ $$/\n/' \
	)\'

# automake's dejagnu library already runs check-DEJAGNU before check-local
# That's why we need to add "execrc" to $(RUNTEST) - to ensure that this
# subtarget gets run even if runtest per se exits with a failure.
check-local:
	if test -n "$(DEJAZILLA)"; then mail $(DEJAZILLA) < systemtap.sum; fi

# but installcheck does not follow an implicit check-DEJAGNU, go figure
installcheck: site.exp
	-rm -rf artifacts
	-test -z $(SYSTEMTAP_TESTSUITE_RESUME) && $(MAKE) clean
	-mv systemtap.log systemtap.temp1.log 2>/dev/null
	-mv systemtap.sum systemtap.temp1.sum 2>/dev/null
	-rmmod uprobes 2>/dev/null
	-$(MAKE) $(AM_MAKEFLAGS) check-DEJAGNU RUNTESTFLAGS="$(RUNTESTFLAGS) $(SKIPTESTS) --tool_opts \'install $(TOOL_OPTS)\'"
	mv systemtap.log systemtap.temp2.log
	mv systemtap.sum systemtap.temp2.sum
	$(SHELL) $(srcdir)/dg-extract-results.sh `find . -name 'systemtap.*sum' -print` > systemtap.sum
	$(SHELL) $(srcdir)/dg-extract-results.sh -L `find . -name 'systemtap.*log' -print` > systemtap.log
	rm -f systemtap.temp*
	tail systemtap.sum
	if test -n "$(DEJAZILLA)"; then mail $(DEJAZILLA) < systemtap.sum; fi


# In the parallel mode, the dg-extract-results.sh generates common .sum and .log
# files for all the tests, plus separate logs per testcase are kept in the artifacts
# directory. Serial tests (see below) log to artifacts/__serialtests.
installcheck-parallel: site.exp
	-test -z $(SYSTEMTAP_TESTSUITE_RESUME) && $(MAKE) clean
	-rmmod uprobes 2>/dev/null
	mkdir -p artifacts/__serialtests
	MAKEFLAGS= $(RUNTEST2) systemtap.base/environment_sanity.exp
	-$(MAKE) -k do-installcheck-parallel; \
	result=$$?; \
	$(SHELL) $(srcdir)/dg-extract-results.sh \
	`find artifacts -name systemtap.sum -print` > systemtap.sum; \
	$(SHELL) $(srcdir)/dg-extract-results.sh -L \
	`find artifacts -name systemtap.log -print` > systemtap.log; \
	tail systemtap.sum
	exit $$result

# TESTS exists for the user to pass on the command line to easily
# say "Only run these tests."  With check or installcheck it's not necessary, but
# with installcheck-parallel there's no other way to (easily) specify a subset
# of tests. To specify all tests in a subdirectory, use TESTS=systemtap.subdir/*.exp.
# E.g., make check TESTS="gdb.server/*.exp gdb.threads/*.exp".
expanded_tests := $(patsubst $(srcdir)/%,%,$(wildcard $(addprefix $(srcdir)/,$(TESTS))))

# Following are all tests we want to run within the installcheck-parallel target.
# Those are either specified via the TESTS env var, or all the available tests.
all_tests := $(or $(expanded_tests),$(shell cd $(srcdir) && find systemtap.* -name '*.exp' -print))

# Tests that need to run serially. This namely applies to systemtap.server tests.
# They start a new systemtap server and only expect one local server to run.
all_serial_tests := $(shell cd $(srcdir) && find systemtap.server -name '*.exp' -print)
serial_tests := $(filter $(all_tests),$(all_serial_tests))

# There are a few tests known to be slow. We run them right from the beginning
# to try to lessen the overall time taken by the test suite -- if one of these
# tests happens to be run late, it will cause the overall time to increase.
all_slow_tests = systemtap.examples/check.exp systemtap.pass1-4/buildok.exp \
	systemtap.base/listing_mode.exp systemtap.syscall/nd_syscall.exp \
	systemtap.unprivileged/unprivileged_myproc.exp systemtap.syscall/syscall.exp \
	systemtap.base/sdt_misc.exp systemtap.pass1-4/parseok.exp \
	systemtap.base/listing_mode_sanity.exp systemtap.pass1-4/parse-semok.exp \
	systemtap.pass1-4/semok.exp systemtap.unprivileged/unprivileged_probes.exp \
	systemtap.pass1-4/buildok-interactive.exp \
	systemtap.onthefly/uprobes_onthefly.exp \
	systemtap.onthefly/kprobes_onthefly.exp systemtap.base/sdt.exp \
	systemtap.onthefly/hrtimer_onthefly.exp \
	systemtap.onthefly/tracepoint_onthefly.exp systemtap.exelib/exelib.exp \
	systemtap.pass1-4/semko.exp systemtap.pass1-4/parseko.exp \
	systemtap.base/rlimit.exp systemtap.base/tracepoints.exp \
	systemtap.base/systemtap-service.exp systemtap.maps/map_hash.exp \
	systemtap.base/stmt_rel.exp systemtap.base/library.exp
slow_tests = $(filter $(all_tests),$(all_slow_tests))

regular_tests := $(filter-out $(serial_tests) $(slow_tests) ,$(all_tests))

REGULAR_TEST_TARGETS := $(addprefix installcheck/,$(slow_tests) $(regular_tests))

do-installcheck-parallel: installcheck-serial-tests $(REGULAR_TEST_TARGETS)
	@:

installcheck-serial-tests: site.exp
	-mv artifacts/__serialtests/systemtap.log artifacts/__serialtests/systemtap.temp1.log 2>/dev/null
	-mv artifacts/__serialtests/systemtap.sum artifacts/__serialtests/systemtap.temp1.sum 2>/dev/null
	test -z $(serial_tests) || MAKEFLAGS= $(MAKE) $(AM_MAKEFLAGS) check-DEJAGNU RUNTESTFLAGS="--outdir=artifacts/__serialtests $(serial_tests) $(RUNTESTFLAGS) $(SKIPTESTS) --tool_opts \'install $(TOOL_OPTS)\'" STAP_PARALLEL=yes
	mv artifacts/__serialtests/systemtap.log artifacts/__serialtests/systemtap.temp2.log
	mv artifacts/__serialtests/systemtap.sum artifacts/__serialtests/systemtap.temp2.sum
	$(SHELL) $(srcdir)/dg-extract-results.sh `find ./artifacts/__serialtests -name 'systemtap.*sum' -print` > artifacts/__serialtests/systemtap.sum
	$(SHELL) $(srcdir)/dg-extract-results.sh -L `find ./artifacts/__serialtests -name 'systemtap.*log' -print` > artifacts/__serialtests/systemtap.log
	rm -f artifacts/__serialtests/systemtap.temp*


installcheck/%.exp: site.exp
	if test -e artifacts/$* ; then \
	echo "Using existing test result for $*.exp"; \
	else \
	mkdir -p artifacts/$* ; \
	MAKEFLAGS= $(MAKE) $(AM_MAKEFLAGS) check-DEJAGNU RUNTESTFLAGS="--outdir=artifacts/$* $*.exp $(RUNTESTFLAGS) --tool_opts \'install $(TOOL_OPTS)\'" STAP_PARALLEL=yes; \
	fi

# In case a test caused kernel panic or stall, followed by the test box reboot
# and testsuite re-run, it will stay unfinished.  All other testcases that might
# have been running in parallel with it, are left unfinished too.  After the the
# last testsuite re-run, after all the testcases have been executed, unfinished
# testcases should be listed and investigated manually.  The "make clean" target
# cleans whole the table entirely.
list-unfinished:
	@awk '/Running.*testsuite.*exp .../ {tests[$$2]++}  /testcase.*testsuite.*completed in.*seconds/ {tests[$$2]++} END {for (key in tests) if (tests[key] != 2) { print key }}' systemtap.log

# $(srcdir)/These values point the test suite to the install tree, and
# are overridden by "make check" from full source/build tree
SYSTEMTAP_RUNTIME=$(DESTDIR)$(pkgdatadir)/runtime
SYSTEMTAP_TAPSET=$(DESTDIR)$(pkgdatadir)/tapset
LD_LIBRARY_PATH=$(DESTDIR)$(libdir)/systemtap
CRASH_LIBDIR=$(DESTDIR)$(libdir)/systemtap
SYSTEMTAP_PATH=$(DESTDIR)$(bindir)
SYSTEMTAP_INCLUDES=$(DESTDIR)$(includedir)

RUNTESTDEFAULTFLAGS = --tool $$tool --tool_opts \'$(TOOL_OPTS)\' --srcdir $(shell readlink -f $(srcdir))
EXPECT = expect
RUNTEST= env XDG_DATA_DIRS= SYSTEMTAP_SYNC=1 LANG=C SYSTEMTAP_TESTREMOTES=$(TESTREMOTES) SYSTEMTAP_TESTAPPS=$(TESTAPPS) SYSTEMTAP_RUNTIME=$(SYSTEMTAP_RUNTIME) SYSTEMTAP_TAPSET=$(SYSTEMTAP_TAPSET) LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) CRASH_LIBDIR=$(CRASH_LIBDIR) PATH=$(SYSTEMTAP_PATH):$$PATH SYSTEMTAP_PATH=$(SYSTEMTAP_PATH) SYSTEMTAP_INCLUDES=$(SYSTEMTAP_INCLUDES)  PKGLIBDIR=$(pkglibexecdir) $(srcdir)/execrc runtest
RUNTEST2= env XDG_DATA_DIRS= SYSTEMTAP_SYNC=1 LANG=C SYSTEMTAP_TESTREMOTES=$(TESTREMOTES) SYSTEMTAP_TESTAPPS=$(TESTAPPS) SYSTEMTAP_RUNTIME=$(SYSTEMTAP_RUNTIME) SYSTEMTAP_TAPSET=$(SYSTEMTAP_TAPSET) LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) CRASH_LIBDIR=$(CRASH_LIBDIR) PATH=$(SYSTEMTAP_PATH):$$PATH SYSTEMTAP_PATH=$(SYSTEMTAP_PATH) SYSTEMTAP_INCLUDES=$(SYSTEMTAP_INCLUDES)  PKGLIBDIR=$(pkglibexecdir) runtest
