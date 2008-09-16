if USE_KDE

BUILDSOURCES =		       \
	decorator.moc.cpp      \
	window.moc.cpp	       \
	KWinInterface_skel.cpp

kde_window_decorator_DEPENDENCIES = \
	$(top_builddir)/libdecoration/libdecoration.la
kde_window_decorator_LDADD   =			       \
	$(top_builddir)/libdecoration/libdecoration.la \
	@KDE_WINDOW_DECORATOR_LIBS@ @KDE_LIBS@
dist_kde_window_decorator_SOURCES = \
	main.cpp	       \
	utils.cpp	       \
	utils.h		       \
	decorator.cpp	       \
	decorator.h	       \
	window.cpp	       \
	window.h	       \
	options.cpp	       \
	options.h	       \
	KWinInterface.h

nodist_kde_window_decorator_SOURCES = $(BUILDSOURCES)

kde_window_decorator_program = kde-window-decorator

INCLUDES = -I$(top_srcdir)/include @KDE_WINDOW_DECORATOR_CFLAGS@ @KDE_CFLAGS@

bin_PROGRAMS = $(kde_window_decorator_program)

%.moc.cpp: %.h
	$(QT_MOC) $< > $@

%.kidl: %.h
	$(DCOPIDL) $< > $@

%_skel.cpp: %.kidl
	$(DCOPIDL2CPP) $< > $@

CLEANFILES = $(BUILDSOURCES) KWinInterface_skel.* KWinInterface_stub.*

endif
