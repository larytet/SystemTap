# Makefile.am --- automake input file for systemtap tapset reference manual
## process this file with automake to produce Makefile.in


DOC_INSTALL_DIR = $(DESTDIR)$(datadir)/doc/systemtap
MAN_INSTALL_DIR = $(DESTDIR)$(mandir)/man3
HTML_INSTALL_DIR = $(DESTDIR)$(datadir)/doc/systemtap/tapsets

if BUILD_REFDOCS
# The tapset documents are now pre generated in the source tree

all: stamp-pdfdoc stamp-htmldocs stamp-mandocs

stamp-pdfdoc:
	cp $(abs_srcdir)/tapsets.pdf $(builddir)/
	touch stamp-pdfdoc

stamp-htmldocs: tapsets.pdf
	cp -R $(abs_srcdir)/tapsets tapsets
	touch stamp-htmldocs

stamp-mandocs:
	cp -R $(abs_srcdir)/man3 man3
	cp -R $(abs_srcdir)/man_pages man_pages
	touch stamp-mandocs

#FIXME need to figure out where to install things appropriately
#installmandocs: mandocs
install-data-hook:
	$(MKDIR_P)  $(DOC_INSTALL_DIR)
	$(INSTALL_DATA) tapsets.pdf $(DOC_INSTALL_DIR)
	$(MKDIR_P)  $(MAN_INSTALL_DIR)
	$(INSTALL_DATA) man3/* $(MAN_INSTALL_DIR)
	$(INSTALL_DATA) man_pages/* $(MAN_INSTALL_DIR) # manpager output
	$(MKDIR_P)  $(HTML_INSTALL_DIR)
	$(INSTALL_DATA) tapsets/* $(HTML_INSTALL_DIR)
endif

CLEANFILES=stamp-* tapsets.pdf
clean-local:
	rm -rf man3 man_pages tapsets
