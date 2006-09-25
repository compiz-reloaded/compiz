if USE_GTK

if USE_METACITY
metacitylibs = @METACITY_LIBS@
endif

gtk_window_decorator_LDADD   =      \
	@GTK_WINDOW_DECORATOR_LIBS@ \
	$(metacitylibs)
gtk_window_decorator_SOURCES = \
	gtk-window-decorator.c
gtk_window_decorator_program = gtk-window-decorator
endif

INCLUDES = @GTK_WINDOW_DECORATOR_CFLAGS@ @METACITY_CFLAGS@

bin_PROGRAMS = $(gtk_window_decorator_program)