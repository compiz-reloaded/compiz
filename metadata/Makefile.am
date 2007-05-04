xmldir       = $(metadatadir)
xml_in_files =		  \
	core.xml.in	  \
	annotate.xml.in   \
	blur.xml.in	  \
	clone.xml.in	  \
	cube.xml.in	  \
	dbus.xml.in	  \
	decoration.xml.in \
	fade.xml.in	  \
	fs.xml.in	  \
	gconf.xml.in	  \
	inotify.xml.in	  \
	minimize.xml.in	  \
	move.xml.in	  \
	place.xml.in	  \
	png.xml.in	  \
	regex.xml.in	  \
	resize.xml.in	  \
	rotate.xml.in     \
	scale.xml.in      \
	screenshot.xml.in \
	svg.xml.in        \
	switcher.xml.in   \
	video.xml.in      \
	water.xml.in      \
	wobbly.xml.in     \
	zoom.xml.in
xml_files    = $(xml_in_files:.xml.in=.xml)
xml_DATA     = $(xml_files)

@INTLTOOL_XML_RULE@

if USE_GCONF
schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = $(patsubst %.xml.in,compiz-%.schemas,$(xml_in_files))

%.schemas: $(xml_files)
	xsltproc -o $@ schemas.xslt $(subst compiz-,,$*).xml;

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA); \
        fi
endif
endif

EXTRA_DIST =		\
	$(xml_files)    \
	$(xml_in_files) \
	schemas.xslt
