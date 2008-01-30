if USE_KDE4

BUILDSOURCES =		      \
	decorator.moc.cpp     \
	window.moc.cpp        \
	kwinadaptor.moc.cpp   \
	kwinadaptor.cpp       \
	kwinadaptor.h

kde4_window_decorator_DEPENDENCIES = \
	$(top_builddir)/libdecoration/libdecoration.la
kde4_window_decorator_LDADD   =			       \
	$(top_builddir)/libdecoration/libdecoration.la \
	@KDE4_WINDOW_DECORATOR_LIBS@ @KDE4_LIBS@
kde4_window_decorator_SOURCES = \
	$(BUILDSOURCES)	       \
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
	kdecoration_plugins.h

kde4_window_decorator_program = kde4-window-decorator

INCLUDES = -I$(top_srcdir)/include @KDE4_WINDOW_DECORATOR_CFLAGS@ @KDE4_CFLAGS@

bin_PROGRAMS = $(kde4_window_decorator_program)

%.moc.cpp: %.h
	$(QT4_MOC) $< > $@

kwinadaptor.h: kwinadaptor.cpp

kwinadaptor.cpp: org.kde.KWin.xml
	$(QDBUSXML2CPP) -a kwinadaptor -i decorator.h -l KWD::Decorator $<

CLEANFILES = $(BUILDSOURCES)
EXTRA_DIST = $(BUILDSOURCES)

endif
