AC_PREREQ(2.57)

AC_INIT([compiz], [0.0.1], [davidr@novell.com])

AC_CONFIG_AUX_DIR(config)

AM_INIT_AUTOMAKE([dist-bzip2])
AC_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h unistd.h])

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
  *[[\ \	]]-fno-strict-aliasing[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -fno-strict-aliasing" ;;
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

COMPIZ_REQUIRES="libpng	    \
		 xcomposite \
		 xfixes	    \
		 xdamage    \
		 xrandr	    \
		 ice	    \
		 sm	    \
		 libstartup-notification-1.0 >= 0.7"

PKG_CHECK_MODULES(COMPIZ, $COMPIZ_REQUIRES)
AC_SUBST(COMPIZ_REQUIRES)

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
  PKG_CHECK_MODULES(GCONF, gconf-2.0, [use_gconf=yes], [use_gconf=no])
fi

AM_CONDITIONAL(GCONF_PLUGIN, test "x$use_gconf" = "xyes")
if test "$use_gconf" = yes; then
  AC_DEFINE(USE_GCONF, 1, [Build gconf plugin])
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

AC_ARG_ENABLE(menu,
  [  --disable-menu          Disable window menu plugin],
  [use_menu=$enableval], [use_menu=yes])

if test "x$use_menu" = "xyes"; then
  PKG_CHECK_MODULES(MENU, gtk+-2.0, [use_menu=yes], [use_menu=no])
fi

AM_CONDITIONAL(MENU_PLUGIN, test "x$use_menu" = "xyes")
if test "$use_menu" = yes; then
  AC_DEFINE(USE_MENU, 1, [Build menu plugin])
fi

AC_ARG_ENABLE(libsvg-cairo,
  [  --enable-libsvg-cairo   Enable svg support],
  [use_libsvg_cairo=$enableval], [use_libsvg_cairo=no])

if test "x$use_libsvg_cairo" = "xyes"; then
  PKG_CHECK_MODULES(LIBSVG_CAIRO, libsvg-cairo,
		    [use_libsvg_cairo=yes],
		    [use_libsvg_cairo=no])
fi

AM_CONDITIONAL(USE_LIBSVG_CAIRO, [test x$use_libsvg_cairo = xyes])
if test "$use_libsvg_cairo" = yes; then
  AC_DEFINE(USE_LIBSVG_CAIRO, 1, [libsvg-cairo for SVG support])
fi

AC_ARG_ENABLE(gnome,
  [  --disable-gnome         Disable gnome window decorator and settings module],
  [use_gnome=$enableval], [use_gnome=yes])

if test "x$use_gnome" = "xyes"; then
  PKG_CHECK_MODULES(GNOME_WINDOW_DECORATOR,
		    gtk+-2.0 libwnck-1.0 pangocairo,
		    [use_gnome=yes], [use_gnome=no])
  if test "x$use_gnome" = "xyes"; then
    PKG_CHECK_MODULES(GNOME_WINDOW_SETTINGS,
		      gnome-window-settings-2.0 gnome-desktop-2.0,
		      [use_gnome=yes], [use_gnome=no])
  fi
  windowsettingsdatadir=`pkg-config --variable=prefix gnome-window-settings-2.0`/share
  windowsettingslibdir=`pkg-config --variable=libdir gnome-window-settings-2.0`
fi

AC_SUBST(windowsettingsdatadir)
AC_SUBST(windowsettingslibdir)

AM_CONDITIONAL(USE_GNOME, test "x$use_gnome" = "xyes")
if test "$use_gnome" = yes; then
  AC_DEFINE(USE_GNOME, 1, [Build gnome window decorator and settings module])
fi

AC_ARG_ENABLE(kde,
  [  --disable-kde           Disable kde window decorator],
  [use_kde=$enableval], [use_kde=yes])

if test "x$use_kde" = "xyes"; then
  PKG_CHECK_MODULES(KDE_WINDOW_DECORATOR, 
		    QtCore QtGui,
		    [use_kde=yes], [use_kde=no])
fi

AM_CONDITIONAL(USE_KDE, test "x$use_kde" = "xyes")
if test "$use_kde" = yes; then
  AC_DEFINE(USE_KDE, 1, [Build kde window decorator])
  QT_MOC=`$PKG_CONFIG --variable=exec_prefix QtCore`
  QT_MOC=${QT_MOC}/bin/moc
fi
AC_SUBST(QT_MOC)

AC_OUTPUT([
compiz.pc
Makefile
src/Makefile
include/Makefile
plugins/Makefile
images/Makefile
gnome/Makefile
gnome/window-decorator/Makefile
gnome/compiz.desktop
kde/Makefile
kde/window-decorator/Makefile
])

echo ""
echo "the following optional plugins will be compiled:"
echo "  gconf: $use_gconf"
echo "  place: $use_place"
echo "  menu:  $use_menu"
echo "  svg:   $use_libsvg_cairo"
echo ""
echo "and the following optional features will be compiled:"
echo "  gnome: $use_gnome"
echo "  kde:   $use_kde"
echo ""
