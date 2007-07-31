SUBDIRS = include src libdecoration plugins images gtk kde po metadata

EXTRA_DIST =		    \
	COPYING		    \
	COPYING.GPL	    \
	COPYING.LGPL	    \
	COPYING.MIT	    \
	compiz.pc.in	    \
	compiz-cube.pc.in   \
	compiz-scale.pc.in  \
	compiz-gconf.pc.in  \
	intltool-extract.in \
	intltool-merge.in   \
	intltool-update.in  \
	intltool-extract    \
	intltool-merge	    \
	intltool-update

DISTCLEANFILES =	 \
	intltool-extract \
	intltool-merge   \
	intltool-update

if USE_GCONF
gconfdata = compiz-gconf.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = compiz.pc compiz-cube.pc compiz-scale.pc $(gconfdata)
