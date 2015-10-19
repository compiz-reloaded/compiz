if USE_GTK

if USE_MARCO
marcolibs = @MARCO_LIBS@
endif

gtk_window_decorator_DEPENDENCIES = \
	$(top_builddir)/libdecoration/libdecoration.la
gtk_window_decorator_LDADD	  =		       \
	$(top_builddir)/libdecoration/libdecoration.la \
	@GTK_WINDOW_DECORATOR_LIBS@		       \
	$(marcolibs)

gtk_window_decorator_SOURCES	  = \
	gtk-window-decorator.c
gtk_window_decorator_program = gtk-window-decorator
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/include            \
	@GTK_WINDOW_DECORATOR_CFLAGS@      \
	@MARCO_CFLAGS@                     \
	-DALL_LINGUAS="\"@ALL_LINGUAS@\""  \
	-DLOCALEDIR="\"@datadir@/locale"\"

bin_PROGRAMS = $(gtk_window_decorator_program)

