xmldir       = $(metadatadir)

if ANNOTATE_PLUGIN
annotate_in = annotate.xml.in
endif

if FUSE_PLUGIN
fs_in = fs.xml.in
endif

if INOTIFY_PLUGIN
inotify_in = inotify.xml.in
endif

if USE_GLIB
glib_in = glib.xml.in
endif

if USE_LIBRSVG
svg_in = svg.xml.in
endif

if USE_MATE
matecompat_in = matecompat.xml.in
endif

if DBUS_PLUGIN
dbus_in = dbus.xml.in
endif

xml_in_files =		   \
	core.xml.in	   \
	$(annotate_in)	   \
	blur.xml.in	   \
	clone.xml.in	   \
	commands.xml.in	   \
	cube.xml.in	   \
	$(dbus_in)	   \
	decoration.xml.in  \
	fade.xml.in	   \
	$(fs_in)	   \
	$(glib_in)	   \
	$(matecompat_in)   \
	ini.xml.in	   \
	$(inotify_in)	   \
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
	$(svg_in)          \
	switcher.xml.in    \
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

