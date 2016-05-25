SUBDIRS = include src libdecoration plugins images gtk-window-decorator po mate metadata

EXTRA_DIST =		   \
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
	compiz-scale.pc.in  \
	intltool-extract.in \
	intltool-merge.in   \
	intltool-update.in

DISTCLEANFILES =		\
	po/stamp-it		 \
	intltool-extract	 \
	intltool-merge		 \
	intltool-update		 \
	po/.intltool-merge-cache

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =	\
	compiz.pc	 \
	compiz-cube.pc	 \
	compiz-scale.pc
