# Makefile.am --- automake input file for systemtap tapset reference manual
## process this file with automake to produce Makefile.in


DOC_INSTALL_DIR = $(DESTDIR)$(datadir)/doc/systemtap
SBG = SystemTap_Beginners_Guide
BEGIN_INSTALL_DIR = $(DOC_INSTALL_DIR)/$(SBG)

if BUILD_DOCS
all: $(SBG)/index.html

# The beginner documents are now pre generated in the source tree

# move all the relevant files into the build directory
$(SBG).pdf:
	mkdir -p build/en-US/xml
	cp en-US/*.xml build/en-US/xml/
	cp en-US/*.ent build/en-US/xml/
	cp -R en-US/xsl build/en-US/
	cp -R en-US/extras build/en-US/xml/
	cp -R en-US/images build/en-US/xml/
	cp -R en-US/Common_Content build/en-US/xml
	cp en-US/$(SBG).pdf $(builddir)/

# rely on the pdf generation to move all the necessary files into the right dirs
$(SBG)/index.html: $(SBG).pdf
if BUILD_HTMLDOCS
	xmlto --skip-validation \
				-x build/en-US/xsl/html.xsl -o $(SBG) html build/en-US/xml/$(SBG).xml
	cp -R build/en-US/xml/images $(SBG)
	mkdir -p $(SBG)/Common_Content
	cp -R build/en-US/xml/Common_Content/images $(SBG)/Common_Content
	cp -R build/en-US/xml/Common_Content/css $(SBG)/Common_Content
endif

clean-local:
	rm -rf build/en-US/xml
	rm -rf build/en-US/xsl
	rm -f $(SBG).pdf
	rm -rf $(SBG)

install-data-hook:
	$(MKDIR_P) $(DOC_INSTALL_DIR)
	$(INSTALL_DATA) $(SBG).pdf $(DOC_INSTALL_DIR)
if BUILD_HTMLDOCS
	$(MKDIR_P) $(DOC_INSTALL_DIR)/$(SBG)
	$(INSTALL_DATA) $(SBG)/*.html $(DOC_INSTALL_DIR)/$(SBG)
	$(MKDIR_P) $(DOC_INSTALL_DIR)/$(SBG)/images
	$(INSTALL_DATA) $(SBG)/images/*.png $(SBG)/images/*.svg \
			$(DOC_INSTALL_DIR)/$(SBG)/images
	$(MKDIR_P) $(DOC_INSTALL_DIR)/$(SBG)/Common_Content/css
	$(MKDIR_P) $(DOC_INSTALL_DIR)/$(SBG)/Common_Content/images
	$(INSTALL_DATA) $(SBG)/Common_Content/css/*.css \
			$(DOC_INSTALL_DIR)/$(SBG)/Common_Content/css
	$(INSTALL_DATA) $(SBG)/Common_Content/images/*.png \
			$(SBG)/Common_Content/images/*.svg \
			$(DOC_INSTALL_DIR)/$(SBG)/Common_Content/images
	$(MKDIR_P) $(DOC_INSTALL_DIR)/$(SBG)/xsl
	$(INSTALL_DATA) en-US/xsl/*.xsl \
			$(DOC_INSTALL_DIR)/$(SBG)/xsl
endif

uninstall-local:
	rm -f $(DOC_INSTALL_DIR)/$(SBG).pdf
	rm -rf $(DOC_INSTALL_DIR)/$(SBG)
endif
