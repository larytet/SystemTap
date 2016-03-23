# Makefile.am --- automake input file for systemtap tapset reference manual
## process this file with automake to produce Makefile.in


DOC_INSTALL_DIR = $(DESTDIR)$(datadir)/doc/systemtap
MAN_INSTALL_DIR = $(DESTDIR)$(mandir)/man3
HTML_INSTALL_DIR = $(DESTDIR)$(datadir)/doc/systemtap/tapsets

if XMLTO_STRINGPARAM
XMLTOHTMLPARAMS=--stringparam use.id.as.filename=1 -m $(abs_srcdir)/generate-toc.xml
endif

if BUILD_REFDOCS
# The tapset documents are now pre generated in the source tree
if BUILD_HTMLDOCS
noinst_PROGRAMS = docproc
SRCTREE=$(abs_top_srcdir)/
DOCPROC=$(abs_builddir)/docproc
endif

all: stamp-htmldocs
tapsets.xml: docproc $(shell find $(SRCTREE)/tapset -name '*.stp')
if BUILD_HTMLDOCS
	sed -e '/^!Syscalls/{r $(abs_srcdir)/syscalls.xmlpart' -e 'd}' $(abs_srcdir)/tapsets.tmpl > tapsets.tmpl.new
	SRCTREE=$(SRCTREE) $(DOCPROC) doc tapsets.tmpl.new > tapsets.xml.new
	python $(srcdir)/overload.py tapsets.xml.new > tapsets.xml.new1
	xsltproc $(srcdir)/sort-tapsets.xslt tapsets.xml.new1 > tapsets.xml.new2
	rm tapsets.xml.new tapsets.xml.new1 tapsets.tmpl.new
	if test -s tapsets.xml && cmp tapsets.xml.new2 tapsets.xml >/dev/null ; then \
		echo tapsets.xml unchanged; \
		rm tapsets.xml.new2; \
	else \
		mv tapsets.xml.new2 tapsets.xml; \
	fi
endif

stamp-htmldocs: tapsets.xml
if BUILD_HTMLDOCS
	xmlto $(XMLTOHTMLPARAMS) html -o tapsets tapsets.xml
	touch stamp-htmldocs
endif

#FIXME need to figure out where to install things appropriately
#installmandocs: mandocs
install-data-hook:
	$(MKDIR_P)  $(DOC_INSTALL_DIR)
	cp $(abs_srcdir)/tapsets.pdf $(DOC_INSTALL_DIR)
	$(MKDIR_P)  $(MAN_INSTALL_DIR)
	cp -R $(abs_srcdir)/man3/* $(MAN_INSTALL_DIR)
	cp -R $(abs_srcdir)/man_pages/* $(MAN_INSTALL_DIR) # manpager output
if BUILD_HTMLDOCS
	$(MKDIR_P)  $(HTML_INSTALL_DIR)
	$(INSTALL_DATA) tapsets/* $(HTML_INSTALL_DIR)
endif
endif

CLEANFILES=stamp-* tapsets.pdf tapsets.xml
clean-local:
	rm -rf tapsets
