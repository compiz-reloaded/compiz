if USE_KDE
kde_window_decorator_LDADD   = @KDE_WINDOW_DECORATOR_LIBS@
kde_window_decorator_SOURCES = main.cpp qdecorator.cpp qwmutils.cpp \
	qwmwindow.cpp qwmscreen.cpp
BUILT_SOURCES = qdecorator.moc qwmscreen.moc qwmwindow.moc
kde_window_decorator_program = kde-window-decorator

INCLUDES = @KDE_WINDOW_DECORATOR_CFLAGS@

bin_PROGRAMS = $(kde_window_decorator_program)

%.moc: %.h
	$(QT_MOC) $< > $@

endif
