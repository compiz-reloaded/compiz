SUBDIRS = include src libdecoration plugins images gtk po metadata

EXTRA_DIST =		    \
	AUTHORS		    \
	COPYING		    \
	COPYING.GPL	    \
	COPYING.LGPL	    \
	COPYING.MIT	    \
	NEWS		    \
	README.md	    \
	autogen.sh	    \
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
