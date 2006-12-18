if USE_KDE
kde_window_decorator_DEPENDENCIES = \
	$(top_builddir)/libdecoration/libdecoration.la
kde_window_decorator_LDADD   =			       \
	$(top_builddir)/libdecoration/libdecoration.la \
	@KDE_WINDOW_DECORATOR_LIBS@ @KDE_LIBS@
kde_window_decorator_SOURCES = \
	main.cpp	       \
	utils.cpp	       \
	utils.h		       \
	decorator.cpp	       \
	decorator.h	       \
	window.cpp	       \
	window.h	       \
	options.cpp	       \
	options.h	       \
	KWinInterface_skel.cpp \
	KWinInterface.h

kde_window_decorator_program = kde-window-decorator

INCLUDES = -I$(top_srcdir)/include @KDE_WINDOW_DECORATOR_CFLAGS@ @KDE_CFLAGS@

bin_PROGRAMS = $(kde_window_decorator_program)

%.moc: %.h
	$(QT_MOC) $< > $@

%.kidl: ./KWinInterface.h
	$(DCOPIDL) $< > $@

%skel.cpp: ./KWinInterface.kidl
	$(DCOPIDL2CPP) $< > $@

EXTRA_DIST = window.moc decorator.moc KWinInterface_skel.cpp

endif
