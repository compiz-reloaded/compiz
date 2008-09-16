if USE_GTK

if USE_METACITY
metacitylibs = @METACITY_LIBS@
endif

gtk_window_decorator_DEPENDENCIES = \
	$(top_builddir)/libdecoration/libdecoration.la
gtk_window_decorator_LDADD	  =		       \
	$(top_builddir)/libdecoration/libdecoration.la \
	@GTK_WINDOW_DECORATOR_LIBS@		       \
	@GCONF_LIBS@				       \
	@DBUS_GLIB_LIBS@			       \
	$(metacitylibs)

gtk_window_decorator_SOURCES	  = \
	gtk-window-decorator.c
gtk_window_decorator_program = gtk-window-decorator
endif

if USE_GCONF
schemadir       = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = gwd.schemas.in
schema_files    = $(schema_in_files:.schemas.in=.schemas)
schema_DATA     = $(schema_files)

@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA); \
	fi
endif

INCLUDES = \
	-I$(top_srcdir)/include            \
	@GTK_WINDOW_DECORATOR_CFLAGS@      \
	@METACITY_CFLAGS@                  \
	@GCONF_CFLAGS@                     \
	@DBUS_GLIB_CFLAGS@                 \
	-DALL_LINGUAS="\"@ALL_LINGUAS@\""  \
	-DLOCALEDIR="\"@datadir@/locale"\"

bin_PROGRAMS = $(gtk_window_decorator_program)

EXTRA_DIST =		\
	$(schema_in_files)

DISTCLEANFILES = \
	$(schema_files)
