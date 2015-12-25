if USE_GTK

if USE_MARCO
marcolibs = @MARCO_LIBS@
endif

gtk_window_decorator_DEPENDENCIES = \
	$(top_builddir)/libdecoration/libdecoration.la
gtk_window_decorator_LDADD	  =		       \
	$(top_builddir)/libdecoration/libdecoration.la \
	@GTK_WINDOW_DECORATOR_LIBS@		       \
	@GSETTINGS_LIBS@			       \
	@DBUS_GLIB_LIBS@			       \
	$(marcolibs)

gtk_window_decorator_SOURCES	  = \
	gtk-window-decorator.c
gtk_window_decorator_program = gtk-window-decorator
endif

if USE_GSETTINGS
gsettings_SCHEMAS = org.compiz-0.gwd.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(gsettings_SCHEMAS)
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/include            \
	@GTK_WINDOW_DECORATOR_CFLAGS@      \
	@MARCO_CFLAGS@                     \
	@GSETTINGS_CFLAGS@                 \
	@DBUS_GLIB_CFLAGS@                 \
	-DALL_LINGUAS="\"@ALL_LINGUAS@\""  \
	-DLOCALEDIR="\"@datadir@/locale"\"

bin_PROGRAMS = $(gtk_window_decorator_program)
