if USE_GNOME
libcompiz_la_LDFLAGS = -export-dynamic -avoid-version
libcompiz_la_LIBADD  = @GNOME_WINDOW_SETTINGS_LIBS@
libcompiz_la_SOURCES = 		\
	compiz-window-manager.c	\
	compiz-window-manager.h
libcompiz_module     = libcompiz.la

desktopfilesdir	      = $(windowsettingsdatadir)/gnome/wm-properties
desktopfiles_in_files = compiz.desktop.in
desktopfiles_files    = $(desktopfiles_in_files:.desktop.in=.desktop)
desktopfiles_DATA     = $(desktopfiles_files)
endif

INCLUDES = @GNOME_WINDOW_SETTINGS_CFLAGS@			   \
	-DMETACITY_THEME_DIR=\""$(windowsettingsdatadir)/themes"\"

moduledir = $(windowsettingslibdir)/window-manager-settings

module_LTLIBRARIES = \
	$(libcompiz_module)
