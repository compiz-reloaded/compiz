xmldir       = $(metadatadir)
xml_in_files =		\
	compiz.xml.in	\
	annotate.xml.in \
	blur.xml.in	\
	clone.xml.in	\
	cube.xml.in	\
	resize.xml.in
xml_files    = $(xml_in_files:.xml.in=.xml)
xml_DATA     = $(xml_files)

@INTLTOOL_XML_RULE@

EXTRA_DIST =		\
	$(xml_files)    \
	$(xml_in_files)
