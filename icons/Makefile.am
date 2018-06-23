iconsdir = $(pluginiconsdir)/scalable/apps
icons_DATA =			\
	plugin-core.svg		\
	plugin-annotate.svg	\
	plugin-blur.svg		\
	plugin-clone.svg	\
	plugin-commands.svg	\
	plugin-cube.svg		\
	plugin-dbus.svg		\
	plugin-decoration.svg	\
	plugin-fade.svg		\
	plugin-fs.svg		\
	plugin-glib.svg		\
	plugin-matecompat.svg	\
	plugin-inotify.svg	\
	plugin-minimize.svg	\
	plugin-move.svg		\
	plugin-obs.svg		\
	plugin-place.svg	\
	plugin-png.svg		\
	plugin-regex.svg	\
	plugin-resize.svg	\
	plugin-rotate.svg	\
	plugin-scale.svg	\
	plugin-screenshot.svg	\
	plugin-svg.svg		\
	plugin-switcher.svg	\
	plugin-water.svg	\
	plugin-wobbly.svg	\
	plugin-wall.svg		\
	plugin-zoom.svg

install-data-hook:
	$(shell which gtk-update-icon-cache &> /dev/null &&	\
	gtk-update-icon-cache -f -t $(DESTDIR)$(pluginiconsdir))
