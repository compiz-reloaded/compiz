xmldir       = $(metadatadir)
xml_in_files =		  \
	compiz.xml.in	  \
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
	resize.xml.in     \
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

EXTRA_DIST =		\
	$(xml_files)    \
	$(xml_in_files)
