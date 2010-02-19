if USE_KDE4

BUILDSOURCES =		      \
	decorator.moc.cpp     \
	window.moc.cpp        \
	paintredirector.moc.cpp \
	kwinadaptor.moc.cpp   \
	kwinadaptor.cpp       \
	kwinadaptor.h

kde4_window_decorator_DEPENDENCIES = \
	$(top_builddir)/libdecoration/libdecoration.la
kde4_window_decorator_LDADD   =			       \
	$(top_builddir)/libdecoration/libdecoration.la \
	@KDE4_WINDOW_DECORATOR_LIBS@ @KDE4_LIBS@ -ldl

nodist_kde4_window_decorator_SOURCES = \
	$(BUILDSOURCES)

dist_kde4_window_decorator_SOURCES = \
	main.cpp	       \
	utils.cpp	       \
	utils.h		       \
	decorator.cpp	       \
	decorator.h	       \
	window.cpp	       \
	window.h	       \
	options.cpp	       \
	options.h              \
	kdecoration_plugins.cpp \
	kdecoration_plugins.h   \
	switcher.cpp            \
	switcher.h              \
	paintredirector.cpp     \
	paintredirector.h       \
	org.kde.KWin.xml

kde4_window_decorator_program = kde4-window-decorator

INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include @KDE4_WINDOW_DECORATOR_CFLAGS@ @KDE4_CFLAGS@

bin_PROGRAMS = $(kde4_window_decorator_program)

%.moc.cpp: %.h
	$(QT4_MOC) $< > $@

kwinadaptor.h: kwinadaptor.cpp

kwinadaptor.cpp: org.kde.KWin.xml
	$(QDBUSXML2CPP) -a kwinadaptor -i decorator.h -l KWD::Decorator $<

# make sure kwinadaptor.h is built before any of the dist_SOURCES uses it
main.cpp: kwinadaptor.h

CLEANFILES = $(BUILDSOURCES)
EXTRA_DIST = org.kde.KWin.xml

endif
