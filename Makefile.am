SUBDIRS = include src libdecoration plugins images gtk kde po metadata

EXTRA_DIST =		    \
	COPYING		    \
	COPYING.GPL	    \
	COPYING.LGPL	    \
	COPYING.MIT	    \
	compiz.pc.in	    \
	compiz-gconf.pc.in  \
	intltool-extract.in \
	intltool-merge.in   \
	intltool-update.in

DISTCLEANFILES =	 \
	intltool-extract \
	intltool-merge   \
	intltool-update

if USE_GCONF
gconfdata = compiz-gconf.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = compiz.pc $(gconfdata)
