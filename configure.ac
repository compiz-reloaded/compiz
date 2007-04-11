AC_PREREQ(2.57)

AC_INIT([compiz], [0.5.1], [davidr@novell.com])

AC_CONFIG_AUX_DIR(config)

AM_INIT_AUTOMAKE([dist-bzip2])
AC_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

/* decorator interface version */
AC_DEFINE(DECOR_INTERFACE_VERSION, 20070319, [Decorator interface version])

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h unistd.h])

ALL_LINGUAS="cs de es fi fr hu it ja pl pt_BR sv zh_CN zh_TW af ar bg bn bs ca cy da el en_GB en_US et gl gu he hi hr id ka km ko lo lt mk mr nb nl pa pt ro ru sk sl sr ta tr uk vi xh zu"
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

AC_C_BIGENDIAN

plugindir=$libdir/compiz
AC_SUBST(plugindir)

imagedir=$datadir/compiz
AC_SUBST(imagedir)

metadatadir=$libdir/compiz
AC_SUBST(metadatadir)


COMPIZ_REQUIRES="libpng	    \
		 xcomposite \
		 xfixes	    \
		 xdamage    \
		 xrandr	    \
		 xinerama   \
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

AC_ARG_ENABLE(gconf,
  [  --disable-gconf         Disable gconf plugin],
  [use_gconf=$enableval], [use_gconf=yes])

if test "x$use_gconf" = "xyes"; then
  PKG_CHECK_MODULES(GCONF, gconf-2.0)

  AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
  if test x"$GCONFTOOL" = xno; then
    AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
  fi
fi

AM_GCONF_SOURCE_2

AM_CONDITIONAL(USE_GCONF, test "x$use_gconf" = "xyes")
if test "$use_gconf" = yes; then
  AC_DEFINE(USE_GCONF, 1, [Build gconf plugin])
fi

AC_ARG_ENABLE(gconf-dump,
  [  --enable-gconf-dump     Enable gconf-dump plugin (for developers)],
  [use_gconf_dump=$enableval], [use_gconf_dump=no])

AM_CONDITIONAL(GCONF_DUMP_PLUGIN, test "x$use_gconf_dump" = "xyes")
if test "$use_gconf_dump" = yes; then
  AC_DEFINE(USE_GCONF_DUMP, 1, [Build gconf-dump plugin])
fi

AC_ARG_ENABLE(place,
  [  --disable-place         Disable window placement plugin],
  [use_place=$enableval], [use_place=yes])

if test "x$use_place" = "xyes"; then
  PKG_CHECK_MODULES(PLACE, glib-2.0, [use_place=yes], [use_place=no])
fi

AM_CONDITIONAL(PLACE_PLUGIN, test "x$use_place" = "xyes")
if test "$use_place" = yes; then
  AC_DEFINE(USE_PLACE, 1, [Build placement plugin])
fi

AC_ARG_ENABLE(dbus,
  [  --disable-dbus          Disable dbus plugin],
  [use_dbus=$enableval], [use_dbus=yes])

if test "x$use_dbus" = "xyes"; then
  PKG_CHECK_MODULES(DBUS, dbus-1, [use_dbus=yes], [use_dbus=no])
fi

AM_CONDITIONAL(DBUS_PLUGIN, test "x$use_dbus" = "xyes")
if test "$use_dbus" = yes; then
  AC_DEFINE(USE_DBUS, 1, [Build dbus plugin])
fi

AC_ARG_ENABLE(dbus-glib,
  [  --disable-dbus-glib     Disable dbus-glib support],
  [use_dbus_glib=$enableval], [use_dbus_glib=yes])

if test "x$use_dbus_glib" = "xyes"; then
  PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1, [use_dbus_glib=yes], [use_dbus_glib=no])
fi

if test "$use_dbus" = yes; then
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
  [  --enable-librsvg        Enable svg support],
  [use_librsvg=$enableval], [use_librsvg=no])

if test "x$use_librsvg" = "xyes"; then
  PKG_CHECK_MODULES(LIBRSVG, [cairo >= 1.0 librsvg-2.0 >= 2.14.0], [use_librsvg=yes], [use_librsvg=no])
fi

AM_CONDITIONAL(USE_LIBRSVG, test "x$use_librsvg" = "xyes")
if test "$use_librsvg" = yes; then
  AC_DEFINE(USE_LIBRSVG, 1, [librsvg for SVG support])
fi

AC_ARG_WITH(scale-corner,
  [  --with-scale-corner     Set default scale plugin activation corner],
  [case $withval in
   no) SCALE_CORNER="" ;;
   TopLeft|TopRight|BottomLeft|BottomRight) SCALE_CORNER="$withval" ;;
   *) AC_MSG_ERROR([Unrecognized scale corner "$withval"]) ;;
   esac], [SCALE_CORNER="TopRight"])
if test -n "$SCALE_CORNER"; then
  AC_DEFINE_UNQUOTED(SCALE_CORNER, "$SCALE_CORNER", [Default scale plugin activation corner])
fi
AC_SUBST(SCALE_CORNER)

AC_ARG_WITH(default-plugins,
  [  --with-default-plugins  Set default plugins],
  [DEFAULT_PLUGINS="$withval"],
  [DEFAULT_PLUGINS="png,decoration,fade,minimize,move,resize,switcher"])

if test -n "$DEFAULT_PLUGINS"; then
  AC_DEFINE_UNQUOTED(DEFAULT_PLUGINS, "$DEFAULT_PLUGINS", [Default plugins])
fi
AC_SUBST(DEFAULT_PLUGINS)

AC_ARG_ENABLE(gtk,
  [  --disable-gtk           Disable gtk window decorator],
  [use_gtk=$enableval], [use_gtk=yes])

AC_ARG_ENABLE(metacity,
  [  --disable-metacity      Disable metacity theme support],
  [use_metacity=$enableval], [use_metacity=yes])

AC_ARG_ENABLE(gnome,
  [  --disable-gnome         Disable gnome settings module],
  [use_gnome=$enableval], [use_gnome=yes])

if test "x$use_gtk" = "xyes"; then
  PKG_CHECK_MODULES(GTK_WINDOW_DECORATOR,
		    xrender >= 0.8.4  \
		    gtk+-2.0 >= 2.8.0 \
		    libwnck-1.0	      \
		    pangocairo,
		    [use_gtk=yes], [use_gtk=no])
  if test "x$use_gtk" = "xyes"; then
    save_CFLAGS="$CFLAGS"
    save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $GTK_WINDOW_DECORATOR_CFLAGS"
    LIBS="$LIBS $GTK_WINDOW_DECORATOR_LIBS"
    AC_CHECK_FUNCS(wnck_window_has_name)
    CFLAGS="$save_CFLAGS"
    LIBS="$save_LIBS"

    if test "x$use_metacity" = "xyes"; then
      PKG_CHECK_MODULES(METACITY, libmetacity-private,
			[use_metacity=yes], [use_metacity=no])
      PKG_CHECK_EXISTS(libmetacity-private >= 2.15.21,
		       [have_metacity_2_15_21=yes], [have_metacity_2_15_21=no])
      PKG_CHECK_EXISTS(libmetacity-private >= 2.17.0,
		       [have_metacity_2_17_0=yes], [have_metacity_2_17_0=no])
    fi

    if test "x$use_gnome" = "xyes"; then
      PKG_CHECK_MODULES(GNOME_WINDOW_SETTINGS,
			gnome-window-settings-2.0 gnome-desktop-2.0,
			[use_gnome=yes], [use_gnome=no])
    fi

    windowsettingsdatadir=`pkg-config --variable=prefix gnome-window-settings-2.0`/share
    windowsettingslibdir=`pkg-config --variable=libdir gnome-window-settings-2.0`
  else
    use_metacity="no"
    use_gnome="no"
  fi
else
  use_metacity="no"
  use_gnome="no"
fi

AC_SUBST(windowsettingsdatadir)
AC_SUBST(windowsettingslibdir)

AM_CONDITIONAL(USE_GTK, test "x$use_gtk" = "xyes")
if test "$use_gtk" = yes; then
  AC_DEFINE(USE_GTK, 1, [Build gtk window decorator])
fi

AM_CONDITIONAL(USE_METACITY, test "x$use_metacity" = "xyes")
if test "$use_metacity" = yes; then
  AC_DEFINE(USE_METACITY, 1, [Build metacity theme support])
  if test "$have_metacity_2_15_21" = yes; then
    AC_DEFINE(HAVE_METACITY_2_15_21, 1,
	      [Define to 1 if metacity version >= 2.15.21])
  fi
  if test "$have_metacity_2_17_0" = yes; then
    AC_DEFINE(HAVE_METACITY_2_17_0, 1,
	      [Define to 1 if metacity version >= 2.17.0])
  fi
fi

AM_CONDITIONAL(USE_GNOME, test "x$use_gnome" = "xyes")
if test "$use_gnome" = yes; then
  AC_DEFINE(USE_GNOME, 1, [Build gnome settings module])
fi

AC_ARG_ENABLE(kde,
  [  --disable-kde           Disable kde window decorator],
  [use_kde=$enableval], [use_kde=yes])

if test "x$use_kde" = "xyes"; then
  qt_incdirs="$QTINC /usr/local/qt/include /usr/include/qt /usr/include /usr/X11R6/include/X11/qt /usr/X11R6/include/qt /usr/lib/qt3/include /usr/lib/qt/include /usr/share/qt3/include"
  qt_libdirs="$QTLIB /usr/local/qt/lib /usr/lib/qt /usr/lib /usr/X11R6/lib/X11/qt /usr/X11R6/lib/qt /usr/lib/qt3/lib /usr/lib/qt/lib /usr/share/qt3/lib"

  if test -n "$QTDIR" ; then
    qt_incdirs="$QTDIR/include $qt_incdirs"
    qt_libdirs="$QTDIR/lib $qt_libdirs"
  fi

  qt_test_include="qstyle.h"
  qt_test_library="libqt-mt.so"

  dnl Check for Qt headers
  AC_MSG_CHECKING([for Qt headers])
  qt_incdir="no"
  for it in $qt_incdirs ; do
    if test -r "$it/$qt_test_include" ; then
      qt_incdir="$it"
      break
    fi
  done
  AC_MSG_RESULT([$qt_incdir])

  dnl Check for Qt libraries
  AC_MSG_CHECKING([for Qt libraries])
  qt_libdir="no"
  for qt_check in $qt_libdirs ; do
    if test -r "$qt_check/$qt_test_library" ; then
      qt_libdir="$qt_check"
      break
    fi
  done
  AC_MSG_RESULT([$qt_libdir])

  use_kde=no;
  if test "x$qt_libdir" != "xno" ; then
    if test "x$qt_incdir" != "xno" ; then
      use_kde=yes;
    fi
  fi

  QT_CXXFLAGS="-I$qt_incdir"
  QT_LIBS="-L$qt_libdir"

  if test "x$use_kde" = xyes; then
    kdedir=`kde-config --prefix --expandvars 2>/dev/null`
    kdelibs=`kde-config --install lib --expandvars 2>/dev/null`
    kdeincs=`kde-config --install include --expandvars 2>/dev/null`
    if test -d $kdelibs; then
      PKG_CHECK_MODULES(KDE_WINDOW_DECORATOR,
			dbus-1 xdamage xcomposite,
			[use_kde=yes], [use_kde=no])

      KDE_CFLAGS="$QT_CXXFLAGS -I$kdeincs"
      KDE_LIBS="$QT_LIBS -L$kdelibs -lkdecore -lkdecorations -ldbus-qt-1"

      QT_MOC=$QTDIR/bin/moc
      DCOPIDL=$kdedir/bin/dcopidl
      DCOPIDL2CPP=$kdedir/bin/dcopidl2cpp
    else
      use_kde=no;
    fi
  fi
fi

AM_CONDITIONAL(USE_KDE, test "x$use_kde" = "xyes")
if test "$use_kde" = yes; then
  AC_DEFINE(USE_KDE, 1, [Build kde window decorator])
fi

AC_SUBST(KDE_CFLAGS)
AC_SUBST(KDE_LIBS)
AC_SUBST(QT_MOC)
AC_SUBST(DCOPIDL)
AC_SUBST(DCOPIDL2CPP)

AC_ARG_WITH(max-desktops,
  [  --with-max-desktops     Max reasonable desktops],
  [MAX_DESKTOPS=$withval],
  [MAX_DESKTOPS=36])

if test -n "$MAX_DESKTOPS"; then
  AC_DEFINE_UNQUOTED(MAX_DESKTOPS, $MAX_DESKTOPS, [Max reasonable desktops])
fi

AC_OUTPUT([
compiz.pc
Makefile
src/Makefile
libdecoration/Makefile
libdecoration/libdecoration.pc
include/Makefile
plugins/Makefile
plugins/compiz.schemas
images/Makefile
gtk/Makefile
gtk/window-decorator/Makefile
gtk/window-decorator/gwd.schemas
gtk/gnome/Makefile
gtk/gnome/compiz.desktop
gtk/gnome/50-compiz-key.xml
gtk/gnome/50-compiz-desktop-key.xml
kde/Makefile
kde/window-decorator/Makefile
po/Makefile.in
metadata/Makefile
])

echo ""
echo "the following optional plugins will be compiled:"
echo "  gconf:    $use_gconf"
echo "  place:    $use_place"
echo "  dbus:     $use_dbus"
echo "  annotate: $use_annotate"
echo "  svg:      $use_librsvg"
echo "  inotify:  $use_inotify"
echo "  fuse:     $use_fuse"
echo ""
echo "and the following optional features will be compiled:"
echo "  svg:      $use_librsvg"
echo "  gtk:      $use_gtk"
echo "  metacity: $use_metacity"
echo "  gnome:    $use_gnome"
echo "  kde:      $use_kde"
echo ""
