SUBDIRS = include src libdecoration plugins images gtk po metadata scripts

EXTRA_DIST =		    \
	COPYING		    \
	COPYING.GPL	    \
	COPYING.LGPL	    \
	COPYING.MIT	    \
	compiz.pc.in	    \
	compiz-cube.pc.in   \
	compiz-scale.pc.in

DISTCLEANFILES =	 \
	po/stamp-it	 \
	po/.intltool-merge-cache

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =	\
	compiz.pc	\
	compiz-cube.pc	\
	compiz-scale.pc
