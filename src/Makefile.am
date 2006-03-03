INCLUDES =			     \
	@COMPIZ_CFLAGS@		     \
	-I$(top_srcdir)/include	     \
	-DPLUGINDIR=\"$(plugindir)\" \
	-DIMAGEDIR=\"$(imagedir)\"

bin_PROGRAMS = compiz

compiz_LDADD = @COMPIZ_LIBS@ @GL_LIBS@ -lm
compiz_LDFLAGS = -export-dynamic
compiz_SOURCES =   \
	main.c	   \
	privates.c \
	texture.c  \
	display.c  \
	screen.c   \
	window.c   \
	event.c    \
	paint.c	   \
	option.c   \
	plugin.c   \
	readpng.c  \
	session.c
