if USE_GTK
desktopfilesdir	      = $(DESTDIR)$(datadir)/applications
desktopfiles_in_files = compiz.desktop.in
desktopfiles_files    = $(desktopfiles_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

install-data-local:
	if mkdir -p $(desktopfilesdir) && test -w $(desktopfilesdir); then ( \
		$(INSTALL) -m 644 $(desktopfiles_files) \
		$(desktopfilesdir) \
		); \
	fi

uninstall-local:
	if test -w $(desktopfilesdir); then ( \
		rm -f $(desktopfilesdir)/compiz.desktop \
		); \
	fi
endif

noinst_DATA =		      \
	$(desktopfiles_files)

EXTRA_DIST =			 \
	$(desktopfiles_in_files)

DISTCLEANFILES = \
	$(noinst_DATA)
