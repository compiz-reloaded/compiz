xmldir       = $(metadatadir)
xml_in_files =		   \
	core.xml.in	   \
	annotate.xml.in    \
	blur.xml.in	   \
	clone.xml.in	   \
	commands.xml.in	   \
	cube.xml.in	   \
	decoration.xml.in  \
	fade.xml.in	   \
	fs.xml.in	   \
	matecompat.xml.in \
	ini.xml.in	   \
	inotify.xml.in	   \
	minimize.xml.in	   \
	move.xml.in	   \
	obs.xml.in	   \
	place.xml.in	   \
	png.xml.in	   \
	regex.xml.in	   \
	resize.xml.in	   \
	rotate.xml.in      \
	scale.xml.in       \
	screenshot.xml.in  \
	svg.xml.in         \
	switcher.xml.in    \
	video.xml.in       \
	water.xml.in       \
	wobbly.xml.in      \
	wall.xml.in        \
	zoom.xml.in
xml_files    = $(xml_in_files:.xml.in=.xml)
xml_DATA     = $(xml_files)

core.xml.in: core.xml.in.in Makefile
	plugins=$(default_plugins); \
	if [ -z $$plugins ]; then plugins="core"; fi; \
	plugin_values=`echo $$plugins | sed 's/\([a-zA-Z0-9]*\)/<value>\1<\/value>/g' | tr ',' '\n' | sed 's/^/\t\t    /g'`; \
	$(AWK) -v aa="$$plugin_values" '{gsub("\t\t    <default_plugins/>", aa); print }' <$(srcdir)/core.xml.in.in >core.xml.in

@INTLTOOL_XML_RULE@

EXTRA_DIST =		    \
	$(xml_in_files)     \
	core.xml.in.in

DISTCLEANFILES =	    \
	$(xml_files)        \
	core.xml.in

