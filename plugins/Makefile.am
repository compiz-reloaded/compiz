libfade_la_LDFLAGS = -module -avoid-version -no-undefined
libfade_la_LIBADD = @COMPIZ_LIBS@
libfade_la_SOURCES = fade.c

libcube_la_LDFLAGS = -module -avoid-version -no-undefined
libcube_la_LIBADD = @COMPIZ_LIBS@ @LIBSVG_CAIRO_LIBS@
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

if GCONF_PLUGIN
libgconf_la_LDFLAGS = -module -avoid-version -no-undefined
libgconf_la_LIBADD = @COMPIZ_LIBS@ @GCONF_LIBS@
libgconf_la_SOURCES = gconf.c
libgconf_module = libgconf.la
endif

if PLACE_PLUGIN
libplace_la_LDFLAGS = -module -avoid-version -no-undefined
libplace_la_LIBADD = @COMPIZ_LIBS@ @PLACE_LIBS@
libplace_la_SOURCES = place.c
libplace_module = libplace.la
endif

INCLUDES =			\
	@COMPIZ_CFLAGS@		\
	@LIBSVG_CAIRO_CFLAGS@	\
	@GCONF_CFLAGS@		\
	@PLACE_CFLAGS@		\
	-I$(top_srcdir)/include

moduledir = $(plugindir)

module_LTLIBRARIES =	      \
	libfade.la	      \
	libcube.la	      \
	librotate.la	      \
	libzoom.la	      \
	libscale.la	      \
	libwobbly.la	      \
	libminimize.la	      \
	libmove.la	      \
	libresize.la	      \
	libdecoration.la      \
	libswitcher.la	      \
	libwater.la	      \
	$(libgconf_module)    \
	$(libplace_module)
