desktopfiles_in_files =   \
	compiz.desktop.in

EXTRA_DIST =		         \
	$(desktopfiles_in_files)

if USE_MATE
desktopfilesdir	      = $(DESTDIR)$(datadir)/applications
desktopfiles_files    = $(desktopfiles_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

install-data-local:
	if mkdir -p $(desktopfilesdir) && test -w $(desktopfilesdir); then \
	  $(INSTALL) -m 0644 $(desktopfiles_files) $(desktopfilesdir); \
	fi

uninstall-local:
	if test -w $(desktopfilesdir); then ( \
	  for file in $(desktopfiles_files); do \
	    rm -f $(desktopfilesdir)/$$file; \
	  done; \
	fi

noinst_DATA =		      \
	$(desktopfiles_files)

CLEANFILES = 		      \
	$(desktopfiles_files)
endif
