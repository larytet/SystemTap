mandir = @mandir@/cs

man_MANS = stapprobes.3stap stapfuncs.3stap stapvars.3stap stapex.3stap \
	dtrace.1 stap-merge.1 stappaths.7 stapsh.8 systemtap.8 stapref.1

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(man7dir)
	for f in $(srcdir)/*.7stap; do \
		$(INSTALL_DATA) "$$f" "$(DESTDIR)$(man7dir)"; \
	done

if BUILD_TRANSLATOR
man_MANS += stap.1 stap-prep.1 stap-report.1
endif

if HAVE_NSS
if BUILD_SERVER
man_MANS += stap-server.8
endif
endif

if BUILD_VIRT
man_MANS += stapvirt.1
endif
