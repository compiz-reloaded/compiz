if USE_KDE
kde_window_decorator_LDADD   = @KDE_WINDOW_DECORATOR_LIBS@
kde_window_decorator_SOURCES = main.cpp qdecorator.cpp
kde_window_decorator_program = kde-window-decorator

$(top_srcdir)/kde/window-decorator/qdecorator.cpp: qdecorator.moc

INCLUDES = @KDE_WINDOW_DECORATOR_CFLAGS@

bin_PROGRAMS = $(kde_window_decorator_program)

CLEANFILES=qdecorator.moc

%.moc: %.h
	$(QT_MOC) $< > $@

endif
