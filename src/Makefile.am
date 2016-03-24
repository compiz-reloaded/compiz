AM_CPPFLAGS =			     \
	@COMPIZ_CFLAGS@		     \
	@GL_CFLAGS@		     \
	-I$(top_srcdir)/include	     \
	-I$(top_builddir)/include    \
	-DPLUGINDIR=\"$(plugindir)\" \
	-DIMAGEDIR=\"$(imagedir)\"   \
	-DMETADATADIR=\"$(metadatadir)\"

bin_PROGRAMS = compiz

compiz_LDADD = @COMPIZ_LIBS@ @GL_LIBS@ -lm -ldl
compiz_LDFLAGS = -export-dynamic -pthread
compiz_SOURCES =   \
	main.c	   \
	privates.c \
	object.c   \
	core.c	   \
	texture.c  \
	display.c  \
	screen.c   \
	window.c   \
	event.c    \
	paint.c	   \
	option.c   \
	plugin.c   \
	session.c  \
	fragment.c \
	matrix.c   \
	cursor.c   \
	match.c    \
	metadata.c

EXTRA_DIST =                     \
	compiz-start.desktop.in

if USE_MENU_ENTRIES
desktopfilesdir       = $(DESTDIR)$(datadir)/applications
desktopfiles_in_files = compiz-start.desktop.in
desktopfiles_files    = $(desktopfiles_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

install-data-local:
	if mkdir -p $(desktopfilesdir) && test -w $(desktopfilesdir); then ( \
		$(INSTALL) -m 0644 $(desktopfiles_files) \
		$(desktopfilesdir) \
	); \
	fi

uninstall-local:
	if test -w $(desktopfilesdir); then ( \
		rm -f $(desktopfilesdir)/compiz-start.desktop \
		); \
	fi

noinst_DATA =                 \
	$(desktopfiles_files)
endif
