if USE_GNOME

if USE_METACITY
metacitylibs = @METACITY_LIBS@
endif

gnome_window_decorator_LDADD   =      \
	@GNOME_WINDOW_DECORATOR_LIBS@ \
	$(metacitylibs)
gnome_window_decorator_SOURCES = \
	gnome-window-decorator.c
gnome_window_decorator_program = gnome-window-decorator
endif

INCLUDES = @GNOME_WINDOW_DECORATOR_CFLAGS@ @METACITY_CFLAGS@

bin_PROGRAMS = $(gnome_window_decorator_program)