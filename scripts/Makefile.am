bin_SCRIPTS = \
	compiz-mate-gtk \
	compiz-decorator-gtk

desktopfilesdir	      = $(DESTDIR)$(datadir)/applications
desktopfiles_in_files = \
	compiz-mate-gtk.desktop.in \
	gtk-decorator.desktop.in
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
		rm -f $(desktopfilesdir)/*.desktop \
		); \
	fi

noinst_DATA =		      \
	$(desktopfiles_files)

EXTRA_DIST =			 \
	$(desktopfiles_in_files) \
	compiz-mate-gtk \
	compiz-decorator-gtk

DISTCLEANFILES = \
	$(noinst_DATA)
