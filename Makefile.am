SUBDIRS = include src libdecoration plugins images icons gtk-window-decorator po mate metadata

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
	gettext

DISTCHECK_CONFIGURE_FLAGS = --with-gtk=$(GTK_API_VERSION)

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =	\
	compiz.pc	 \
	compiz-cube.pc	 \
	compiz-scale.pc

# Build ChangeLog from GIT history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
