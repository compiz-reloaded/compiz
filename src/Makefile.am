INCLUDES =			     \
	@COMPIZ_CFLAGS@		     \
	@GL_CFLAGS@		     \
	-I$(top_srcdir)/include	     \
	-I$(top_builddir)/include    \
	-DPLUGINDIR=\"$(plugindir)\" \
	-DIMAGEDIR=\"$(imagedir)\"   \
	-DMETADATADIR=\"$(metadatadir)\"

bin_PROGRAMS = compiz

compiz_LDADD = @COMPIZ_LIBS@ @GL_LIBS@ -lm -ldl
compiz_LDFLAGS = -export-dynamic -pthread
compiz_SOURCES =   \
	main.c	   \
	privates.c \
	object.c   \
	core.c	   \
	texture.c  \
	display.c  \
	screen.c   \
	window.c   \
	event.c    \
	paint.c	   \
	option.c   \
	plugin.c   \
	session.c  \
	fragment.c \
	matrix.c   \
	cursor.c   \
	match.c    \
	metadata.c
