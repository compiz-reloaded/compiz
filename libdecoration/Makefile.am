lib_LTLIBRARIES = libdecoration.la

libdecoration_la_LIBADD  = @DECORATION_LIBS@ -lm
libdecoration_la_LDFLAGS = -no-undefined
libdecoration_la_SOURCES = decoration.c

INCLUDES = @DECORATION_CFLAGS@ -I$(top_srcdir)/include

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libdecoration.pc

EXTRA_DIST = libdecoration.pc.in