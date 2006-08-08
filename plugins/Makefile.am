libfade_la_LDFLAGS = -module -avoid-version -no-undefined
libfade_la_LIBADD = @COMPIZ_LIBS@
libfade_la_SOURCES = fade.c

libcube_la_LDFLAGS = -module -avoid-version -no-undefined
libcube_la_LIBADD = @COMPIZ_LIBS@ @LIBRSVG_LIBS@
libcube_la_SOURCES = cube.c

librotate_la_LDFLAGS = -module -avoid-version -no-undefined
librotate_la_LIBADD = @COMPIZ_LIBS@
librotate_la_SOURCES = rotate.c

libzoom_la_LDFLAGS = -module -avoid-version -no-undefined
libzoom_la_LIBADD = @COMPIZ_LIBS@
libzoom_la_SOURCES = zoom.c

libscale_la_LDFLAGS = -module -avoid-version -no-undefined
libscale_la_LIBADD = @COMPIZ_LIBS@
libscale_la_SOURCES = scale.c

libwobbly_la_LDFLAGS = -module -avoid-version -no-undefined
libwobbly_la_LIBADD = @COMPIZ_LIBS@
libwobbly_la_SOURCES = wobbly.c

libminimize_la_LDFLAGS = -module -avoid-version -no-undefined
libminimize_la_LIBADD = @COMPIZ_LIBS@
libminimize_la_SOURCES = minimize.c

libmove_la_LDFLAGS = -module -avoid-version -no-undefined
libmove_la_LIBADD = @COMPIZ_LIBS@
libmove_la_SOURCES = move.c

libresize_la_LDFLAGS = -module -avoid-version -no-undefined
libresize_la_LIBADD = @COMPIZ_LIBS@
libresize_la_SOURCES = resize.c

libdecoration_la_LDFLAGS = -module -avoid-version -no-undefined
libdecoration_la_LIBADD = @COMPIZ_LIBS@
libdecoration_la_SOURCES = decoration.c

libswitcher_la_LDFLAGS = -module -avoid-version -no-undefined
libswitcher_la_LIBADD = @COMPIZ_LIBS@
libswitcher_la_SOURCES = switcher.c

libwater_la_LDFLAGS = -module -avoid-version -no-undefined
libwater_la_LIBADD = @COMPIZ_LIBS@
libwater_la_SOURCES = water.c

libscreenshot_la_LDFLAGS = -module -avoid-version -no-undefined
libscreenshot_la_LIBADD = @COMPIZ_LIBS@
libscreenshot_la_SOURCES = screenshot.c

if GCONF_PLUGIN
libgconf_la_LDFLAGS = -module -avoid-version -no-undefined
libgconf_la_LIBADD = @COMPIZ_LIBS@ @GCONF_LIBS@
libgconf_la_SOURCES =	     \
	gconf.c		     \
	gconf-compiz-utils.c \
	gconf-compiz-utils.h
libgconf_module = libgconf.la

if GCONF_DUMP_PLUGIN
libgconf_dump_la_LDFLAGS = -module -avoid-version -no-undefined
libgconf_dump_la_LIBADD = @COMPIZ_LIBS@ @GCONF_LIBS@
libgconf_dump_la_SOURCES =   \
	gconf-dump.c	     \
	gconf-compiz-utils.c \
	gconf-compiz-utils.h
libgconf_dump_module = libgconf-dump.la
endif

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = compiz.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

schema_modules = $(patsubst gconf%,,$(subst lib,,$(subst .la,,$(module_LTLIBRARIES))))

compiz.schemas.in:
	COMPIZ_SCHEMA_PLUGINS="$(schema_modules)" COMPIZ_SCHEMA_GENERAL=1 \
	compiz --replace $(schema_modules) gconf-dump &&		  \
	sed -e 's/\[TopRight\]/[\@SCALE_CORNER\@]/'			  \
	  < compiz.schemas.dump > compiz.schemas.in;			  \
	rm -f compiz.schemas.dump

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA); \
        fi
endif
endif

if PLACE_PLUGIN
libplace_la_LDFLAGS = -module -avoid-version -no-undefined
libplace_la_LIBADD = @COMPIZ_LIBS@ @PLACE_LIBS@
libplace_la_SOURCES = place.c
libplace_module = libplace.la
endif

if DBUS_PLUGIN
libdbus_la_LDFLAGS = -module -avoid-version -no-undefined
libdbus_la_LIBADD = @COMPIZ_LIBS@ @DBUS_LIBS@
libdbus_la_SOURCES = dbus.c
libdbus_module = libdbus.la
endif

INCLUDES =					\
	@COMPIZ_CFLAGS@				\
	@LIBRSVG_CFLAGS@			\
	@GCONF_CFLAGS@				\
	@PLACE_CFLAGS@				\
	@DBUS_CFLAGS@				\
	-DALL_LINGUAS="\"@ALL_LINGUAS@\""	\
	-DLOCALEDIR="\"@datadir@/locale\""	\
	-I$(top_srcdir)/include

moduledir = $(plugindir)

# NOTE: the order here determines the default active_plugins order in
# compiz.schemas
module_LTLIBRARIES =		\
	$(libgconf_module)	\
	libdecoration.la	\
	libwobbly.la		\
	libfade.la		\
	libminimize.la		\
	libcube.la		\
	librotate.la		\
	libzoom.la		\
	libscale.la		\
	libmove.la		\
	libresize.la		\
	$(libplace_module)	\
	$(libdbus_module)	\
	libswitcher.la		\
	libwater.la		\
	libscreenshot.la	\
	$(libgconf_dump_module)

EXTRA_DIST = $(schema_in_files)
