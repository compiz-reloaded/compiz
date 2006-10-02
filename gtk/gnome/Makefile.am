moduledir = $(windowsettingslibdir)/window-manager-settings

if USE_GNOME
libcompiz_la_LDFLAGS = -export-dynamic -avoid-version -rpath $(moduledir)
libcompiz_la_LIBADD  = @GNOME_WINDOW_SETTINGS_LIBS@
libcompiz_la_SOURCES = 		\
	compiz-window-manager.c	\
	compiz-window-manager.h
libcompiz_module     = libcompiz.la

desktopfilesdir	      = $(windowsettingsdatadir)/gnome/wm-properties
desktopfiles_in_files = compiz.desktop.in
desktopfiles_files    = $(desktopfiles_in_files:.desktop.in=.desktop)
noinst_DATA           = $(desktopfiles_files)

install-exec-local:
	if test -w $(moduledir); then ( \
		$(LIBTOOL) --mode=install $(INSTALL) $(libcompiz_module) \
		$(moduledir) \
		); \
	fi
	if test -w $(desktopfilesdir); then ( \
		$(INSTALL) -m 644 $(desktopfiles_files) $(desktopfilesdir) \
		); \
	fi

endif

INCLUDES = @GNOME_WINDOW_SETTINGS_CFLAGS@			   \
	-DMETACITY_THEME_DIR=\""$(windowsettingsdatadir)/themes"\"

noinst_LTLIBRARIES = \
	$(libcompiz_module)
