if USE_KDE
kde_window_decorator_LDADD   = @KDE_WINDOW_DECORATOR_LIBS@
kde_window_decorator_SOURCES = main.cpp qdecorator.cpp qwmutils.cpp \
	qwmwindow.cpp qwmscreen.cpp
kde_window_decorator_program = kde-window-decorator

$(top_srcdir)/kde/window-decorator/qdecorator.cpp: qdecorator.moc
$(top_srcdir)/kde/window-decorator/qwmscreen.cpp: qwmscreen.moc
$(top_srcdir)/kde/window-decorator/qwmwindow.cpp: qwmwindow.moc

INCLUDES = @KDE_WINDOW_DECORATOR_CFLAGS@

bin_PROGRAMS = $(kde_window_decorator_program)

CLEANFILES=qdecorator.moc qwmscreen.moc qwmwindow.moc

%.moc: %.h
	$(QT_MOC) $< > $@

endif
