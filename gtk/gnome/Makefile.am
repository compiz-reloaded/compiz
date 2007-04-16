moduledir = $(DESTDIR)$(windowsettingslibdir)/window-manager-settings

if USE_GNOME
libcompiz_la_LDFLAGS = -export-dynamic -avoid-version -rpath $(moduledir)
libcompiz_la_LIBADD  = @GNOME_WINDOW_SETTINGS_LIBS@
libcompiz_la_SOURCES = 		\
	compiz-window-manager.c	\
	compiz-window-manager.h
libcompiz_module     = libcompiz.la

desktopfilesdir	      = $(DESTDIR)$(windowsettingsdatadir)/gnome/wm-properties
desktopfiles_in_files = compiz.desktop.in
desktopfiles_files    = $(desktopfiles_in_files:.desktop.in=.desktop)
desktopfiles_DATA     = $(desktopfiles_files)

@INTLTOOL_DESKTOP_RULE@

xmldir       = $(datadir)/gnome-control-center/keybindings
xml_in_files = 50-compiz-desktop-key.xml.in 50-compiz-key.xml.in
xml_files    = $(xml_in_files:.xml.in=.xml)
xml_DATA     = $(xml_files)

@INTLTOOL_XML_NOMERGE_RULE@

install-exec-local:
	if mkdir -p $(moduledir) && test -w $(moduledir); then ( \
		$(LIBTOOL) --mode=install $(INSTALL) $(libcompiz_module) \
		$(moduledir) \
		); \
	fi

install-data-local:
	if mkdir -p $(desktopfilesdir) && test -w $(desktopfilesdir); then ( \
		$(INSTALL) -m 644 $(desktopfiles_files) \
		$(desktopfilesdir) \
		); \
	fi

uninstall-local:
	if test -w $(moduledir); then ( \
		rm -f $(moduledir)/libcompiz.* \
		); \
	fi
	if test -w $(desktopfilesdir); then ( \
		rm -f $(desktopfilesdir)/compiz.desktop \
		); \
	fi

endif

INCLUDES = @GNOME_WINDOW_SETTINGS_CFLAGS@			   \
	-DMETACITY_THEME_DIR=\""$(windowsettingsdatadir)/themes"\"

noinst_LTLIBRARIES = \
	$(libcompiz_module)

EXTRA_DIST =			 \
	$(desktopfiles_files) 	 \
	$(desktopfiles_in_files) \
	$(xml_files)		 \
	$(xml_in_files)
