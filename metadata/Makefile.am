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
	plugin_values=`echo $(default_plugins) | sed 's/\([[:alnum:]]*\)/<value>\1<\/value>/g' | tr ',' '\n' | sed "s/^/$$(printf '%b' '\t')$$(printf '%b' '\t')    /g"`; \
	$(AWK) 'BEGIN {repl=ARGV[1]; delete ARGV[1]}; {gsub("\t\t    <default_plugins/>", repl); print}' "$$plugin_values" <$(srcdir)/core.xml.in.in >core.xml.in

%.xml: %.xml.in
	GETTEXTDATADIR=$(top_srcdir)/gettext $(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST =		    \
	$(xml_in_files)     \
	core.xml.in.in

CLEANFILES =	     \
	$(xml_files) \
	core.xml.in
