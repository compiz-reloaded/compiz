AC_PREREQ(2.57)

dnl compiz package version number
dnl An odd micro number indicates in-progress development.
dnl An even micro number indicates a released version.
m4_define(compiz_version_major, 0)
m4_define(compiz_version_minor, 8)
m4_define(compiz_version_micro, 9)

AC_INIT([compiz],
	[compiz_version_major().compiz_version_minor().compiz_version_micro()],
	[davidr@novell.com])

COMPIZ_VERSION_MAJOR=compiz_version_major()
COMPIZ_VERSION_MINOR=compiz_version_minor()
COMPIZ_VERSION_MICRO=compiz_version_micro()
AC_SUBST(COMPIZ_VERSION_MAJOR)
AC_SUBST(COMPIZ_VERSION_MINOR)
AC_SUBST(COMPIZ_VERSION_MICRO)

AM_INIT_AUTOMAKE([1.9 foreign dist-xz no-dist-gzip check-news])
AC_CONFIG_HEADER([config.h])
AC_PROG_INTLTOOL([0.23])
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE

dnl decorator interface version
AC_DEFINE(DECOR_INTERFACE_VERSION, 20080529, [Decorator interface version])

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h unistd.h])

ALL_LINGUAS="af ar bg bn bn_IN bs ca cs cy da de el en_GB en_US es eu et fi fr gl gu he hi hr hu id it ja ka km ko lo lt mk mr nb nl or pa pl pt pt_BR ro ru sk sl sr sv ta tr uk vi xh zh_CN zh_TW zu"
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
GETTEXT_PACKAGE=compiz
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])
AC_SUBST(GETTEXT_PACKAGE)

if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[[\ \	]]-Wall[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wpointer-arith[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wpointer-arith" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wstrict-prototypes[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wstrict-prototypes" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wmissing-prototypes[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wmissing-prototypes" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wmissing-declarations[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wmissing-declarations" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wnested-externs[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wnested-externs" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-D_FORTIFY_SOURCE=2[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2" ;;
  esac

  if test "x$enable_ansi" = "xyes"; then
    case " $CFLAGS " in
    *[[\ \	]]-ansi[[\ \	]]*) ;;
    *) CFLAGS="$CFLAGS -ansi" ;;
    esac

    case " $CFLAGS " in
    *[[\ \	]]-pedantic[[\ \	]]*) ;;
    *) CFLAGS="$CFLAGS -pedantic" ;;
    esac
  fi
fi

if test "x$GCC" = "xyes"; then
  case " $CXXFLAGS " in
  *[[\ \	]]-Wall[[\ \	]]*) ;;
  *) CXXFLAGS="$CXXFLAGS -Wall" ;;
  esac

  case " $CXXFLAGS " in
  *[[\ \	]]-D_FORTIFY_SOURCE=2[[\ \	]]*) ;;
  *) CXXFLAGS="$CXXFLAGS -D_FORTIFY_SOURCE=2" ;;
  esac
fi

AC_C_BIGENDIAN

plugindir=$libdir/compiz
AC_SUBST(plugindir)

AC_ARG_WITH(default-plugins, [  --with-default-plugins=PLUGINS ],
		       [default_plugins="$withval"],
		       [default_plugins=""])
AC_SUBST(default_plugins)

imagedir=$datadir/compiz
AC_SUBST(imagedir)

metadatadir=$datadir/compiz
AC_SUBST(metadatadir)

COMPIZ_REQUIRES="x11        \
		 xcomposite \
		 xext       \
		 xfixes	    \
		 xdamage    \
		 xrandr	    \
		 xinerama   \
		 xcursor    \
		 ice	    \
		 sm	    \
		 libxml-2.0 \
		 libstartup-notification-1.0 >= 0.7"

PKG_CHECK_MODULES(COMPIZ, $COMPIZ_REQUIRES)
AC_SUBST(COMPIZ_REQUIRES)

DECORATION_REQUIRES="xrender"

PKG_CHECK_MODULES(DECORATION, $DECORATION_REQUIRES)
AC_SUBST(DECORATION_REQUIRES)

PKG_CHECK_EXISTS(xrender >= 0.9.3,
		 [have_xrender_0_9_3=yes], [have_xrender_0_9_3=no])

if test "$have_xrender_0_9_3" = yes; then
  AC_DEFINE(HAVE_XRENDER_0_9_3, 1,
	    [Define to 1 if xrender version >= 0.9.3])
fi

AC_MSG_CHECKING(for GL_CFLAGS)
AC_ARG_WITH(gl-cflags, [  --with-gl-cflags=CFLAGS ],
		       [GL_CFLAGS="$withval"],
		       [GL_CFLAGS=""])

AC_MSG_RESULT($GL_CFLAGS)
AC_MSG_CHECKING(for GL_LIBS)
AC_ARG_WITH(gl-libs, [  --with-gl-libs=LIBS ],
		     [GL_LIBS="$withval"],
		     [GL_LIBS="-lGL"])
AC_MSG_RESULT($GL_LIBS)

AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

AC_CHECK_PROGS(XSLTPROC, xsltproc)
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([Couldn't find xsltproc])
fi

PKG_CHECK_MODULES(LIBPNG, libpng)

dnl Check for jpeglib.h, required for jpeg.c
AC_CHECK_HEADERS([jpeglib.h], [] ,AC_MSG_ERROR(jpeglib.h not found --- aborting))

dnl Check for GL/glu.h, required for plugin blur
AC_CHECK_HEADERS([GL/glu.h], [] ,AC_MSG_ERROR(GL/glu.h not found --- aborting))

AC_ARG_ENABLE(inotify,
  [  --disable-inotify       Disable inotify plugin],
  [use_inotify=$enableval], [use_inotify=yes])

if test "x$use_inotify" = "xyes"; then
  AC_CHECK_HEADERS([sys/inotify.h], [use_inotify=yes], [use_inotify=no])
fi

AM_CONDITIONAL(INOTIFY_PLUGIN, test "x$use_inotify" = "xyes")
if test "$use_inotify" = yes; then
  AC_DEFINE(USE_INOTIFY, 1, [Build inotify plugin])
fi

AC_ARG_ENABLE(fuse,
  [  --disable-fuse          Disable fuse plugin],
  [use_fuse=$enableval], [use_fuse=yes])

if test "x$use_fuse" = "xyes"; then
  PKG_CHECK_MODULES(FUSE, fuse, [use_fuse=yes], [use_fuse=no])
fi

AM_CONDITIONAL(FUSE_PLUGIN, test "x$use_fuse" = "xyes")
if test "$use_fuse" = yes; then
  AC_DEFINE(USE_FUSE, 1, [Build fuse plugin])
fi

AC_ARG_ENABLE(annotate,
  [  --disable-annotate      Disable annotate plugin],
  [use_annotate=$enableval], [use_annotate=yes])

if test "x$use_annotate" = "xyes"; then
  PKG_CHECK_MODULES(ANNOTATE, cairo-xlib-xrender, [use_annotate=yes], [use_annotate=no])
fi

AM_CONDITIONAL(ANNOTATE_PLUGIN, test "x$use_annotate" = "xyes")
if test "$use_annotate" = yes; then
  AC_DEFINE(USE_ANNOTATE, 1, [Build annotate plugin])
fi

AC_ARG_ENABLE(librsvg,
  [  --disable-librsvg       Disable svg support],
  [use_librsvg=$enableval], [use_librsvg=yes])

if test "x$use_librsvg" = "xyes"; then
  PKG_CHECK_MODULES(LIBRSVG, [cairo >= 1.0 librsvg-2.0 >= 2.14.0], [use_librsvg=yes], [use_librsvg=no])
fi

AM_CONDITIONAL(USE_LIBRSVG, test "x$use_librsvg" = "xyes")
if test "$use_librsvg" = yes; then
  AC_DEFINE(USE_LIBRSVG, 1, [librsvg for SVG support])
fi

AC_ARG_ENABLE(gtk,
  [  --disable-gtk           Disable gtk window decorator],
  [use_gtk=$enableval], [use_gtk=yes])

AC_ARG_ENABLE(marco,
  [  --disable-marco      Disable marco theme support],
  [use_marco=$enableval], [use_marco=yes])

if test "x$use_gtk" = "xyes"; then
  PKG_CHECK_MODULES(GTK_WINDOW_DECORATOR,
		    xrender >= 0.8.4  \
		    xcursor           \
		    gtk+-2.0 >= 2.8.0 \
		    libwnck-1.0	      \
		    pangocairo,
		    [use_gtk=yes], [use_gtk=no])
  if test "x$use_gtk" = "xyes"; then
    save_CFLAGS="$CFLAGS"
    save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $GTK_WINDOW_DECORATOR_CFLAGS"
    LIBS="$LIBS $GTK_WINDOW_DECORATOR_LIBS"
    CFLAGS="$save_CFLAGS"
    LIBS="$save_LIBS -lm -ldl"

    if test "x$use_marco" = "xyes"; then
      PKG_CHECK_MODULES(MARCO, libmarco-private,
			[use_marco=yes], [use_marco=no])
    fi
  else
    use_marco="no"
  fi
else
  use_marco="no"
fi

AM_CONDITIONAL(USE_GTK, test "x$use_gtk" = "xyes")
if test "$use_gtk" = yes; then
  AC_DEFINE(USE_GTK, 1, [Build gtk window decorator])
fi

AM_CONDITIONAL(USE_MARCO, test "x$use_marco" = "xyes")
if test "$use_marco" = yes; then
  AC_DEFINE(USE_MARCO, 1, [Build marco theme support])
fi

AC_ARG_WITH(max-desktops,
  [  --with-max-desktops     Max reasonable desktops],
  [MAX_DESKTOPS=$withval],
  [MAX_DESKTOPS=36])

if test -n "$MAX_DESKTOPS"; then
  AC_DEFINE_UNQUOTED(MAX_DESKTOPS, $MAX_DESKTOPS, [Max reasonable desktops])
fi

AC_CONFIG_COMMANDS([include/compiz-common.h],
[
	commonfile=include/compiz-common.h
	outfile=$commonfile.tmp
	AC_MSG_NOTICE([creating $commonfile])
	cat > $outfile <<_EOF
#ifndef COMPIZ_COMMON_H
#define COMPIZ_COMMON_H

#ifdef  __cplusplus
# define COMPIZ_BEGIN_DECLS  extern "C" {
# define COMPIZ_END_DECLS    }
#else
# define COMPIZ_BEGIN_DECLS
# define COMPIZ_END_DECLS
#endif

#define COMPIZ_VERSION_MAJOR $COMPIZ_VERSION_MAJOR
#define COMPIZ_VERSION_MINOR $COMPIZ_VERSION_MINOR
#define COMPIZ_VERSION_MICRO $COMPIZ_VERSION_MICRO

#define COMPIZ_VERSION_STRING "$COMPIZ_VERSION_MAJOR.$COMPIZ_VERSION_MINOR.$COMPIZ_VERSION_MICRO"

_EOF
	echo '#endif' >> $outfile

	if cmp -s $outfile $commonfile; then
	  AC_MSG_NOTICE([$commonfile is unchanged])
	  rm -f $outfile
	else
	  mv $outfile $commonfile
	fi
],[
	COMPIZ_VERSION_MAJOR=$COMPIZ_VERSION_MAJOR
	COMPIZ_VERSION_MINOR=$COMPIZ_VERSION_MINOR
	COMPIZ_VERSION_MICRO=$COMPIZ_VERSION_MICRO
])

AC_OUTPUT([
compiz.pc
compiz-cube.pc
compiz-scale.pc
Makefile
src/Makefile
libdecoration/Makefile
libdecoration/libdecoration.pc
include/Makefile
plugins/Makefile
images/Makefile
gtk/Makefile
gtk/window-decorator/Makefile
po/Makefile.in
metadata/Makefile
scripts/Makefile
])

echo ""
echo "the following optional plugins will be compiled:"
echo "  annotate: $use_annotate"
echo "  svg:      $use_librsvg"
echo "  inotify:  $use_inotify"
echo "  fuse:     $use_fuse"
echo ""
echo "and the following optional features will be compiled:"
echo "  gtk:      $use_gtk"
echo "  marco:    $use_marco"
echo ""
