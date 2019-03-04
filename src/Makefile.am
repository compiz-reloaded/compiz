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

desktopfiles_in_files =         \
	compiz-start.desktop.in

EXTRA_DIST =                     \
	$(desktopfiles_in_files)

if USE_MENU_ENTRIES
desktopfilesdir       = $(DESTDIR)$(datadir)/applications
desktopfiles_files    = $(desktopfiles_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

install-data-local:
	if mkdir -p $(desktopfilesdir) && test -w $(desktopfilesdir); then \
	  $(INSTALL) -m 0644 $(desktopfiles_files) $(desktopfilesdir); \
	fi

uninstall-local:
	if test -w $(desktopfilesdir); then \
	  for file in $(desktopfiles_files); do \
	    rm -f $(desktopfilesdir)/$$file; \
	  done; \
	fi

noinst_DATA =                 \
	$(desktopfiles_files)

CLEANFILES =                  \
	$(desktopfiles_files)
endif
