AC_PREREQ(2.57)

dnl compiz package version number
m4_define(compiz_version_major, 0)
m4_define(compiz_version_minor, 8)
m4_define(compiz_version_micro, 18)

AC_INIT([compiz], [0.8.18], [chat-to-me@raveit.de],
        [compiz], [https://gitlab.com/compiz/compiz-core])

COMPIZ_VERSION_MAJOR=compiz_version_major()
COMPIZ_VERSION_MINOR=compiz_version_minor()
COMPIZ_VERSION_MICRO=compiz_version_micro()
AC_SUBST(COMPIZ_VERSION_MAJOR)
AC_SUBST(COMPIZ_VERSION_MINOR)
AC_SUBST(COMPIZ_VERSION_MICRO)

AM_INIT_AUTOMAKE([1.9 foreign dist-xz no-dist-gzip check-news])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE

dnl decorator interface version
AC_DEFINE(DECOR_INTERFACE_VERSION, 20110504, [Decorator interface version])

AC_ISC_POSIX
AC_PROG_CC
LT_INIT
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h unistd.h])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.7])
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])
AC_SUBST(GETTEXT_PACKAGE)

dnl The following case statement works around a bug present in
dnl gettext-0.20 and gettext-0.20.1 which occurs at 'make dist'
dnl and 'make distcheck' invocations.
dnl https://git.savannah.gnu.org/gitweb/?p=gettext.git;a=commitdiff;h=2336451ed68d91ff4b5ae1acbc1eca30e47a86a9

dnl Unless --for-msgfmt is functional, assign the value that
dnl was default before --for-msgfmt was introduced:
case `LC_ALL=C $MSGMERGE --version | sed 1q | sed -e 's,^[[^0-9]]*,,'` in
  0.20|0.20.1)
    MSGMERGE_FOR_MSGFMT_OPTION='--no-fuzzy-matching --no-location --quiet' ;;
esac

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

AC_MSG_CHECKING([which plugins to use by default])
AC_ARG_WITH([default-plugins],
  [AS_HELP_STRING([--with-default-plugins=PLUGINS], [Plugins to use by default, comma-separated [default=core]])],
  [
   case "$withval" in
   yes|no)
     default_plugins=core ;;
   *)
     if expr "x$withval" : "x[[[:alnum:]]][[[:alnum:]]]*\(,[[[:alnum:]]][[[:alnum:]]]*\)*$" 1> /dev/null; then
       default_plugins="$withval"
     else
       AC_MSG_ERROR([["]$withval["] is not a comma-separated list of plugins])
     fi ;;
   esac
  ],
  [default_plugins=core])
AC_MSG_RESULT($default_plugins)
AC_SUBST(default_plugins)

imagedir=$datadir/compiz
AC_SUBST(imagedir)

metadatadir=$datadir/compiz
AC_SUBST(metadatadir)

pluginiconsdir=$datadir/compiz/icons/hicolor
AC_SUBST(pluginiconsdir)

COMPIZ_REQUIRES="x11 xcomposite xext xfixes xdamage xrandr xi xinerama xcursor ice sm libxml-2.0 libstartup-notification-1.0 >= 0.7"

PKG_CHECK_MODULES(COMPIZ, [$COMPIZ_REQUIRES])

AC_SUBST([COMPIZ_REQUIRES])
LIBS="$LIBS -lm -ldl"

DECORATION_REQUIRES="xrender"

PKG_CHECK_MODULES(DECORATION, [$DECORATION_REQUIRES])
AC_SUBST([DECORATION_REQUIRES])

PKG_CHECK_EXISTS(xrender >= 0.9.3,
  [have_xrender_0_9_3=yes], [have_xrender_0_9_3=no])

if test "$have_xrender_0_9_3" = yes; then
  AC_DEFINE(HAVE_XRENDER_0_9_3, 1,
    [Define to 1 if xrender version >= 0.9.3])
fi

AC_CHECK_HEADER([X11/extensions/XInput2.h],
  [have_xinput2=yes], [have_xinput2=no])

if test "$have_xinput2" = yes; then
  AC_DEFINE(HAVE_XINPUT2, 1,
    [Define to 1 if xinput2 is available])
fi

AC_MSG_CHECKING(for GL_CFLAGS)
AC_ARG_WITH(gl-cflags,
  [  --with-gl-cflags=CFLAGS ],
  [GL_CFLAGS="$withval"],
  [GL_CFLAGS=""])
AC_MSG_RESULT($GL_CFLAGS)

AC_MSG_CHECKING(for GL_LIBS)
AC_ARG_WITH(gl-libs,
  [  --with-gl-libs=LIBS ],
  [GL_LIBS="$withval"],
  [GL_LIBS="-lGL"])
AC_MSG_RESULT($GL_LIBS)

AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

AC_CHECK_PROGS(XSLTPROC, xsltproc)
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([Couldn't find xsltproc])
fi

PKG_CHECK_MODULES(LIBPNG, [libpng])

dnl Check for jpeglib.h, required for jpeg.c
AC_CHECK_HEADERS([jpeglib.h], [] ,AC_MSG_ERROR(jpeglib.h not found --- aborting))

dnl Check for GL/glu.h, required for plugin blur
AC_CHECK_HEADERS([GL/glu.h], [] ,AC_MSG_ERROR(GL/glu.h not found --- aborting))

AC_ARG_ENABLE([xi2-events],
  [AS_HELP_STRING([--enable-xi2-events], [Enable XInput2 events support])],
  [use_xi2_events=$enableval
   AS_IF([test "x$use_xi2_events" = xyes && \
          test "x$have_xinput2" != xyes],
         [AC_MSG_ERROR([XInput2 events support requires XInput2])])],
  [use_xi2_events=no])

AS_IF([test "x$use_xi2_events" = xyes],
  [AC_DEFINE([USE_XI2_EVENTS], [1], [Whether to use XInput2 events])])

AC_ARG_ENABLE(glib,
  [  --disable-glib          Disable glib plugin],
  [use_glib=$enableval], [use_glib=yes])

if test "x$use_glib" = "xyes"; then
  PKG_CHECK_MODULES(GLIB, [glib-2.0])
fi

AM_CONDITIONAL(USE_GLIB, test "x$use_glib" = "xyes")
if test "$use_glib" = yes; then
  AC_DEFINE(USE_GLIB, 1, [Build glib plugin])
fi

AC_ARG_ENABLE(dbus,
  [  --disable-dbus          Disable dbus plugin],
  [use_dbus=$enableval], [use_dbus=yes])

if test "x$use_dbus" = "xyes"; then
  PKG_CHECK_MODULES(DBUS, [dbus-1 libxml-2.0], [use_dbus=yes], [use_dbus=no])
fi

AM_CONDITIONAL(DBUS_PLUGIN, test "x$use_dbus" = "xyes")
if test "$use_dbus" = yes; then
  AC_DEFINE(USE_DBUS, 1, [Build dbus plugin])
fi

AC_ARG_ENABLE(dbus-glib,
  [  --disable-dbus-glib     Disable dbus-glib support],
  [use_dbus_glib=$enableval], [use_dbus_glib=yes])

if test "x$use_dbus_glib" = "xyes"; then
  PKG_CHECK_MODULES(DBUS_GLIB, [dbus-glib-1], [use_dbus_glib=yes], [use_dbus_glib=no])
fi

if test "x$use_dbus_glib" = "xyes"; then
  AC_DEFINE(USE_DBUS_GLIB, 1, [Build dbus glib support])
fi

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
  PKG_CHECK_MODULES(FUSE, [fuse], [use_fuse=yes], [use_fuse=no])
fi

AM_CONDITIONAL(FUSE_PLUGIN, test "x$use_fuse" = "xyes")
if test "$use_fuse" = yes; then
  AC_DEFINE(USE_FUSE, 1, [Build fuse plugin])
fi

AC_ARG_ENABLE(annotate,
  [  --disable-annotate      Disable annotate plugin],
  [use_annotate=$enableval], [use_annotate=yes])

if test "x$use_annotate" = "xyes"; then
  PKG_CHECK_MODULES(ANNOTATE, [cairo-xlib-xrender], [use_annotate=yes], [use_annotate=no])
fi

AM_CONDITIONAL(ANNOTATE_PLUGIN, test "x$use_annotate" = "xyes")
if test "$use_annotate" = yes; then
  AC_DEFINE(USE_ANNOTATE, 1, [Build annotate plugin])
fi

AC_ARG_ENABLE(librsvg,
  [  --disable-librsvg       Disable svg support],
  [use_librsvg=$enableval], [use_librsvg=yes])

if test "x$use_librsvg" = "xyes"; then
  PKG_CHECK_MODULES(LIBRSVG, [cairo >= 1.0 librsvg-2.0 >= 2.36.2], [use_librsvg=yes], [use_librsvg=no])
fi

AM_CONDITIONAL(USE_LIBRSVG, test "x$use_librsvg" = "xyes")
if test "$use_librsvg" = yes; then
  AC_DEFINE(USE_LIBRSVG, 1, [librsvg for SVG support])
fi

AC_ARG_ENABLE(menu-entries,
  [  --enable-menu-entries   Enable desktop menu entries],
  [use_menu_entries=$enableval], [use_menu_entries=no])

AM_CONDITIONAL(USE_MENU_ENTRIES, test "x$use_menu_entries" = "xyes")
if test "$use_menu_entries" = yes; then
  AC_DEFINE(USE_MENU_ENTRIES, 1, [Enable desktop menu entries])
fi

AC_ARG_ENABLE(gtk,
  [  --disable-gtk           Disable gtk window decorator],
  [use_gtk=$enableval], [use_gtk=auto])

AC_MSG_CHECKING([which gtk+ version to compile against])
AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk=2.0|3.0|auto], [Which gtk+ version to compile against [default=auto]])],
  [
   case "$with_gtk" in
     2.0|3.0|auto) ;;
     *) AC_MSG_ERROR([invalid gtk+ version specified]) ;;
   esac
  ],
  [with_gtk=auto])
AC_MSG_RESULT([$with_gtk])

AC_SUBST([GTK_API_VERSION])

AC_ARG_ENABLE(marco,
  [  --disable-marco         Disable marco theme support],
  [use_marco=$enableval], [use_marco=auto])

AC_ARG_ENABLE(mate,
  [  --disable-mate          Disable basic mate compatibility],
  [use_mate=$enableval], [use_mate=yes])

AC_ARG_ENABLE(gsettings,
  [  --disable-gsettings     Disable gsettings gwd support],
  [use_gsettings=$enableval], [use_gsettings=yes])

GWD_REQUIRE_GTK3="gtk+-3.0
                  libwnck-3.0
                  cairo >= 1.4
                  pangocairo
                  xrender >= 0.8.4
                  xi"
GWD_REQUIRE_GTK2="gtk+-2.0 >= 2.22.0
                  libwnck-1.0 >= 2.22.0
                  cairo >= 1.4
                  pangocairo
                  xrender >= 0.8.4
                  xi"

if test "x$use_gtk" != "xno"; then
  if test "x$use_marco" = "xauto"; then
    PKG_CHECK_MODULES(MARCO, libmarco-private,
                      [use_marco=yes], [use_marco=no])
  elif test "x$use_marco" = "xyes"; then
    PKG_CHECK_MODULES(MARCO, libmarco-private)
  fi
  if test "x$with_gtk" = "xauto"; then
    if test "x$use_marco" = "xno"; then
      # Try Gtk3 first, then Gtk2, then if none exist, ask for Gtk3.
      PKG_CHECK_EXISTS([$GWD_REQUIRE_GTK3], [with_gtk=3.0],
                       [PKG_CHECK_EXISTS([$GWD_REQUIRE_GTK2],
                                         [with_gtk=2.0], [with_gtk=3.0])])
    else
      for i in $(pkg-config --print-requires libmarco-private); do
        if test "x$i" = "xgtk+-3.0"; then
          with_gtk="3.0"
          break
        elif test "x$i" = "xgtk+-2.0"; then
          with_gtk="2.0"
          break
        fi
      done
      if test "x$with_gtk" = "xauto"; then
        AC_MSG_ERROR([marco is built with unknown gtk+])
      fi
    fi
  fi

  if test "x$with_gtk" = "x3.0"; then
    GWD_REQUIRE="$GWD_REQUIRE_GTK3"
  else
    GWD_REQUIRE="$GWD_REQUIRE_GTK2"
  fi
  if test "x$use_gtk" = "xauto"; then
    PKG_CHECK_MODULES(GTK_WINDOW_DECORATOR, [$GWD_REQUIRE],
                      [use_gtk=yes], [use_gtk=no])
  else
    PKG_CHECK_MODULES(GTK_WINDOW_DECORATOR, [$GWD_REQUIRE])
  fi
else
  use_marco="no"
fi

# Check if we are building against marco 1.22.2 or later
if test "x$use_marco" = "xyes"; then
  PKG_CHECK_EXISTS(libmarco-private >= 1.22.2,
    [have_marco_1_22_2=yes], [have_marco_1_22_2=no])
else
  have_marco_1_22_2="no"
fi

if test "x$use_gtk" = "xyes"; then
  if test "x$use_gsettings" = "xyes"; then
    PKG_CHECK_MODULES(GSETTINGS, [glib-2.0 >= 2.32 gio-2.0 >= 2.25.0])
    GLIB_GSETTINGS
  fi
else
  use_gsettings="no"
fi

AM_CONDITIONAL(USE_GTK, test "x$use_gtk" = "xyes")
if test "$use_gtk" = yes; then
  AC_DEFINE(USE_GTK, 1, [Build gtk window decorator])
fi

AM_CONDITIONAL(WITH_GTK, test "x$with_gtk" != "x")
if test "$with_gtk" != ""; then
  AC_DEFINE(WITH_GTK, 1, [Build with Gtk $with_gtk])
  GTK_API_VERSION="$with_gtk"
  AC_SUBST([GTK_API_VERSION])
fi

AM_CONDITIONAL(USE_MARCO, test "x$use_marco" = "xyes")
if test "$use_marco" = yes; then
  AC_DEFINE(USE_MARCO, 1, [Build marco theme support])
fi

AM_CONDITIONAL(HAVE_MARCO_1_22_2, test "x$have_marco_1_22_2" = "xyes")
if test "$have_marco_1_22_2" = yes; then
  AC_DEFINE(HAVE_MARCO_1_22_2, 1,
      [Build against marco 1.22.2 o later])
fi

AM_CONDITIONAL(USE_MATE, test "x$use_mate" = "xyes")
if test "$use_mate" = yes; then
  AC_DEFINE(USE_MATE, 1, [Build basic mate compatibility])
fi

AM_CONDITIONAL(USE_GSETTINGS, test "x$use_gsettings" = "xyes")
if test "$use_gsettings" = yes; then
  AC_DEFINE(USE_GSETTINGS, 1, [Build gsettings support in gtk-window-decorator])
fi

AC_ARG_WITH(max-desktops,
  [  --with-max-desktops     Max reasonable desktops],
  [MAX_DESKTOPS=$withval],
  [MAX_DESKTOPS=36])

if test -n "$MAX_DESKTOPS"; then
  AC_DEFINE_UNQUOTED(MAX_DESKTOPS, $MAX_DESKTOPS, [Max reasonable desktops])
fi

AC_PATH_PROG(UPDATE_ICON_CACHE, gtk-update-icon-cache)

AC_CONFIG_COMMANDS([include/compiz-common.h],
[
  commonfile=include/compiz-common.h
  outfile=$commonfile.tmp
  AC_MSG_NOTICE([creating $commonfile])
  cat >$outfile <<-_EOF
	#ifndef COMPIZ_COMMON_H
	#  define COMPIZ_COMMON_H
	
	#  ifdef  __cplusplus
	#    define COMPIZ_BEGIN_DECLS  extern "C" {
	#    define COMPIZ_END_DECLS    }
	#  else
	#    define COMPIZ_BEGIN_DECLS
	#    define COMPIZ_END_DECLS
	#  endif
	
	#  define COMPIZ_VERSION_MAJOR $COMPIZ_VERSION_MAJOR
	#  define COMPIZ_VERSION_MINOR $COMPIZ_VERSION_MINOR
	#  define COMPIZ_VERSION_MICRO $COMPIZ_VERSION_MICRO
	
	#  define COMPIZ_VERSION_STRING "$COMPIZ_VERSION_MAJOR.$COMPIZ_VERSION_MINOR.$COMPIZ_VERSION_MICRO"
	#endif
_EOF
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
icons/Makefile
gtk-window-decorator/Makefile
po/Makefile.in
mate/Makefile
metadata/Makefile
])

echo ""
echo "the following optional plugins will be compiled:"
echo "  glib:                      $use_glib"
echo "  dbus:                      $use_dbus"
echo "  annotate:                  $use_annotate"
echo "  svg:                       $use_librsvg"
echo "  inotify:                   $use_inotify"
echo "  fuse:                      $use_fuse"
echo "  matecompat:                $use_mate"
echo ""
echo "and the following optional features will be used:"
echo "  XI2 events:                $use_xi2_events"
echo "  gtk:                       $use_gtk ($with_gtk)"
echo "  marco:                     $use_marco"
echo "  marco >=1.22.2 support:    $have_marco_1_22_2"
echo "  gwd gsettings support:     $use_gsettings"
echo "  desktop menu entries:      $use_menu_entries"
echo ""
