/*
 * Copyright © 2006 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#ifndef GTK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#endif

#include <gtk/gtk.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdkx.h>

#include <gconf/gconf-client.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>
#include <libwnck/window-action-menu.h>

#include <cairo.h>
#include <cairo-xlib.h>

#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1, 1, 0)
#define CAIRO_EXTEND_PAD CAIRO_EXTEND_NONE
#endif

#include <pango/pango-context.h>
#include <pango/pangocairo.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#define GCONF_DIR "/apps/metacity/general"

#define COMPIZ_USE_SYSTEM_FONT_KEY	 \
    GCONF_DIR "/titlebar_uses_system_font"

#define COMPIZ_TITLEBAR_FONT_KEY \
    GCONF_DIR "/titlebar_font"

#define STROKE_ALPHA 0.6

#define LEFT_SPACE   12
#define RIGHT_SPACE  14
#define TOP_SPACE    10
#define BOTTOM_SPACE 14

#define ICON_SPACE   20
#define BUTTON_SPACE 52

typedef struct _extents {
    gint left;
    gint right;
    gint top;
    gint bottom;
} extents;

#define GRAVITY_WEST  (0)
#define GRAVITY_EAST  (1 << 0)
#define GRAVITY_NORTH (0)
#define GRAVITY_SOUTH (1 << 1)

#define ALIGN_LEFT   (0)
#define ALIGN_RIGHT  (1 << 0)
#define ALIGN_TOP    (0)
#define ALIGN_BOTTOM (1 << 1)

#define XX_MASK (1 << 6)
#define XY_MASK (1 << 7)
#define YX_MASK (1 << 8)
#define YY_MASK (1 << 9)

#define WM_MOVERESIZE_SIZE_TOPLEFT      0
#define WM_MOVERESIZE_SIZE_TOP          1
#define WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define WM_MOVERESIZE_SIZE_RIGHT        3
#define WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define WM_MOVERESIZE_SIZE_BOTTOM       5
#define WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define WM_MOVERESIZE_SIZE_LEFT         7
#define WM_MOVERESIZE_MOVE              8
#define WM_MOVERESIZE_SIZE_KEYBOARD     9
#define WM_MOVERESIZE_MOVE_KEYBOARD    10

typedef struct _point {
    gint x;
    gint y;
    gint gravity;
} point;

typedef struct _quad {
    point	   p1;
    point	   p2;
    gint	   max_width;
    gint	   max_height;
    gint	   align;
    cairo_matrix_t m;
} quad;

#ifdef __SUNPRO_C
#pragma align 4 (_shadow)
#endif
#ifdef __GNUC__
static const guint8 _shadow[] __attribute__ ((__aligned__ (4))) =
#else
static const guint8 _shadow[] =
#endif
{ ""
  /* Pixbuf magic (0x47646b50) */
  "GdkP"
  /* length: header (24) + pixel_data (1705) */
  "\0\0\6\301"
  /* pixdata_type (0x2010002) */
  "\2\1\0\2"
  /* rowstride (96) */
  "\0\0\0`"
  /* width (24) */
  "\0\0\0\30"
  /* height (22) */
  "\0\0\0\26"
  /* pixel_data: */
  "\207\0\0\0\0\212\0\0\0\1\214\0\0\0\0\2\0\0\0\1\0\0\0\2\202\0\0\0\3\202"
  "\0\0\0\4\202\0\0\0\5\202\0\0\0\4\202\0\0\0\3\2\0\0\0\2\0\0\0\1\210\0"
  "\0\0\0\10\0\0\0\1\0\0\0\2\0\0\0\3\0\0\0\5\0\0\0\6\0\0\0\10\0\0\0\11\0"
  "\0\0\12\202\0\0\0\13\10\0\0\0\12\0\0\0\11\0\0\0\10\0\0\0\6\0\0\0\5\0"
  "\0\0\3\0\0\0\2\0\0\0\1\206\0\0\0\0\10\0\0\0\2\0\0\0\3\0\0\0\6\0\0\0\11"
  "\0\0\0\14\0\0\0\17\0\0\0\22\0\0\0\23\202\0\0\0\24\11\0\0\0\23\0\0\0\22"
  "\0\0\0\17\0\0\0\14\0\0\0\11\0\0\0\6\0\0\0\3\0\0\0\2\0\0\0\1\204\0\0\0"
  "\0\11\0\0\0\1\0\0\0\3\0\0\0\6\0\0\0\12\0\0\0\17\0\0\0\24\0\0\0\32\0\0"
  "\0\36\0\0\0\40\202\0\0\0\"\11\0\0\0\40\0\0\0\36\0\0\0\32\0\0\0\24\0\0"
  "\0\17\0\0\0\12\0\0\0\6\0\0\0\3\0\0\0\1\203\0\0\0\0\12\0\0\0\1\0\0\0\2"
  "\0\0\0\5\0\0\0\11\0\0\0\17\0\0\0\26\0\0\0\36\0\0\0&\0\0\0,\0\0\0""0\202"
  "\0\0\0""2\12\0\0\0""0\0\0\0,\0\0\0&\0\0\0\37\0\0\0\27\0\0\0\17\0\0\0"
  "\11\0\0\0\5\0\0\0\2\0\0\0\1\202\0\0\0\0\12\0\0\0\1\0\0\0\3\0\0\0\6\0"
  "\0\0\14\0\0\0\24\0\0\0\37\0\0\0)\0\0\0""4\0\0\0<\0\0\0A\202\0\0\0D\12"
  "\0\0\0A\0\0\0<\0\0\0""4\0\0\0)\0\0\0\37\0\0\0\24\0\0\0\14\0\0\0\6\0\0"
  "\0\3\0\0\0\1\202\0\0\0\0\12\0\0\0\1\0\0\0\4\0\0\0\10\0\0\0\17\0\0\0\31"
  "\0\0\0&\0\0\0""3\0\0\0@\0\0\0J\0\0\0Q\202\0\0\0T\12\0\0\0Q\0\0\0J\0\0"
  "\0@\0\0\0""3\0\0\0&\0\0\0\32\0\0\0\17\0\0\0\10\0\0\0\4\0\0\0\1\202\0"
  "\0\0\0\12\0\0\0\1\0\0\0\4\0\0\0\11\0\0\0\22\0\0\0\35\0\0\0,\0\0\0;\0"
  "\0\0J\0\0\0V\0\0\0]\202\0\0\0a\12\0\0\0]\0\0\0V\0\0\0J\0\0\0<\0\0\0,"
  "\0\0\0\36\0\0\0\22\0\0\0\11\0\0\0\4\0\0\0\2\202\0\0\0\0\12\0\0\0\2\0"
  "\0\0\5\0\0\0\12\0\0\0\23\0\0\0\40\0\0\0""0\0\0\0A\0\0\0Q\0\0\0^\0\0\0"
  "f\202\0\0\0k\12\0\0\0f\0\0\0^\0\0\0Q\0\0\0A\0\0\0""0\0\0\0\40\0\0\0\24"
  "\0\0\0\12\0\0\0\5\0\0\0\2\202\0\0\0\0\12\0\0\0\2\0\0\0\5\0\0\0\13\0\0"
  "\0\24\0\0\0\"\0\0\0""2\0\0\0D\0\0\0T\0\0\0a\0\0\0j\202\0\0\0o\12\0\0"
  "\0j\0\0\0a\0\0\0T\0\0\0D\0\0\0""2\0\0\0\"\0\0\0\24\0\0\0\13\0\0\0\5\0"
  "\0\0\2\202\0\0\0\0\12\0\0\0\2\0\0\0\5\0\0\0\13\0\0\0\24\0\0\0\"\0\0\0"
  "2\0\0\0D\0\0\0T\0\0\0a\0\0\0j\202\0\0\0o\12\0\0\0j\0\0\0a\0\0\0T\0\0"
  "\0D\0\0\0""2\0\0\0\"\0\0\0\24\0\0\0\13\0\0\0\5\0\0\0\2\202\0\0\0\0\12"
  "\0\0\0\2\0\0\0\5\0\0\0\12\0\0\0\23\0\0\0\40\0\0\0""0\0\0\0A\0\0\0Q\0"
  "\0\0^\0\0\0f\202\0\0\0k\12\0\0\0f\0\0\0^\0\0\0Q\0\0\0A\0\0\0""0\0\0\0"
  "\40\0\0\0\24\0\0\0\12\0\0\0\5\0\0\0\2\202\0\0\0\0\12\0\0\0\1\0\0\0\4"
  "\0\0\0\11\0\0\0\22\0\0\0\35\0\0\0,\0\0\0;\0\0\0J\0\0\0V\0\0\0]\202\0"
  "\0\0a\12\0\0\0]\0\0\0V\0\0\0J\0\0\0<\0\0\0,\0\0\0\36\0\0\0\22\0\0\0\11"
  "\0\0\0\4\0\0\0\2\202\0\0\0\0\12\0\0\0\1\0\0\0\4\0\0\0\10\0\0\0\17\0\0"
  "\0\31\0\0\0&\0\0\0""3\0\0\0@\0\0\0J\0\0\0Q\202\0\0\0T\12\0\0\0Q\0\0\0"
  "J\0\0\0@\0\0\0""3\0\0\0&\0\0\0\32\0\0\0\17\0\0\0\10\0\0\0\4\0\0\0\1\202"
  "\0\0\0\0\12\0\0\0\1\0\0\0\3\0\0\0\6\0\0\0\14\0\0\0\24\0\0\0\37\0\0\0"
  ")\0\0\0""4\0\0\0<\0\0\0A\202\0\0\0D\12\0\0\0A\0\0\0<\0\0\0""4\0\0\0)"
  "\0\0\0\37\0\0\0\24\0\0\0\14\0\0\0\6\0\0\0\3\0\0\0\1\202\0\0\0\0\12\0"
  "\0\0\1\0\0\0\2\0\0\0\5\0\0\0\11\0\0\0\17\0\0\0\26\0\0\0\36\0\0\0&\0\0"
  "\0,\0\0\0""0\202\0\0\0""2\12\0\0\0""0\0\0\0,\0\0\0&\0\0\0\37\0\0\0\27"
  "\0\0\0\17\0\0\0\11\0\0\0\5\0\0\0\2\0\0\0\1\203\0\0\0\0\11\0\0\0\1\0\0"
  "\0\3\0\0\0\6\0\0\0\12\0\0\0\17\0\0\0\24\0\0\0\32\0\0\0\36\0\0\0\40\202"
  "\0\0\0\"\11\0\0\0\40\0\0\0\36\0\0\0\32\0\0\0\24\0\0\0\17\0\0\0\12\0\0"
  "\0\6\0\0\0\3\0\0\0\1\205\0\0\0\0\10\0\0\0\2\0\0\0\3\0\0\0\6\0\0\0\11"
  "\0\0\0\14\0\0\0\17\0\0\0\22\0\0\0\23\202\0\0\0\24\11\0\0\0\23\0\0\0\22"
  "\0\0\0\17\0\0\0\14\0\0\0\11\0\0\0\6\0\0\0\3\0\0\0\2\0\0\0\1\205\0\0\0"
  "\0\10\0\0\0\1\0\0\0\2\0\0\0\3\0\0\0\5\0\0\0\6\0\0\0\10\0\0\0\11\0\0\0"
  "\12\202\0\0\0\13\10\0\0\0\12\0\0\0\11\0\0\0\10\0\0\0\6\0\0\0\5\0\0\0"
  "\3\0\0\0\2\0\0\0\1\210\0\0\0\0\2\0\0\0\1\0\0\0\2\202\0\0\0\3\202\0\0"
  "\0\4\202\0\0\0\5\202\0\0\0\4\202\0\0\0\3\2\0\0\0\2\0\0\0\1\214\0\0\0"
  "\0\212\0\0\0\1\207\0\0\0\0"
};

static extents _shadow_extents = { 0, 0, 0, 0 };

static quad _shadow_quads[] = {
    {
	{ -6, -5, GRAVITY_NORTH | GRAVITY_WEST },
	{ -5, 0, GRAVITY_NORTH | GRAVITY_EAST },
	11, SHRT_MAX,
	ALIGN_LEFT,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0, 0.0
	}
    }, {
	{ 5, -5, GRAVITY_NORTH | GRAVITY_WEST },
	{ -4, 0, GRAVITY_NORTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    0.0, 0.0,
	    0.0, 1.0,
	    11.0, 0.0
	}
    }, {
	{ -4, -5, GRAVITY_NORTH | GRAVITY_EAST },
	{ 8, 0, GRAVITY_NORTH | GRAVITY_EAST },
	12, SHRT_MAX,
	ALIGN_RIGHT,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    12.0, 0.0
	}
    },

    {
	{ -6, 0, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, -5, GRAVITY_SOUTH | GRAVITY_WEST },
	SHRT_MAX, 5,
	ALIGN_TOP,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0, 6.0
	}
    }, {
	{ -6, 5, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, -4, GRAVITY_SOUTH | GRAVITY_WEST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 0.0,
	    0.0, 11.0
	}
    }, {
	{ -6, 6, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, 0, GRAVITY_SOUTH | GRAVITY_WEST },
	SHRT_MAX, 4,
	ALIGN_BOTTOM,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0, 12.0
	}
    },

    {
	{ 0, 0, GRAVITY_NORTH | GRAVITY_EAST },
	{ 8, -5, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, 5,
	ALIGN_TOP,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    16.0, 6.0
	}
    }, {
	{ 0, 5, GRAVITY_NORTH | GRAVITY_EAST },
	{ 8, -4, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 0.0,
	    16.0, 11.0
	}
    }, {
	{ 0, 6, GRAVITY_NORTH | GRAVITY_EAST },
	{ 8, 0, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, 4,
	ALIGN_BOTTOM,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    16.0, 12.0
	}
    },

    {
	{ -6, 0, GRAVITY_SOUTH | GRAVITY_WEST },
	{ -5, 7, GRAVITY_SOUTH | GRAVITY_EAST },
	11, SHRT_MAX,
	ALIGN_LEFT,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0, 15.0
	}
    }, {
	{ 5,  0, GRAVITY_SOUTH | GRAVITY_WEST },
	{ -4, 7, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    0.0, 0.0,
	    0.0, 1.0,
	    11.0, 15.0
	}
    }, {
	{ -4, 0, GRAVITY_SOUTH | GRAVITY_EAST },
	{ 8, 7, GRAVITY_SOUTH | GRAVITY_EAST },
	12, SHRT_MAX,
	ALIGN_RIGHT,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    12.0, 15.0
	}
    }
};

#ifdef __SUNPRO_C
#pragma align 4 (_large_shadow)
#endif
#ifdef __GNUC__
static const guint8 _large_shadow[] __attribute__ ((__aligned__ (4))) =
#else
    static const guint8 _large_shadow[] =
#endif
{ ""
  /* Pixbuf magic (0x47646b50) */
  "GdkP"
  /* length: header (24) + pixel_data (2916) */
  "\0\0\13|"
  /* pixdata_type (0x1010002) */
  "\1\1\0\2"
  /* rowstride (108) */
  "\0\0\0l"
  /* width (27) */
  "\0\0\0\33"
  /* height (27) */
  "\0\0\0\33"
  /* pixel_data: */
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\1"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0"
  "\0\2\0\0\0\2\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\1\0\0\0\1\0\0\0\2\0\0\0\3\0\0\0\4\0\0\0\5\0\0\0\5\0\0\0\6\0\0\0\6"
  "\0\0\0\6\0\0\0\5\0\0\0\4\0\0\0\4\0\0\0\2\0\0\0\2\0\0\0\1\0\0\0\1\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\1\0\0\0\2\0\0\0\3\0\0\0\4\0\0\0\6\0\0\0\10\0\0\0\12\0\0\0\13\0"
  "\0\0\15\0\0\0\15\0\0\0\14\0\0\0\13\0\0\0\11\0\0\0\7\0\0\0\5\0\0\0\4\0"
  "\0\0\2\0\0\0\1\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\1\0\0\0\2\0\0\0\3\0\0\0\5\0\0\0\10\0\0\0\13\0\0\0\16"
  "\0\0\0\22\0\0\0\24\0\0\0\26\0\0\0\26\0\0\0\26\0\0\0\23\0\0\0\21\0\0\0"
  "\15\0\0\0\12\0\0\0\7\0\0\0\5\0\0\0\2\0\0\0\1\0\0\0\1\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\2\0\0\0\3\0\0\0\6\0\0\0\11\0\0"
  "\0\16\0\0\0\23\0\0\0\30\0\0\0\35\0\0\0\40\0\0\0#\0\0\0#\0\0\0\"\0\0\0"
  "\37\0\0\0\33\0\0\0\26\0\0\0\21\0\0\0\14\0\0\0\10\0\0\0\5\0\0\0\2\0\0"
  "\0\1\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\1\0\0\0\3\0\0\0\5"
  "\0\0\0\12\0\0\0\17\0\0\0\26\0\0\0\35\0\0\0$\0\0\0+\0\0\0""0\0\0\0""4"
  "\0\0\0""4\0\0\0""2\0\0\0.\0\0\0)\0\0\0\"\0\0\0\32\0\0\0\23\0\0\0\15\0"
  "\0\0\10\0\0\0\5\0\0\0\2\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0"
  "\2\0\0\0\4\0\0\0\10\0\0\0\16\0\0\0\26\0\0\0\37\0\0\0(\0\0\0""2\0\0\0"
  ";\0\0\0A\0\0\0F\0\0\0G\0\0\0D\0\0\0@\0\0\0""8\0\0\0/\0\0\0%\0\0\0\34"
  "\0\0\0\23\0\0\0\14\0\0\0\7\0\0\0\4\0\0\0\2\0\0\0\1\0\0\0\0\0\0\0\1\0"
  "\0\0\1\0\0\0\3\0\0\0\7\0\0\0\13\0\0\0\23\0\0\0\35\0\0\0)\0\0\0""5\0\0"
  "\0A\0\0\0L\0\0\0S\0\0\0X\0\0\0Y\0\0\0W\0\0\0Q\0\0\0I\0\0\0=\0\0\0""1"
  "\0\0\0%\0\0\0\32\0\0\0\21\0\0\0\12\0\0\0\5\0\0\0\2\0\0\0\1\0\0\0\0\0"
  "\0\0\1\0\0\0\2\0\0\0\4\0\0\0\10\0\0\0\17\0\0\0\31\0\0\0%\0\0\0""3\0\0"
  "\0A\0\0\0P\0\0\0[\0\0\0d\0\0\0i\0\0\0k\0\0\0h\0\0\0b\0\0\0X\0\0\0L\0"
  "\0\0=\0\0\0/\0\0\0\"\0\0\0\26\0\0\0\15\0\0\0\7\0\0\0\4\0\0\0\2\0\0\0"
  "\1\0\0\0\1\0\0\0\2\0\0\0\5\0\0\0\13\0\0\0\23\0\0\0\36\0\0\0,\0\0\0=\0"
  "\0\0M\0\0\0\\\0\0\0i\0\0\0r\0\0\0w\0\0\0y\0\0\0v\0\0\0p\0\0\0e\0\0\0"
  "X\0\0\0I\0\0\0""8\0\0\0)\0\0\0\33\0\0\0\21\0\0\0\12\0\0\0\5\0\0\0\2\0"
  "\0\0\1\0\0\0\1\0\0\0\3\0\0\0\7\0\0\0\15\0\0\0\26\0\0\0#\0\0\0""3\0\0"
  "\0D\0\0\0V\0\0\0f\0\0\0s\0\0\0|\0\0\0\202\0\0\0\203\0\0\0\200\0\0\0z"
  "\0\0\0p\0\0\0b\0\0\0R\0\0\0@\0\0\0/\0\0\0\40\0\0\0\24\0\0\0\13\0\0\0"
  "\6\0\0\0\2\0\0\0\1\0\0\0\1\0\0\0\4\0\0\0\7\0\0\0\16\0\0\0\31\0\0\0'\0"
  "\0\0""8\0\0\0K\0\0\0^\0\0\0n\0\0\0{\0\0\0\204\0\0\0\211\0\0\0\213\0\0"
  "\0\210\0\0\0\202\0\0\0x\0\0\0k\0\0\0Z\0\0\0G\0\0\0""5\0\0\0%\0\0\0\27"
  "\0\0\0\15\0\0\0\7\0\0\0\3\0\0\0\1\0\0\0\1\0\0\0\4\0\0\0\10\0\0\0\20\0"
  "\0\0\33\0\0\0*\0\0\0<\0\0\0O\0\0\0b\0\0\0s\0\0\0\200\0\0\0\211\0\0\0"
  "\216\0\0\0\217\0\0\0\215\0\0\0\210\0\0\0~\0\0\0p\0\0\0_\0\0\0M\0\0\0"
  "9\0\0\0(\0\0\0\31\0\0\0\16\0\0\0\10\0\0\0\4\0\0\0\1\0\0\0\1\0\0\0\3\0"
  "\0\0\7\0\0\0\16\0\0\0\27\0\0\0%\0\0\0""6\0\0\0I\0\0\0[\0\0\0k\0\0\0y"
  "\0\0\0\202\0\0\0\210\0\0\0\211\0\0\0\210\0\0\0\202\0\0\0y\0\0\0k\0\0"
  "\0[\0\0\0I\0\0\0""6\0\0\0%\0\0\0\27\0\0\0\16\0\0\0\7\0\0\0\3\0\0\0\1"
  "\0\0\0\1\0\0\0\2\0\0\0\6\0\0\0\14\0\0\0\25\0\0\0!\0\0\0""1\0\0\0A\0\0"
  "\0S\0\0\0c\0\0\0p\0\0\0z\0\0\0\200\0\0\0\202\0\0\0\200\0\0\0z\0\0\0p"
  "\0\0\0c\0\0\0S\0\0\0A\0\0\0""0\0\0\0!\0\0\0\24\0\0\0\14\0\0\0\6\0\0\0"
  "\3\0\0\0\1\0\0\0\1\0\0\0\2\0\0\0\5\0\0\0\12\0\0\0\21\0\0\0\34\0\0\0)"
  "\0\0\0""9\0\0\0I\0\0\0X\0\0\0e\0\0\0n\0\0\0t\0\0\0v\0\0\0t\0\0\0n\0\0"
  "\0e\0\0\0X\0\0\0I\0\0\0""9\0\0\0)\0\0\0\34\0\0\0\21\0\0\0\12\0\0\0\5"
  "\0\0\0\2\0\0\0\1\0\0\0\1\0\0\0\2\0\0\0\4\0\0\0\10\0\0\0\16\0\0\0\27\0"
  "\0\0\"\0\0\0/\0\0\0=\0\0\0K\0\0\0W\0\0\0`\0\0\0e\0\0\0h\0\0\0e\0\0\0"
  "`\0\0\0W\0\0\0K\0\0\0=\0\0\0/\0\0\0\"\0\0\0\27\0\0\0\16\0\0\0\10\0\0"
  "\0\4\0\0\0\2\0\0\0\1\0\0\0\0\0\0\0\1\0\0\0\2\0\0\0\5\0\0\0\12\0\0\0\21"
  "\0\0\0\32\0\0\0%\0\0\0""1\0\0\0=\0\0\0G\0\0\0O\0\0\0U\0\0\0V\0\0\0U\0"
  "\0\0O\0\0\0G\0\0\0=\0\0\0""1\0\0\0%\0\0\0\32\0\0\0\21\0\0\0\12\0\0\0"
  "\5\0\0\0\2\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\2\0\0\0\4\0\0\0\7\0"
  "\0\0\14\0\0\0\23\0\0\0\34\0\0\0%\0\0\0.\0\0\0""7\0\0\0>\0\0\0B\0\0\0"
  "C\0\0\0B\0\0\0>\0\0\0""7\0\0\0.\0\0\0%\0\0\0\34\0\0\0\23\0\0\0\14\0\0"
  "\0\7\0\0\0\4\0\0\0\2\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\2"
  "\0\0\0\5\0\0\0\10\0\0\0\15\0\0\0\23\0\0\0\32\0\0\0!\0\0\0(\0\0\0,\0\0"
  "\0""0\0\0\0""1\0\0\0""0\0\0\0,\0\0\0(\0\0\0!\0\0\0\32\0\0\0\23\0\0\0"
  "\15\0\0\0\10\0\0\0\5\0\0\0\2\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\1\0\0\0\1\0\0\0\2\0\0\0\5\0\0\0\10\0\0\0\14\0\0\0\20\0\0\0\26"
  "\0\0\0\32\0\0\0\36\0\0\0\40\0\0\0!\0\0\0\40\0\0\0\36\0\0\0\32\0\0\0\26"
  "\0\0\0\20\0\0\0\14\0\0\0\10\0\0\0\5\0\0\0\2\0\0\0\1\0\0\0\1\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\1\0\0\0\2\0\0\0\4\0\0\0"
  "\7\0\0\0\12\0\0\0\15\0\0\0\20\0\0\0\23\0\0\0\24\0\0\0\24\0\0\0\24\0\0"
  "\0\23\0\0\0\20\0\0\0\15\0\0\0\12\0\0\0\7\0\0\0\4\0\0\0\2\0\0\0\1\0\0"
  "\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1"
  "\0\0\0\1\0\0\0\2\0\0\0\4\0\0\0\5\0\0\0\7\0\0\0\10\0\0\0\12\0\0\0\13\0"
  "\0\0\13\0\0\0\13\0\0\0\12\0\0\0\10\0\0\0\7\0\0\0\5\0\0\0\4\0\0\0\2\0"
  "\0\0\1\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\1\0\0\0\2\0\0\0\2\0\0\0\4\0\0\0\4\0"
  "\0\0\5\0\0\0\5\0\0\0\6\0\0\0\5\0\0\0\5\0\0\0\4\0\0\0\4\0\0\0\2\0\0\0"
  "\2\0\0\0\1\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\1\0\0\0"
  "\1\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\2\0\0\0\1\0"
  "\0\0\1\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0\1\0\0\0"
  "\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0"
};

static extents _win_extents = {  6,  6, 4,  6 };

static quad _win_quads[] = {
    {
	{ -LEFT_SPACE, -TOP_SPACE, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, 0, GRAVITY_NORTH | GRAVITY_WEST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0, 0.0
	}
    }, {
	{ 0, -TOP_SPACE, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, 0, GRAVITY_NORTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	ALIGN_LEFT,
	{
	    0.0, 0.0,
	    0.0, 1.0,
	    LEFT_SPACE + 1, 0.0
	}
    }, {
	{ 0, -TOP_SPACE, GRAVITY_NORTH | GRAVITY_EAST },
	{ RIGHT_SPACE, 0, GRAVITY_NORTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    LEFT_SPACE + 1.0, 0.0
	}
    },

    {
	{ -LEFT_SPACE, 0, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, 0, GRAVITY_SOUTH | GRAVITY_WEST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 0.0,
	    0.0, TOP_SPACE + 1.0
	}
    }, {

	{  0, 0, GRAVITY_NORTH | GRAVITY_EAST },
	{ RIGHT_SPACE, 0, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 0.0,
	    LEFT_SPACE + 1.0, TOP_SPACE + 1.0
	}
    },

    {
	{ -LEFT_SPACE, 0, GRAVITY_SOUTH | GRAVITY_WEST },
	{ 0, BOTTOM_SPACE, GRAVITY_SOUTH | GRAVITY_WEST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0, TOP_SPACE + 1
	}
    }, {
	{ 0, 0, GRAVITY_SOUTH | GRAVITY_WEST },
	{ 0, BOTTOM_SPACE, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    0.0, 0.0,
	    0.0, 1.0,
	    LEFT_SPACE, TOP_SPACE + 1
	}
    }, {
	{ 0, 0, GRAVITY_SOUTH | GRAVITY_EAST },
	{ RIGHT_SPACE, BOTTOM_SPACE, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    LEFT_SPACE + 1, TOP_SPACE + 1
	}
    }
};

static quad _win_button_quads[] = {
    {
	{ 0 /* + title width */, -TOP_SPACE, GRAVITY_NORTH | GRAVITY_WEST },
	{ -BUTTON_SPACE, 0, GRAVITY_NORTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    0.0, 0.0,
	    0.0, 1.0,
	    0.0 /* title width */, 0.0
	}
    }, {
	{ 0 /* title width + 1 */, -TOP_SPACE, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, 0, GRAVITY_NORTH | GRAVITY_EAST },
	BUTTON_SPACE, SHRT_MAX,
	ALIGN_RIGHT,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0 /* title width + 1.0 */, 0.0
	}
    }
};

#define SWITCHER_SPACE 40

static extents _switcher_extents = { 0, 0, 0, 0 };

static quad _switcher_quads[] = {
    {
	{ -LEFT_SPACE, -BOTTOM_SPACE, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, 0, GRAVITY_NORTH | GRAVITY_WEST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0, 0.0
	}
    }, {
	{ 0, -BOTTOM_SPACE, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, 0, GRAVITY_NORTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	ALIGN_LEFT,
	{
	    0.0, 0.0,
	    0.0, 1.0,
	    LEFT_SPACE + 1, 0.0
	}
    }, {
	{ 0, -BOTTOM_SPACE, GRAVITY_NORTH | GRAVITY_EAST },
	{ RIGHT_SPACE, 0, GRAVITY_NORTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    LEFT_SPACE + 1.0, 0.0
	}
    },

    {
	{ -LEFT_SPACE, 0, GRAVITY_NORTH | GRAVITY_WEST },
	{ 0, 0, GRAVITY_SOUTH | GRAVITY_WEST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 0.0,
	    0.0, BOTTOM_SPACE
	}
    }, {

	{  0, 0, GRAVITY_NORTH | GRAVITY_EAST },
	{ RIGHT_SPACE, 0, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 0.0,
	    LEFT_SPACE + 1.0, BOTTOM_SPACE
	}
    },

    {
	{ -LEFT_SPACE, 0, GRAVITY_SOUTH | GRAVITY_WEST },
	{ 0, SWITCHER_SPACE + BOTTOM_SPACE, GRAVITY_SOUTH | GRAVITY_WEST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    0.0, BOTTOM_SPACE + 1
	}
    }, {
	{ 0, 0, GRAVITY_SOUTH | GRAVITY_WEST },
	{ 0, SWITCHER_SPACE + BOTTOM_SPACE, GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    0.0, 0.0,
	    0.0, 1.0,
	    LEFT_SPACE + 1, BOTTOM_SPACE + 1
	}
    }, {
	{ 0, 0, GRAVITY_SOUTH | GRAVITY_EAST },
	{ RIGHT_SPACE, SWITCHER_SPACE + BOTTOM_SPACE,
	  GRAVITY_SOUTH | GRAVITY_EAST },
	SHRT_MAX, SHRT_MAX,
	0,
	{
	    1.0, 0.0,
	    0.0, 1.0,
	    LEFT_SPACE + 1, BOTTOM_SPACE + 1
	}
    }
};

static GdkPixmap *shadow_pixmap = NULL;
static GdkPixmap *large_shadow_pixmap = NULL;
static GdkPixmap *decor_normal_pixmap = NULL;
static GdkPixmap *decor_active_pixmap = NULL;

static cairo_pattern_t *shadow_pattern;

static Atom frame_window_atom;
static Atom win_decor_atom;
static Atom wm_move_resize_atom;
static Atom restack_window_atom;
static Atom select_window_atom;

#define C(name) { 0, XC_ ## name }

static struct _cursor {
    Cursor	 cursor;
    unsigned int shape;
} cursor[3][3] = {
    { C (top_left_corner),    C (top_side),    C (top_right_corner)    },
    { C (left_side),	      C (left_ptr),    C (right_side)	       },
    { C (bottom_left_corner), C (bottom_side), C (bottom_right_corner) }
};

static struct _pos {
    int x, y, w, h;
    int xw, yh, ww, hh;
} pos[3][3] = {
    {
	{  0,  0, 10, 21,   0, 0, 0, 0 },
	{ 10,  0, -8,  6,   0, 0, 1, 0 },
	{  2,  0, 10, 21,   1, 0, 0, 0 }
    }, {
	{  0, 10,  6, 11,   0, 0, 0, 1 },
	{  6,  6,  0, 15,   0, 0, 1, 0 },
	{  6, 10,  6, 11,   1, 0, 0, 1 }
    }, {
	{  0, 17, 10, 10,   0, 1, 0, 0 },
	{ 10, 21, -8,  6,   0, 1, 1, 0 },
	{  2, 17, 10, 10,   1, 1, 0, 0 }
    }
}, bpos[3] = {
    { -10, 6, 16, 16,   1, 0, 0, 0 },
    { -26, 6, 16, 16,   1, 0, 0, 0 },
    { -42, 6, 16, 16,   1, 0, 0, 0 }
};

typedef struct _decor_color {
    double r;
    double g;
    double b;
} decor_color_t;

#define IN_EVENT_WINDOW      (1 << 0)
#define PRESSED_EVENT_WINDOW (1 << 1)

typedef struct _decor {
    Window	      event_windows[3][3];
    Window	      button_windows[3];
    guint	      button_states[3];
    GdkPixmap	      *pixmap;
    GdkPixmap	      *buffer_pixmap;
    GdkGC	      *gc;
    gint	      width;
    gint	      height;
    gboolean	      decorated;
    gboolean	      active;
    PangoLayout	      *layout;
    gchar	      *name;
    cairo_pattern_t   *icon;
    GdkPixmap	      *icon_pixmap;
    WnckWindowState   state;
    WnckWindowActions actions;
    XID		      prop_xid;
    void	      (*draw) (struct _decor *d);
} decor_t;

typedef void (*event_callback) (WnckWindow *win, XEvent *event);

static GtkWidget     *style_window;

static GHashTable    *frame_table;
static GtkWidget     *action_menu = NULL;
static gboolean      action_menu_mapped = FALSE;
static decor_color_t _title_color[2];
static PangoContext  *pango_context;
static gint	     double_click_timeout = 250;

static GtkWidget     *tip_window;
static GtkWidget     *tip_label;
static GTimeVal	     tooltip_last_popdown = { -1, -1 };
static gint	     tooltip_timer_tag = 0;

static GSList *draw_list = NULL;
static guint  draw_idle_id = 0;

static PangoFontDescription *titlebar_font = NULL;
static gboolean		    use_system_font = FALSE;
static gint		    text_height;
static gint		    titlebar_height;

static GdkPixmap *switcher_pixmap = NULL;
static GdkPixmap *switcher_buffer_pixmap = NULL;
static gint      switcher_width;
static gint      switcher_height;

/*
  decoration property
  -------------------

  data[0] = pixmap

  data[1] = input left
  data[2] = input right
  data[3] = input top
  data[4] = input bottom

  flags

  1st and 2nd bit p1 gravity, 3rd and 4th bit p2 gravity,
  5rd and 6th bit alignment, 7th bit XX, 8th bit XY, 9th bit YX, 10th bit YY.

  data[4 + n * 9 + 1] = flags
  data[4 + n * 9 + 2] = p1 x
  data[4 + n * 9 + 3] = p1 y
  data[4 + n * 9 + 4] = p2 x
  data[4 + n * 9 + 5] = p2 y
  data[4 + n * 9 + 6] = widthMax
  data[4 + n * 9 + 7] = heightMax
  data[4 + n * 9 + 8] = x0
  data[4 + n * 9 + 9] = y0
 */
static void
decoration_to_property (long	*data,
			Pixmap	pixmap,
			extents	*input,
			quad	*quad,
			int	nQuad)
{
    memcpy (data++, &pixmap, sizeof (Pixmap));

    *data++ = input->left;
    *data++ = input->right;
    *data++ = input->top;
    *data++ = input->bottom;

    while (nQuad--)
    {
	*data++ =
	    (quad->p1.gravity << 0)    |
	    (quad->p2.gravity << 2)    |
	    (quad->align      << 4)    |
	    (quad->m.xx ? XX_MASK : 0) |
	    (quad->m.xy ? XY_MASK : 0) |
	    (quad->m.yx ? YX_MASK : 0) |
	    (quad->m.yy ? YY_MASK : 0);

	*data++ = quad->p1.x;
	*data++ = quad->p1.y;
	*data++ = quad->p2.x;
	*data++ = quad->p2.y;
	*data++ = quad->max_width;
	*data++ = quad->max_height;
	*data++ = quad->m.x0;
	*data++ = quad->m.y0;

	quad++;
    }
}

static void
decor_quads_init8 (quad *dst,
		   quad *src,
		   int  height)
{
    memcpy (dst, src, 8 * sizeof (quad));

    dst[0].p1.y -= height;
    dst[1].p1.y -= height;
    dst[2].p1.y -= height;

    dst[3].m.y0 += height;
    dst[4].m.y0 += height;

    dst[5].m.y0 += height;
    dst[6].m.y0 += height;
    dst[7].m.y0 += height;
}

static void
decor_update_window_property (decor_t *d)
{
    long    data[128];
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    gint    nQuad = sizeof (_win_quads) / sizeof (_win_quads[0]);
    gint    nButtonQuad = sizeof (_win_button_quads) /
	sizeof (_win_button_quads[0]);
    quad    quads[nQuad + nButtonQuad];
    extents extents = _win_extents;

    /* this is kinda hard to read */

    decor_quads_init8 (quads, _win_quads, titlebar_height);

    memcpy (quads + nQuad, _win_button_quads, nButtonQuad * sizeof (quad));

    quads[8].p1.y  -= titlebar_height;
    quads[9].p1.y  -= titlebar_height;
    quads[10].p1.y -= titlebar_height;

    quads[2].m.x0 = quads[4].m.x0 = quads[7].m.x0 = d->width - RIGHT_SPACE;
    quads[1].m.xx = 1.0;

    quads[1].max_width = d->width - LEFT_SPACE - RIGHT_SPACE - BUTTON_SPACE;

    quads[8].p1.x = d->width - LEFT_SPACE - RIGHT_SPACE - BUTTON_SPACE;
    quads[8].m.x0 = d->width - RIGHT_SPACE - BUTTON_SPACE;

    quads[9].p1.x = d->width - LEFT_SPACE - RIGHT_SPACE - BUTTON_SPACE;
    quads[9].m.x0 = d->width - RIGHT_SPACE - BUTTON_SPACE;

    quads[9].max_width = BUTTON_SPACE;

    nQuad += nButtonQuad;

    extents.top += titlebar_height;

    decoration_to_property (data, GDK_PIXMAP_XID (d->pixmap),
			    &extents, quads, nQuad);

    gdk_error_trap_push ();
    XChangeProperty (xdisplay, d->prop_xid,
		     win_decor_atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data, 5 + 9 * nQuad);
    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();
}

static void
gdk_cairo_set_source_color_alpha (cairo_t  *cr,
				  GdkColor *color,
				  double   alpha)
{
    cairo_set_source_rgba (cr,
			   color->red   / 65535.0,
			   color->green / 65535.0,
			   color->blue  / 65535.0,
			   alpha);
}

#define CORNER_TOPLEFT     (1 << 0)
#define CORNER_TOPRIGHT    (1 << 1)
#define CORNER_BOTTOMRIGHT (1 << 2)
#define CORNER_BOTTOMLEFT  (1 << 3)

static void
rounded_rectangle (cairo_t *cr,
		   double  x,
		   double  y,
		   double  w,
		   double  h,
		   double  radius,
		   int	   corner)
{
    if (corner & CORNER_TOPLEFT)
	cairo_move_to (cr, x + radius, y);
    else
	cairo_move_to (cr, x, y);

    if (corner & CORNER_TOPRIGHT)
	cairo_arc (cr, x + w - radius, y + radius, radius,
		   M_PI * 1.5, M_PI * 2.0);
    else
	cairo_line_to (cr, x + w, y);

    if (corner & CORNER_BOTTOMRIGHT)
	cairo_arc (cr, x + w - radius, y + h - radius, radius,
		   0.0, M_PI * 0.5);
    else
	cairo_line_to (cr, x + w, y + h);

    if (corner & CORNER_BOTTOMLEFT)
	cairo_arc (cr, x + radius, y + h - radius, radius,
		   M_PI * 0.5, M_PI);
    else
	cairo_line_to (cr, x, y + h);

    if (corner & CORNER_TOPLEFT)
	cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
    else
	cairo_line_to (cr, x, y);
}

#define SHADE_LEFT   (1 << 0)
#define SHADE_RIGHT  (1 << 1)
#define SHADE_TOP    (1 << 2)
#define SHADE_BOTTOM (1 << 3)

static void
fill_rounded_rectangle (cairo_t       *cr,
			double        x,
			double        y,
			double        w,
			double        h,
			double	      radius,
			int	      corner,
			decor_color_t *c0,
			double        alpha0,
			decor_color_t *c1,
			double	      alpha1,
			int	      gravity)
{
    cairo_pattern_t *pattern;

    rounded_rectangle (cr, x, y, w, h, radius, corner);

    if (gravity & SHADE_RIGHT)
    {
	x = x + w;
	w = -w;
    }
    else if (!(gravity & SHADE_LEFT))
    {
	x = w = 0;
    }

    if (gravity & SHADE_BOTTOM)
    {
	y = y + h;
	h = -h;
    }
    else if (!(gravity & SHADE_TOP))
    {
	y = h = 0;
    }

    if (w && h)
    {
	cairo_matrix_t matrix;

	pattern = cairo_pattern_create_radial (0.0, 0.0, 0.0, 0.0, 0.0, w);

	cairo_matrix_init_scale (&matrix, 1.0, w / h);
	cairo_matrix_translate (&matrix, -(x + w), -(y + h));

	cairo_pattern_set_matrix (pattern, &matrix);
    }
    else
    {
	pattern = cairo_pattern_create_linear (x + w, y + h, x, y);
    }

    cairo_pattern_add_color_stop_rgba (pattern, 0.0, c0->r, c0->g, c0->b,
				       alpha0);

    cairo_pattern_add_color_stop_rgba (pattern, 1.0, c1->r, c1->g, c1->b,
				       alpha1);

    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

    cairo_set_source (cr, pattern);
    cairo_fill (cr);
    cairo_pattern_destroy (pattern);
}

static void
draw_shadow_background (decor_t *d,
			cairo_t	*cr)
{
    cairo_matrix_t matrix;
    double	   w, h, x2, y2;

    w = d->width - 13.0 - 14.0;
    h = d->height - 13.0 - 14.0;

    x2 = d->width - 14.0;
    y2 = d->height - 14.0;

    /* top left */
    cairo_matrix_init_identity (&matrix);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, 0.0, 0.0, 13.0, 13.0);
    cairo_fill (cr);

    /* top */
    cairo_matrix_init_translate (&matrix, 13.0, 0.0);
    cairo_matrix_scale (&matrix, 1.0 / w, 1.0);
    cairo_matrix_translate (&matrix, -13.0, 0.0);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, 13.0, 0.0, w, 13.0);
    cairo_fill (cr);

    /* top right */
    cairo_matrix_init_translate (&matrix, 13.0 - x2, 0.0);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, x2, 0.0, 14.0, 13.0);
    cairo_fill (cr);

    /* left */
    cairo_matrix_init_translate (&matrix, 0.0, 13.0);
    cairo_matrix_scale (&matrix, 1.0, 1.0 / h);
    cairo_matrix_translate (&matrix, 0.0, -13.0);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, 0.0, 13.0, 13.0, h);
    cairo_fill (cr);

    /* right */
    cairo_matrix_init_translate (&matrix, 13.0 - x2, 13.0);
    cairo_matrix_scale (&matrix, 1.0, 1.0 / h);
    cairo_matrix_translate (&matrix, 0.0, -13.0);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, x2, 13.0, 14.0, h);
    cairo_fill (cr);

    /* bottom left */
    cairo_matrix_init_translate (&matrix, 0.0, 13.0 - y2);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, 0.0, y2, 13.0, 14.0);
    cairo_fill (cr);

    /* bottom */
    cairo_matrix_init_translate (&matrix, 13.0, 13.0 - y2);
    cairo_matrix_scale (&matrix, 1.0 / w, 1.0);
    cairo_matrix_translate (&matrix, -13.0, 0.0);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, 13.0, y2, w, 14.0);
    cairo_fill (cr);

    /* bottom right */
    cairo_matrix_init_translate (&matrix, 13.0 - x2, 13.0 - y2);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, x2, y2, 14.0, 14.0);
    cairo_fill (cr);
}

static void
draw_close_button (decor_t *d,
		   cairo_t *cr,
		   double  s)
{
    cairo_rel_move_to (cr, 0.0, s);

    cairo_rel_line_to (cr, s, -s);
    cairo_rel_line_to (cr, s, s);
    cairo_rel_line_to (cr, s, -s);
    cairo_rel_line_to (cr, s, s);

    cairo_rel_line_to (cr, -s, s);
    cairo_rel_line_to (cr, s, s);
    cairo_rel_line_to (cr, -s, s);
    cairo_rel_line_to (cr, -s, -s);

    cairo_rel_line_to (cr, -s, s);
    cairo_rel_line_to (cr, -s, -s);
    cairo_rel_line_to (cr, s, -s);

    cairo_close_path (cr);
}

static void
draw_max_button (decor_t *d,
		 cairo_t *cr,
		 double  s)
{
    cairo_rel_line_to (cr, 12.0, 0.0);
    cairo_rel_line_to (cr, 0.0, 12.0);
    cairo_rel_line_to (cr, -12.0, 0.0);

    cairo_close_path (cr);

    cairo_rel_move_to (cr, 2.0, s);

    cairo_rel_line_to (cr, 12.0 - 4.0, 0.0);
    cairo_rel_line_to (cr, 0.0, 12.0 - s - 2.0);
    cairo_rel_line_to (cr, -(12.0 - 4.0), 0.0);

    cairo_close_path (cr);
}

static void
draw_unmax_button (decor_t *d,
		 cairo_t *cr,
		 double  s)
{
    cairo_rel_move_to (cr, 1.0, 1.0);

    cairo_rel_line_to (cr, 10.0, 0.0);
    cairo_rel_line_to (cr, 0.0, 10.0);
    cairo_rel_line_to (cr, -10.0, 0.0);

    cairo_close_path (cr);

    cairo_rel_move_to (cr, 2.0, s);

    cairo_rel_line_to (cr, 10.0 - 4.0, 0.0);
    cairo_rel_line_to (cr, 0.0, 10.0 - s - 2.0);
    cairo_rel_line_to (cr, -(10.0 - 4.0), 0.0);

    cairo_close_path (cr);
}

static void
draw_min_button (decor_t *d,
		 cairo_t *cr,
		 double  s)
{
    cairo_rel_move_to (cr, 0.0, 8.0);

    cairo_rel_line_to (cr, 12.0, 0.0);
    cairo_rel_line_to (cr, 0.0, s);
    cairo_rel_line_to (cr, -12.0, 0.0);

    cairo_close_path (cr);
}

typedef void (*draw_proc) (cairo_t *cr);

static void
button_state_offsets (gdouble x,
		      gdouble y,
		      guint   state,
		      gdouble *return_x,
		      gdouble *return_y)
{
    static double off[]	= { 0.0, 0.0, 0.0, 0.5 };

    *return_x  = x + off[state];
    *return_y  = y + off[state];
}

static void
button_state_paint (cairo_t	  *cr,
		    GtkStyle	  *style,
		    decor_color_t *color,
		    guint	  state)
{

#define IN_STATE (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW)

    if ((state & IN_STATE) == IN_STATE)
    {
	if (state & IN_EVENT_WINDOW)
	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	else
	    cairo_set_source_rgba (cr, color->r, color->g, color->b, 0.95);

	cairo_fill_preserve (cr);

	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  STROKE_ALPHA);

	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);
	cairo_set_line_width (cr, 2.0);
    }
    else
    {
	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  STROKE_ALPHA);
	cairo_stroke_preserve (cr);

	if (state & IN_EVENT_WINDOW)
	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	else
	    cairo_set_source_rgba (cr, color->r, color->g, color->b, 0.95);

	cairo_fill (cr);
    }
}

static void
draw_window_decoration (decor_t *d)
{
    cairo_t       *cr;
    GtkStyle	  *style;
    decor_color_t color;
    double        alpha;
    double        x1, y1, x2, y2, x, y;
    int		  corners = SHADE_LEFT | SHADE_RIGHT | SHADE_TOP | SHADE_BOTTOM;
    int		  top;

    if (!d->pixmap)
	return;

    style = gtk_widget_get_style (style_window);

    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
		    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
	corners = 0;

    color.r = style->bg[GTK_STATE_NORMAL].red   / 65535.0;
    color.g = style->bg[GTK_STATE_NORMAL].green / 65535.0;
    color.b = style->bg[GTK_STATE_NORMAL].blue  / 65535.0;

    if (d->buffer_pixmap)
	cr = gdk_cairo_create (GDK_DRAWABLE (d->buffer_pixmap));
    else
	cr = gdk_cairo_create (GDK_DRAWABLE (d->pixmap));

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    top = _win_extents.top + titlebar_height;

    x1 = LEFT_SPACE - _win_extents.left;
    y1 = TOP_SPACE - _win_extents.top;
    x2 = d->width - RIGHT_SPACE + _win_extents.right;
    y2 = d->height - BOTTOM_SPACE + _win_extents.bottom;

    cairo_set_line_width (cr, 1.0);

    draw_shadow_background (d, cr);

    if (d->active)
    {
	decor_color_t *title_color = _title_color;

	alpha = 0.8;

	fill_rounded_rectangle (cr,
				x1 + 0.5,
				y1 + 0.5,
				_win_extents.left - 0.5,
				top - 0.5,
				5.0, CORNER_TOPLEFT & corners,
				&title_color[0], 1.0, &title_color[1], alpha,
				SHADE_TOP | SHADE_LEFT);

	fill_rounded_rectangle (cr,
				x1 + _win_extents.left,
				y1 + 0.5,
				x2 - x1 - _win_extents.left -
				_win_extents.right,
				top - 0.5,
				5.0, 0,
				&title_color[0], 1.0, &title_color[1], alpha,
				SHADE_TOP);

	fill_rounded_rectangle (cr,
				x2 - _win_extents.right,
				y1 + 0.5,
				_win_extents.right - 0.5,
				top - 0.5,
				5.0, CORNER_TOPRIGHT & corners,
				&title_color[0], 1.0, &title_color[1], alpha,
				SHADE_TOP | SHADE_RIGHT);
    }
    else
    {
	alpha = 0.5;

	fill_rounded_rectangle (cr,
				x1 + 0.5,
				y1 + 0.5,
				_win_extents.left - 0.5,
				top - 0.5,
				5.0, CORNER_TOPLEFT & corners,
				&color, 1.0, &color, alpha,
				SHADE_TOP | SHADE_LEFT);

	fill_rounded_rectangle (cr,
				x1 + _win_extents.left,
				y1 + 0.5,
				x2 - x1 - _win_extents.left -
				_win_extents.right,
				top - 0.5,
				5.0, 0,
				&color, 1.0, &color, alpha,
				SHADE_TOP);

	fill_rounded_rectangle (cr,
				x2 - _win_extents.right,
				y1 + 0.5,
				_win_extents.right - 0.5,
				top - 0.5,
				5.0, CORNER_TOPRIGHT & corners,
				&color, 1.0, &color, alpha,
				SHADE_TOP | SHADE_RIGHT);
    }

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + top,
			    _win_extents.left - 0.5,
			    1,
			    5.0, 0,
			    &color, 1.0, &color, alpha,
			    SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y1 + top,
			    _win_extents.right - 0.5,
			    1,
			    5.0, 0,
			    &color, 1.0, &color, alpha,
			    SHADE_RIGHT);


    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y2 - _win_extents.bottom,
			    _win_extents.left - 0.5,
			    _win_extents.bottom - 0.5,
			    5.0, CORNER_BOTTOMLEFT & corners,
			    &color, 1.0, &color, alpha,
			    SHADE_BOTTOM | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + _win_extents.left,
			    y2 - _win_extents.bottom,
			    x2 - x1 - _win_extents.left -
			    _win_extents.right,
			    _win_extents.bottom - 0.5,
			    5.0, 0,
			    &color, 1.0, &color, alpha,
			    SHADE_BOTTOM);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y2 - _win_extents.bottom,
			    _win_extents.right - 0.5,
			    _win_extents.bottom - 0.5,
			    5.0, CORNER_BOTTOMRIGHT & corners,
			    &color, 1.0, &color, alpha,
			    SHADE_BOTTOM | SHADE_RIGHT);

    cairo_rectangle (cr,
		     LEFT_SPACE, titlebar_height + TOP_SPACE,
		     d->width - LEFT_SPACE - RIGHT_SPACE, 1);
    gdk_cairo_set_source_color (cr, &style->bg[GTK_STATE_NORMAL]);
    cairo_fill (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    if (d->active)
    {
	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  0.7);

	cairo_move_to (cr, x1 + 0.5, y1 + top - 0.5);
	cairo_rel_line_to (cr, x2 - x1 - 1.0, 0.0);

	cairo_stroke (cr);
    }

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       (CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
			CORNER_BOTTOMRIGHT) & corners);

    cairo_clip (cr);

    cairo_translate (cr, 1.0, 1.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       (CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
			CORNER_BOTTOMRIGHT) & corners);

    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.4);
    cairo_stroke (cr);

    cairo_translate (cr, -2.0, -2.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       (CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
			CORNER_BOTTOMRIGHT) & corners);

    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.1);
    cairo_stroke (cr);

    cairo_translate (cr, 1.0, 1.0);

    cairo_reset_clip (cr);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       (CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
			CORNER_BOTTOMRIGHT) & corners);

    gdk_cairo_set_source_color_alpha (cr,
				      &style->fg[GTK_STATE_NORMAL],
				      alpha);

    cairo_stroke (cr);

    cairo_set_line_width (cr, 2.0);

    if (d->actions & WNCK_WINDOW_ACTION_CLOSE)
    {
	button_state_offsets (d->width - RIGHT_SPACE - BUTTON_SPACE + 39.0,
			      titlebar_height / 2 + 3.0,
			      d->button_states[0], &x, &y);

	if (d->active)
	{
	    cairo_move_to (cr, x, y);
	    draw_close_button (d, cr, 3.0);
	    button_state_paint (cr, style, &color, d->button_states[0]);
	}
	else
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
	    cairo_move_to (cr, x, y);
	    draw_close_button (d, cr, 3.0);
	    cairo_fill (cr);
	}
    }

    if (d->actions & WNCK_WINDOW_ACTION_MAXIMIZE)
    {
	button_state_offsets (d->width - RIGHT_SPACE - BUTTON_SPACE + 21.0,
			      titlebar_height / 2 + 3.0,
			      d->button_states[1], &x, &y);

	cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);

	if (d->active)
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);
	    cairo_move_to (cr, x, y);

	    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
			    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
		draw_unmax_button (d, cr, 4.0);
	    else
		draw_max_button (d, cr, 4.0);

	    button_state_paint (cr, style, &color, d->button_states[1]);
	}
	else
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
	    cairo_move_to (cr, x, y);

	    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
			    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
		draw_unmax_button (d, cr, 4.0);
	    else
		draw_max_button (d, cr, 4.0);

	    cairo_fill (cr);
	}
    }

    if (d->actions & WNCK_WINDOW_ACTION_MINIMIZE)
    {
	button_state_offsets (d->width - RIGHT_SPACE - BUTTON_SPACE + 3.0,
			      titlebar_height / 2 + 3.0,
			      d->button_states[2], &x, &y);

	if (d->active)
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);
	    cairo_move_to (cr, x, y);
	    draw_min_button (d, cr, 4.0);
	    button_state_paint (cr, style, &color, d->button_states[2]);
	}
	else
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
	    cairo_move_to (cr, x, y);
	    draw_min_button (d, cr, 4.0);
	    cairo_fill (cr);
	}
    }

    if (d->layout)
    {
	if (d->active)
	{
	    cairo_move_to (cr,
			   32.0,
			   8.0 + (titlebar_height - text_height) / 2.0);

	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);

	    pango_cairo_layout_path (cr, d->layout);
	    cairo_stroke (cr);

	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	}
	else
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha);
	}

	cairo_move_to (cr,
		       32.0,
		       8.0 + (titlebar_height - text_height) / 2.0);

	pango_cairo_show_layout (cr, d->layout);
    }

    if (d->icon)
    {
	cairo_translate (cr, LEFT_SPACE + 1, titlebar_height / 2 + 1.0);
	cairo_set_source (cr, d->icon);
	cairo_rectangle (cr, 0.0, 0.0, 16.0, 16.0);
	cairo_clip (cr);

	if (d->active)
	    cairo_paint (cr);
	else
	    cairo_paint_with_alpha (cr, alpha);
    }

    cairo_destroy (cr);

    if (d->buffer_pixmap)
	gdk_draw_drawable  (d->pixmap,
			    d->gc,
			    d->buffer_pixmap,
			    0,
			    0,
			    0,
			    0,
			    d->width,
			    d->height);

    if (d->prop_xid)
    {
	decor_update_window_property (d);
	d->prop_xid = 0;
    }
}

static void
decor_update_switcher_property (decor_t *d)
{
    long    data[128];
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    gint    nQuad = sizeof (_switcher_quads) / sizeof (_switcher_quads[0]);
    quad    quads[nQuad];
    extents extents = _switcher_extents;

    decor_quads_init8 (quads, _switcher_quads, 0);

    quads[2].m.x0 = quads[4].m.x0 = quads[7].m.x0 = d->width - RIGHT_SPACE;
    quads[6].m.xx = 1.0;

    decoration_to_property (data, GDK_PIXMAP_XID (d->pixmap),
			    &extents, quads, nQuad);

    gdk_error_trap_push ();
    XChangeProperty (xdisplay, d->prop_xid,
		     win_decor_atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data, 5 + 9 * nQuad);
    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();
}

#define SWITCHER_ALPHA 0xa0a0

static void
draw_switcher_background (decor_t *d)
{
    Display	  *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    cairo_t	  *cr;
    GtkStyle	  *style;
    decor_color_t color;
    double	  alpha = SWITCHER_ALPHA / 65535.0;
    double	  x1, y1, x2, y2;
    int		  top;
    unsigned long pixel;
    ushort	  a = SWITCHER_ALPHA;

    if (!d->buffer_pixmap)
	return;

    style = gtk_widget_get_style (style_window);

    color.r = style->bg[GTK_STATE_NORMAL].red   / 65535.0;
    color.g = style->bg[GTK_STATE_NORMAL].green / 65535.0;
    color.b = style->bg[GTK_STATE_NORMAL].blue  / 65535.0;

    cr = gdk_cairo_create (GDK_DRAWABLE (d->buffer_pixmap));

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    top = _win_extents.bottom;

    x1 = LEFT_SPACE - _win_extents.left;
    y1 = BOTTOM_SPACE - _win_extents.bottom;
    x2 = d->width - RIGHT_SPACE + _win_extents.right;
    y2 = d->height - BOTTOM_SPACE + _win_extents.bottom;

    cairo_set_line_width (cr, 1.0);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    draw_shadow_background (d, cr);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + 0.5,
			    _win_extents.left - 0.5,
			    top - 0.5,
			    5.0, CORNER_TOPLEFT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + _win_extents.left,
			    y1 + 0.5,
			    x2 - x1 - _win_extents.left -
			    _win_extents.right,
			    top - 0.5,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y1 + 0.5,
			    _win_extents.right - 0.5,
			    top - 0.5,
			    5.0, CORNER_TOPRIGHT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP | SHADE_RIGHT);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + top,
			    _win_extents.left - 0.5,
			    SWITCHER_SPACE + 1,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y1 + top,
			    _win_extents.right - 0.5,
			    SWITCHER_SPACE + 1,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_RIGHT);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y2 - _win_extents.bottom,
			    _win_extents.left - 0.5,
			    _win_extents.bottom - 0.5,
			    5.0, CORNER_BOTTOMLEFT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + _win_extents.left,
			    y2 - _win_extents.bottom,
			    x2 - x1 - _win_extents.left -
			    _win_extents.right,
			    _win_extents.bottom - 0.5,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y2 - _win_extents.bottom,
			    _win_extents.right - 0.5,
			    _win_extents.bottom - 0.5,
			    5.0, CORNER_BOTTOMRIGHT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM | SHADE_RIGHT);

    cairo_rectangle (cr, x1 + _win_extents.left,
		     y1 + top,
		     x2 - x1 - _win_extents.left - _win_extents.right,
		     SWITCHER_SPACE + 1);
    gdk_cairo_set_source_color_alpha (cr,
				      &style->bg[GTK_STATE_NORMAL],
				      alpha);
    cairo_fill (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_clip (cr);

    cairo_translate (cr, 1.0, 1.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.4);
    cairo_stroke (cr);

    cairo_translate (cr, -2.0, -2.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.1);
    cairo_stroke (cr);

    cairo_translate (cr, 1.0, 1.0);

    cairo_reset_clip (cr);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    gdk_cairo_set_source_color_alpha (cr,
				      &style->fg[GTK_STATE_NORMAL],
				      alpha);

    cairo_stroke (cr);

    cairo_destroy (cr);

    gdk_draw_drawable  (d->pixmap,
			d->gc,
			d->buffer_pixmap,
			0,
			0,
			0,
			0,
			d->width,
			d->height);

    pixel = ((((a * style->bg[GTK_STATE_NORMAL].red  ) >> 24) & 0x0000ff) |
	     (((a * style->bg[GTK_STATE_NORMAL].green) >> 16) & 0x00ff00) |
	     (((a * style->bg[GTK_STATE_NORMAL].blue ) >>  8) & 0xff0000) |
	     (((a & 0xff00) << 16)));

    decor_update_switcher_property (d);

    gdk_error_trap_push ();
    XSetWindowBackground (xdisplay, d->prop_xid, pixel);
    XClearWindow (xdisplay, d->prop_xid);
    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();

    d->prop_xid = 0;
}

static void
draw_switcher_foreground (decor_t *d)
{
    cairo_t	  *cr;
    GtkStyle	  *style;
    decor_color_t color;
    double	  alpha = SWITCHER_ALPHA / 65535.0;
    double	  x1, y1, x2;
    int		  top;

    if (!d->pixmap || !d->buffer_pixmap)
	return;

    style = gtk_widget_get_style (style_window);

    color.r = style->bg[GTK_STATE_NORMAL].red   / 65535.0;
    color.g = style->bg[GTK_STATE_NORMAL].green / 65535.0;
    color.b = style->bg[GTK_STATE_NORMAL].blue  / 65535.0;

    top = _win_extents.bottom;

    x1 = LEFT_SPACE - _win_extents.left;
    y1 = BOTTOM_SPACE - _win_extents.bottom;
    x2 = d->width - RIGHT_SPACE + _win_extents.right;

    cr = gdk_cairo_create (GDK_DRAWABLE (d->buffer_pixmap));

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    cairo_rectangle (cr, x1 + _win_extents.left,
		     y1 + top,
		     x2 - x1 - _win_extents.left - _win_extents.right,
		     SWITCHER_SPACE + 1);

    gdk_cairo_set_source_color_alpha (cr,
				      &style->bg[GTK_STATE_NORMAL],
				      alpha);
    cairo_fill (cr);

    if (d->layout)
    {
	int w;

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  1.0);

	pango_layout_get_pixel_size (d->layout, &w, NULL);

	cairo_move_to (cr, d->width / 2 - w / 2, SWITCHER_SPACE - 12.0);

	pango_cairo_show_layout (cr, d->layout);
    }

    cairo_destroy (cr);

    gdk_draw_drawable  (d->pixmap,
			d->gc,
			d->buffer_pixmap,
			0,
			0,
			0,
			0,
			d->width,
			d->height);
}

static void
draw_switcher_decoration (decor_t *d)
{
    if (d->prop_xid)
	draw_switcher_background (d);

    draw_switcher_foreground (d);
}

static gboolean
draw_decor_list (void *data)
{
    GSList  *list;
    decor_t *d;

    draw_idle_id = 0;

    for (list = draw_list; list; list = list->next)
    {
	d = (decor_t *) list->data;
	(*d->draw) (d);
    }

    g_slist_free (draw_list);
    draw_list = NULL;

    return FALSE;
}

static void
queue_decor_draw (decor_t *d)
{
    if (g_slist_find (draw_list, d))
	return;

    draw_list = g_slist_append (draw_list, d);

    if (!draw_idle_id)
	draw_idle_id = g_idle_add (draw_decor_list, NULL);
}

static GdkPixmap *
create_pixmap (int w,
	       int h)
{
    GdkPixmap	*pixmap;
    GdkVisual	*visual;
    GdkColormap *colormap;

    visual = gdk_visual_get_best_with_depth (32);
    if (!visual)
	return NULL;

    pixmap = gdk_pixmap_new (NULL, w, h, 32);
    if (!pixmap)
	return NULL;

    colormap = gdk_colormap_new (visual, FALSE);
    if (!colormap)
    {
	gdk_pixmap_unref (pixmap);
	return NULL;
    }

    gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap), colormap);
    gdk_colormap_unref (colormap);

    return pixmap;
}

static GdkPixmap *
pixmap_new_from_pixbuf (GdkPixbuf *pixbuf)
{
    GdkPixmap *pixmap;
    guint     width, height;
    cairo_t   *cr;

    width  = gdk_pixbuf_get_width (pixbuf);
    height = gdk_pixbuf_get_height (pixbuf);

    pixmap = create_pixmap (width, height);
    if (!pixmap)
	return NULL;

    cr = (cairo_t *) gdk_cairo_create (GDK_DRAWABLE (pixmap));
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);

    return pixmap;
}

static GdkPixmap *
pixmap_new_from_inline (const guint8 *data)
{
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_new_from_inline (-1, data, FALSE, NULL);
    if (!pixbuf)
	return NULL;

    return pixmap_new_from_pixbuf (pixbuf);
}

static void
update_default_decorations (GdkScreen *screen)
{
    long       data[128];
    Window     xroot;
    GdkDisplay *gdkdisplay = gdk_display_get_default ();
    Display    *xdisplay = gdk_x11_display_get_xdisplay (gdkdisplay);
    Atom       atom;
    decor_t    d;
    gint       nShadowQuad;
    gint       nQuad = sizeof (_win_quads) / sizeof (_win_quads[0]);
    quad       quads[nQuad];
    extents    extents = _win_extents;

    xroot = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    atom = XInternAtom (xdisplay, "_NET_WINDOW_DECOR_BARE", FALSE);
    nShadowQuad = sizeof (_shadow_quads) / sizeof (_shadow_quads[0]);
    decoration_to_property (data, GDK_PIXMAP_XID (shadow_pixmap),
			    &_shadow_extents, _shadow_quads, nShadowQuad);

    XChangeProperty (xdisplay, xroot,
		     atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data, 5 + 9 * nShadowQuad);

    d.width  = LEFT_SPACE + 1 + RIGHT_SPACE;
    d.height = titlebar_height + TOP_SPACE + 1 + BOTTOM_SPACE;

    decor_quads_init8 (quads, _win_quads, titlebar_height);

    extents.top += titlebar_height;

    d.buffer_pixmap = NULL;
    d.layout	    = NULL;
    d.icon	    = NULL;
    d.state	    = 0;
    d.actions	    = 0;
    d.prop_xid	    = 0;
    d.draw	    = draw_window_decoration;

    if (decor_normal_pixmap)
	gdk_pixmap_unref (decor_normal_pixmap);

    decor_normal_pixmap = create_pixmap (d.width, d.height);
    if (decor_normal_pixmap)
    {
	d.pixmap = decor_normal_pixmap;
	d.active = FALSE;

	(*d.draw) (&d);

	atom = XInternAtom (xdisplay, "_NET_WINDOW_DECOR_NORMAL", FALSE);
	decoration_to_property (data, GDK_PIXMAP_XID (d.pixmap),
				&extents, quads, nQuad);

	XChangeProperty (xdisplay, xroot,
			 atom,
			 XA_INTEGER,
			 32, PropModeReplace, (guchar *) data, 5 + 9 * nQuad);
    }

    if (decor_active_pixmap)
	gdk_pixmap_unref (decor_active_pixmap);

    decor_active_pixmap = create_pixmap (d.width, d.height);
    if (decor_active_pixmap)
    {
	d.pixmap = decor_active_pixmap;
	d.active = TRUE;

	(*d.draw) (&d);

	atom = XInternAtom (xdisplay, "_NET_WINDOW_DECOR_ACTIVE", FALSE);
	decoration_to_property (data, GDK_PIXMAP_XID (d.pixmap),
				&extents, quads, nQuad);

	XChangeProperty (xdisplay, xroot,
			 atom,
			 XA_INTEGER,
			 32, PropModeReplace, (guchar *) data, 5 + 9 * nQuad);
    }
}

static void
set_dm_check_hint (GdkScreen *screen)
{
    XSetWindowAttributes attrs;
    unsigned long	 data[1];
    Window		 xroot;
    GdkDisplay		 *gdkdisplay = gdk_display_get_default ();
    Display		 *xdisplay = gdk_x11_display_get_xdisplay (gdkdisplay);
    Atom		 atom;

    attrs.override_redirect = TRUE;
    attrs.event_mask	    = PropertyChangeMask;

    xroot = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    data[0] = XCreateWindow (xdisplay,
			     xroot,
			     -100, -100, 1, 1,
			     0,
			     CopyFromParent,
			     CopyFromParent,
			     (Visual *) CopyFromParent,
			     CWOverrideRedirect | CWEventMask,
			     &attrs);

    atom = XInternAtom (xdisplay, "_NET_SUPPORTING_DM_CHECK", FALSE);

    XChangeProperty (xdisplay, xroot,
		     atom,
		     XA_WINDOW,
		     32, PropModeReplace, (guchar *) data, 1);
}

static gboolean
check_dm_hint (GdkScreen *screen)
{
    Window	  xroot;
    GdkDisplay	  *gdkdisplay = gdk_display_get_default ();
    Display	  *xdisplay = gdk_x11_display_get_xdisplay (gdkdisplay);
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    Atom	  atom;
    gboolean	  dm = FALSE;

    xroot = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    atom = XInternAtom (xdisplay, "_NET_SUPPORTING_DM_CHECK", FALSE);

    result = XGetWindowProperty (xdisplay, xroot,
				 atom, 0L, 1L, FALSE,
				 XA_WINDOW, &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	XWindowAttributes attr;
	Window		  window;

	memcpy (&window, data, sizeof (Window));

	XFree (data);

	gdk_error_trap_push ();

	XGetWindowAttributes (xdisplay, window, &attr);
	XSync (xdisplay, FALSE);

	if (!gdk_error_trap_pop ())
	    dm = TRUE;
    }

    return dm;
}

static gboolean
get_window_prop (Window xwindow,
		 Atom   atom,
		 Window *val)
{
    Atom   type;
    int	   format;
    gulong nitems;
    gulong bytes_after;
    Window *w;
    int    err, result;

    *val = 0;

    gdk_error_trap_push ();

    type = None;
    result = XGetWindowProperty (gdk_display,
				 xwindow,
				 atom,
				 0, G_MAXLONG,
				 False, XA_WINDOW, &type, &format, &nitems,
				 &bytes_after, (void*) &w);
    err = gdk_error_trap_pop ();
    if (err != Success || result != Success)
	return FALSE;

    if (type != XA_WINDOW)
    {
	XFree (w);
	return FALSE;
    }

    *val = *w;
    XFree (w);

    return TRUE;
}

static void
update_event_windows (WnckWindow *win)
{
    Display *xdisplay;
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    gint    x0, y0, width, height, x, y, w, h;
    gint    i, j;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    wnck_window_get_geometry (win, &x0, &y0, &width, &height);

    gdk_error_trap_push ();

    for (i = 0; i < 3; i++)
    {
	static guint event_window_actions[3][3] = {
	    {
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_RESIZE
	    }, {
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_MOVE,
		WNCK_WINDOW_ACTION_RESIZE
	    }, {
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_RESIZE
	    }
	};

	for (j = 0; j < 3; j++)
	{
	    if (d->actions & event_window_actions[i][j])
	    {
		x = pos[i][j].x + pos[i][j].xw * width;
		y = pos[i][j].y + pos[i][j].yh * height;
		w = pos[i][j].w + pos[i][j].ww * width;
		h = pos[i][j].h + pos[i][j].hh * height;

		XMapWindow (xdisplay, d->event_windows[i][j]);
		XMoveResizeWindow (xdisplay, d->event_windows[i][j],
				   x, y, w, h);
	    }
	    else
	    {
		XUnmapWindow (xdisplay, d->event_windows[i][j]);
	    }
	}
    }

    for (i = 0; i < 3; i++)
    {
	static guint button_actions[3] = {
	    WNCK_WINDOW_ACTION_CLOSE,
	    WNCK_WINDOW_ACTION_MAXIMIZE,
	    WNCK_WINDOW_ACTION_MINIMIZE
	};

	if (d->actions & button_actions[i])
	{
	    x = bpos[i].x + bpos[i].xw * width;
	    y = bpos[i].y + bpos[i].yh * height;
	    w = bpos[i].w + bpos[i].ww * width;
	    h = bpos[i].h + bpos[i].hh * height;

	    XMapWindow (xdisplay, d->button_windows[i]);
	    XMoveResizeWindow (xdisplay, d->button_windows[i], x, y, w, h);
	}
	else
	    XUnmapWindow (xdisplay, d->button_windows[i]);
    }

    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();
}

static gint
max_window_name_width (WnckWindow *win)
{
    decor_t     *d = g_object_get_data (G_OBJECT (win), "decor");
    const gchar *name;
    gint	w;

    name = wnck_window_get_name (win);
    if (!name)
	return 0;

    if (!d->layout)
    {
	d->layout = pango_layout_new (pango_context);
	if (!d->layout)
	    return 0;

	pango_layout_set_wrap (d->layout, PANGO_WRAP_CHAR);
    }

    pango_layout_set_width (d->layout, -1);
    pango_layout_set_text (d->layout, name, strlen (name));
    pango_layout_get_pixel_size (d->layout, &w, NULL);

    if (d->name)
	pango_layout_set_text (d->layout, d->name, strlen (d->name));

    return w + 4;
}

static void
update_window_decoration_name (WnckWindow *win)
{
    decor_t	    *d = g_object_get_data (G_OBJECT (win), "decor");
    const gchar	    *name;
    glong	    name_length;
    PangoLayoutLine *line;

    if (d->name)
    {
	g_free (d->name);
	d->name = NULL;
    }

    name = wnck_window_get_name (win);
    if (name && (name_length = strlen (name)))
    {
	gint w, n_line;

	w = d->width - LEFT_SPACE - RIGHT_SPACE - BUTTON_SPACE - ICON_SPACE - 4;
	if (w < 1)
	    w = 1;

	pango_layout_set_width (d->layout, w * PANGO_SCALE);
	pango_layout_set_text (d->layout, name, name_length);

	n_line = pango_layout_get_line_count (d->layout);

	line = pango_layout_get_line (d->layout, 0);

	name_length = line->length;
	if (pango_layout_get_line_count (d->layout) > 1)
	{
	    if (name_length < 4)
	    {
		g_object_unref (G_OBJECT (d->layout));
		d->layout = NULL;
		return;
	    }

	    d->name = g_strndup (name, name_length);
	    strcpy (d->name + name_length - 3, "...");
	}
	else
	    d->name = g_strndup (name, name_length);

	pango_layout_set_text (d->layout, d->name, name_length);
    }
    else if (d->layout)
    {
	g_object_unref (G_OBJECT (d->layout));
	d->layout = NULL;
    }
}

static void
update_window_decoration_icon (WnckWindow *win)
{
    decor_t   *d = g_object_get_data (G_OBJECT (win), "decor");
    GdkPixbuf *icon;

    if (d->icon)
    {
	cairo_pattern_destroy (d->icon);
	d->icon = NULL;
    }

    if (d->icon_pixmap)
    {
	gdk_pixmap_unref (d->icon_pixmap);
	d->icon_pixmap = NULL;
    }

    icon = wnck_window_get_mini_icon (win);
    if (icon)
    {
	cairo_t	*cr;

	d->icon_pixmap = pixmap_new_from_pixbuf (icon);
	cr = gdk_cairo_create (GDK_DRAWABLE (d->icon_pixmap));
	d->icon = cairo_pattern_create_for_surface (cairo_get_target (cr));
	cairo_destroy (cr);
    }
}

static void
update_window_decoration_state (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    d->state = wnck_window_get_state (win);
}

static void
update_window_decoration_actions (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    d->actions = wnck_window_get_actions (win);
}

static gboolean
update_window_decoration_size (WnckWindow *win)
{
    decor_t   *d = g_object_get_data (G_OBJECT (win), "decor");
    GdkPixmap *pixmap, *buffer_pixmap = NULL;
    gint      width, height;
    gint      w;

    width = max_window_name_width (win) + BUTTON_SPACE + ICON_SPACE;

    wnck_window_get_geometry (win, NULL, NULL, &w, NULL);
    if (w < width)
	width = MAX (ICON_SPACE + BUTTON_SPACE, w);

    width  += LEFT_SPACE + RIGHT_SPACE;
    height  = titlebar_height + TOP_SPACE + 1 + BOTTOM_SPACE;

    if (width == d->width && height == d->height)
    {
	update_window_decoration_name (win);
	return FALSE;
    }

    pixmap = create_pixmap (width, height);
    if (!pixmap)
	return FALSE;

    buffer_pixmap = create_pixmap (width, height);
    if (!buffer_pixmap)
    {
	gdk_pixmap_unref (pixmap);
	return FALSE;
    }

    if (d->pixmap)
	gdk_pixmap_unref (d->pixmap);

    if (d->buffer_pixmap)
	gdk_pixmap_unref (d->buffer_pixmap);

    if (d->gc)
	gdk_gc_unref (d->gc);

    d->pixmap	     = pixmap;
    d->buffer_pixmap = buffer_pixmap;
    d->gc	     = gdk_gc_new (pixmap);

    d->width  = width;
    d->height = height;

    d->prop_xid = wnck_window_get_xid (win);

    update_window_decoration_name (win);

    queue_decor_draw (d);

    return TRUE;
}

static void
add_frame_window (WnckWindow *win,
		  Window     frame)
{
    Display		 *xdisplay;
    XSetWindowAttributes attr;
    gulong		 xid = wnck_window_get_xid (win);
    decor_t		 *d = g_object_get_data (G_OBJECT (win), "decor");
    gint		 i, j;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    attr.event_mask = ButtonPressMask | EnterWindowMask | LeaveWindowMask;
    attr.override_redirect = TRUE;

    gdk_error_trap_push ();

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    d->event_windows[i][j] =
		XCreateWindow (xdisplay,
			       frame,
			       0, 0, 1, 1, 0,
			       CopyFromParent, CopyFromParent, CopyFromParent,
			       CWOverrideRedirect | CWEventMask, &attr);

	    if (cursor[i][j].cursor)
		XDefineCursor (xdisplay, d->event_windows[i][j],
			       cursor[i][j].cursor);
	}
    }

    attr.event_mask |= ButtonReleaseMask;

    for (i = 0; i < 3; i++)
    {
	d->button_windows[i] =
	    XCreateWindow (xdisplay,
			   frame,
			   0, 0, 1, 1, 0,
			   CopyFromParent, CopyFromParent, CopyFromParent,
			   CWOverrideRedirect | CWEventMask, &attr);

	d->button_states[i] = 0;
    }

    XSync (xdisplay, FALSE);
    if (!gdk_error_trap_pop ())
    {
	d->decorated = TRUE;

	for (i = 0; i < 3; i++)
	    for (j = 0; j < 3; j++)
		g_hash_table_insert (frame_table,
				     GINT_TO_POINTER (d->event_windows[i][j]),
				     GINT_TO_POINTER (xid));

	for (i = 0; i < 3; i++)
	    g_hash_table_insert (frame_table,
				 GINT_TO_POINTER (d->button_windows[i]),
				 GINT_TO_POINTER (xid));


	update_window_decoration_state (win);
	update_window_decoration_actions (win);
	update_window_decoration_icon (win);
	update_window_decoration_size (win);

	update_event_windows (win);
    }
    else
    {
	memset (d->event_windows, 0, sizeof (d->event_windows));
    }
}

static gboolean
update_switcher_window (WnckWindow *win,
			Window     selected)
{
    decor_t    *d = g_object_get_data (G_OBJECT (win), "decor");
    GdkPixmap  *pixmap, *buffer_pixmap = NULL;
    gint       height, width = 0;
    WnckWindow *selected_win;

    wnck_window_get_geometry (win, NULL, NULL, &width, NULL);

    width  += LEFT_SPACE + RIGHT_SPACE;
    height  = BOTTOM_SPACE + SWITCHER_SPACE + BOTTOM_SPACE;

    d->decorated = FALSE;
    d->draw	 = draw_switcher_decoration;

    if (!d->pixmap && switcher_pixmap)
    {
	gdk_pixmap_ref (switcher_pixmap);
	d->pixmap = switcher_pixmap;
    }

    if (!d->buffer_pixmap && switcher_buffer_pixmap)
    {
	gdk_pixmap_ref (switcher_buffer_pixmap);
	d->buffer_pixmap = switcher_buffer_pixmap;
    }

    if (!d->width)
	d->width = switcher_width;

    if (!d->height)
	d->height = switcher_height;

    selected_win = wnck_window_get (selected);
    if (selected_win)
    {
	glong		name_length;
	PangoLayoutLine *line;
	const gchar	*name;

	if (d->name)
	{
	    g_free (d->name);
	    d->name = NULL;
	}

	name = wnck_window_get_name (selected_win);
	if (name && (name_length = strlen (name)))
	{
	    gint n_line;

	    if (!d->layout)
	    {
		d->layout = pango_layout_new (pango_context);
		if (d->layout)
		    pango_layout_set_wrap (d->layout, PANGO_WRAP_CHAR);
	    }

	    if (d->layout)
	    {
		pango_layout_set_width (d->layout,
					(d->width - 64) * PANGO_SCALE);
		pango_layout_set_text (d->layout, name, name_length);

		n_line = pango_layout_get_line_count (d->layout);

		line = pango_layout_get_line (d->layout, 0);

		name_length = line->length;
		if (pango_layout_get_line_count (d->layout) > 1)
		{
		    if (name_length < 4)
		    {
			g_object_unref (G_OBJECT (d->layout));
			d->layout = NULL;
		    }
		    else
		    {
			d->name = g_strndup (name, name_length);
			strcpy (d->name + name_length - 3, "...");
		    }
		}
		else
		    d->name = g_strndup (name, name_length);

		if (d->layout)
		    pango_layout_set_text (d->layout, d->name, name_length);
	    }
	}
	else if (d->layout)
	{
	    g_object_unref (G_OBJECT (d->layout));
	    d->layout = NULL;
	}
    }

    if (width == d->width && height == d->height)
    {
	if (!d->gc)
	    d->gc = gdk_gc_new (d->pixmap);

	queue_decor_draw (d);
	return FALSE;
    }

    pixmap = create_pixmap (width, height);
    if (!pixmap)
	return FALSE;

    buffer_pixmap = create_pixmap (width, height);
    if (!buffer_pixmap)
    {
	gdk_pixmap_unref (pixmap);
	return FALSE;
    }

    if (switcher_pixmap)
	gdk_pixmap_unref (switcher_pixmap);

    if (switcher_buffer_pixmap)
	gdk_pixmap_unref (switcher_buffer_pixmap);

    if (d->pixmap)
	gdk_pixmap_unref (d->pixmap);

    if (d->buffer_pixmap)
	gdk_pixmap_unref (d->buffer_pixmap);

    if (d->gc)
	gdk_gc_unref (d->gc);

    switcher_pixmap	   = pixmap;
    switcher_buffer_pixmap = buffer_pixmap;

    switcher_width  = width;
    switcher_height = height;

    gdk_pixmap_ref (pixmap);
    gdk_pixmap_ref (buffer_pixmap);

    d->pixmap	     = pixmap;
    d->buffer_pixmap = buffer_pixmap;
    d->gc	     = gdk_gc_new (pixmap);

    d->width  = width;
    d->height = height;

    d->prop_xid = wnck_window_get_xid (win);

    queue_decor_draw (d);

    return TRUE;
}

static void
remove_frame_window (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->pixmap)
    {
	gdk_pixmap_unref (d->pixmap);
	d->pixmap = NULL;
    }

    if (d->buffer_pixmap)
    {
	gdk_pixmap_unref (d->buffer_pixmap);
	d->buffer_pixmap = NULL;
    }

    if (d->gc)
    {
	gdk_gc_unref (d->gc);
	d->gc = NULL;
    }

    if (d->name)
    {
	g_free (d->name);
	d->name = NULL;
    }

    if (d->layout)
    {
	g_object_unref (G_OBJECT (d->layout));
	d->layout = NULL;
    }

    if (d->icon)
    {
	cairo_pattern_destroy (d->icon);
	d->icon = NULL;
    }

    if (d->icon_pixmap)
    {
	gdk_pixmap_unref (d->icon_pixmap);
	d->icon_pixmap = NULL;
    }

    d->width  = 0;
    d->height = 0;

    d->decorated = FALSE;

    d->state   = 0;
    d->actions = 0;

    draw_list = g_slist_remove (draw_list, d);
}

static void
window_name_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	if (!update_window_decoration_size (win))
	    queue_decor_draw (d);
    }
}

static void
window_geometry_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	update_window_decoration_size (win);
	update_event_windows (win);
    }
}

static void
window_icon_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	update_window_decoration_icon (win);
	queue_decor_draw (d);
    }
}

static void
window_state_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	update_window_decoration_state (win);
	queue_decor_draw (d);
    }
}

static void
window_actions_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	update_window_decoration_actions (win);
	queue_decor_draw (d);
    }
}

static void
connect_window (WnckWindow *win)
{
    g_signal_connect_object (win, "name_changed",
			     G_CALLBACK (window_name_changed),
			     0, 0);
    g_signal_connect_object (win, "geometry_changed",
			     G_CALLBACK (window_geometry_changed),
			     0, 0);
    g_signal_connect_object (win, "icon_changed",
			     G_CALLBACK (window_icon_changed),
			     0, 0);
    g_signal_connect_object (win, "state_changed",
			     G_CALLBACK (window_state_changed),
			     0, 0);
    g_signal_connect_object (win, "actions_changed",
			     G_CALLBACK (window_actions_changed),
			     0, 0);
}

static void
active_window_changed (WnckScreen *screen)
{
    WnckWindow *win;
    decor_t    *d;

    win = wnck_screen_get_previously_active_window (screen);
    if (win)
    {
	d = g_object_get_data (G_OBJECT (win), "decor");
	if (d->pixmap)
	{
	    d->active = wnck_window_is_active (win);
	    queue_decor_draw (d);
	}
    }

    win = wnck_screen_get_active_window (screen);
    if (win)
    {
	d = g_object_get_data (G_OBJECT (win), "decor");
	if (d->pixmap)
	{
	    d->active = wnck_window_is_active (win);
	    queue_decor_draw (d);
	}
    }
}

static void
window_opened (WnckScreen *screen,
	       WnckWindow *win)
{
    decor_t *d;
    Window  window;
    gulong  xid;

    d = g_malloc (sizeof (decor_t));
    if (!d)
	return;

    d->pixmap	     = NULL;
    d->buffer_pixmap = NULL;
    d->gc	     = NULL;

    d->icon	   = NULL;
    d->icon_pixmap = NULL;

    d->width  = 0;
    d->height = 0;

    d->active = wnck_window_is_active (win);

    d->layout = NULL;
    d->name   = NULL;

    d->state   = 0;
    d->actions = 0;

    d->prop_xid = 0;

    d->decorated = FALSE;

    d->draw = draw_window_decoration;

    g_object_set_data (G_OBJECT (win), "decor", d);

    connect_window (win);

    xid = wnck_window_get_xid (win);

    if (get_window_prop (xid, frame_window_atom, &window))
    {
	add_frame_window (win, window);
    }
    else if (get_window_prop (xid, select_window_atom, &window))
    {
	d->prop_xid = wnck_window_get_xid (win);
	update_switcher_window (win, window);
    }
}

static void
window_closed (WnckScreen *screen,
	       WnckWindow *win)
{
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    remove_frame_window (win);

    gdk_error_trap_push ();
    XDeleteProperty (xdisplay, wnck_window_get_xid (win), win_decor_atom);
    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();

    g_free (d);
}

static void
connect_screen (WnckScreen *screen)
{
    GList *windows;

    g_signal_connect_object (G_OBJECT (screen), "active_window_changed",
			     G_CALLBACK (active_window_changed),
			     0, 0);
    g_signal_connect_object (G_OBJECT (screen), "window_opened",
			     G_CALLBACK (window_opened),
			     0, 0);
    g_signal_connect_object (G_OBJECT (screen), "window_closed",
			     G_CALLBACK (window_closed),
			     0, 0);

    windows = wnck_screen_get_windows (screen);
    while (windows != NULL)
    {
	window_opened (screen, windows->data);
	windows = windows->next;
    }
}

static void
move_resize_window (WnckWindow *win,
		    int	       direction,
		    XEvent     *xevent)
{
    Display    *xdisplay;
    GdkDisplay *gdkdisplay;
    GdkScreen  *screen;
    Window     xroot;
    XEvent     ev;

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = GDK_DISPLAY_XDISPLAY (gdkdisplay);
    screen     = gdk_display_get_default_screen (gdkdisplay);
    xroot      = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    if (action_menu_mapped)
    {
	gtk_object_destroy (GTK_OBJECT (action_menu));
	action_menu_mapped = FALSE;
	action_menu = NULL;
	return;
    }

    ev.xclient.type    = ClientMessage;
    ev.xclient.display = xdisplay;

    ev.xclient.serial	  = 0;
    ev.xclient.send_event = TRUE;

    ev.xclient.window	    = wnck_window_get_xid (win);
    ev.xclient.message_type = wm_move_resize_atom;
    ev.xclient.format	    = 32;

    ev.xclient.data.l[0] = xevent->xbutton.x_root;
    ev.xclient.data.l[1] = xevent->xbutton.y_root;
    ev.xclient.data.l[2] = direction;
    ev.xclient.data.l[3] = xevent->xbutton.button;
    ev.xclient.data.l[4] = 1;

    XUngrabPointer (xdisplay, xevent->xbutton.time);
    XUngrabKeyboard (xdisplay, xevent->xbutton.time);

    XSendEvent (xdisplay, xroot, FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&ev);

    XSync (xdisplay, FALSE);
}

static void
lower_window (WnckWindow *win)
{
    Display    *xdisplay;
    GdkDisplay *gdkdisplay;
    GdkScreen  *screen;
    Window     xroot;
    XEvent     ev;
    WnckWindow *sibling = NULL;
    GList      *windows, *w;

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = GDK_DISPLAY_XDISPLAY (gdkdisplay);
    screen     = gdk_display_get_default_screen (gdkdisplay);
    xroot      = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    if (action_menu_mapped)
    {
	gtk_object_destroy (GTK_OBJECT (action_menu));
	action_menu_mapped = FALSE;
	action_menu = NULL;
	return;
    }

    windows = wnck_screen_get_windows_stacked (wnck_screen_get_default ());
    for (w = windows; w; w = w->next)
    {
	sibling = WNCK_WINDOW (w->data);

	if (wnck_window_get_state (sibling) & WNCK_WINDOW_STATE_HIDDEN)
	    continue;

	if (wnck_window_get_window_type (sibling) == WNCK_WINDOW_DESKTOP)
	    continue;

	break;
    }

    if (!w)
	return;

    sibling = WNCK_WINDOW (w->data);
    if (sibling == win)
	return;

    ev.xclient.type    = ClientMessage;
    ev.xclient.display = xdisplay;

    ev.xclient.serial	  = 0;
    ev.xclient.send_event = TRUE;

    ev.xclient.window	    = wnck_window_get_xid (win);
    ev.xclient.message_type = restack_window_atom;
    ev.xclient.format	    = 32;

    ev.xclient.data.l[0] = 2;
    ev.xclient.data.l[1] = wnck_window_get_xid (sibling);
    ev.xclient.data.l[2] = Below;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = 0;

    XSendEvent (xdisplay, xroot, FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&ev);

    XSync (xdisplay, FALSE);
}

/* stolen from gtktooltip.c */

#define DEFAULT_DELAY 500           /* Default delay in ms */
#define STICKY_DELAY 0              /* Delay before popping up next tip
				     * if we're sticky
				     */
#define STICKY_REVERT_DELAY 1000    /* Delay before sticky tooltips revert
				     * to normal
				     */

static void
show_tooltip (const char *text)
{
    GdkDisplay     *gdkdisplay;
    GtkRequisition requisition;
    gint	   x, y, w, h;
    GdkScreen	   *screen;
    gint	   monitor_num;
    GdkRectangle   monitor;

    gdkdisplay = gdk_display_get_default ();

    gtk_label_set_text (GTK_LABEL (tip_label), text);

    gtk_widget_size_request (tip_window, &requisition);

    w = requisition.width;
    h = requisition.height;

    gdk_display_get_pointer (gdkdisplay, &screen, &x, &y, NULL);

    x -= (w / 2 + 4);

    monitor_num = gdk_screen_get_monitor_at_point (screen, x, y);
    gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

    if ((x + w) > monitor.x + monitor.width)
	x -= (x + w) - (monitor.x + monitor.width);
    else if (x < monitor.x)
	x = monitor.x;

    if ((y + h + 16) > monitor.y + monitor.height)
	y = y - h - 16;
    else
	y = y + 16;

    gtk_window_move (GTK_WINDOW (tip_window), x, y);
    gtk_widget_show (tip_window);
}

static void
hide_tooltip (void)
{
    if (GTK_WIDGET_VISIBLE (tip_window))
	g_get_current_time (&tooltip_last_popdown);

    gtk_widget_hide (tip_window);

    if (tooltip_timer_tag)
    {
	g_source_remove (tooltip_timer_tag);
	tooltip_timer_tag = 0;
    }
}

static gboolean
tooltip_recently_shown (void)
{
    GTimeVal now;
    glong    msec;

    g_get_current_time (&now);

    msec = (now.tv_sec - tooltip_last_popdown.tv_sec) * 1000 +
	(now.tv_usec - tooltip_last_popdown.tv_usec) / 1000;

    return (msec < STICKY_REVERT_DELAY);
}

static gint
tooltip_timeout (gpointer data)
{
    tooltip_timer_tag = 0;

    show_tooltip ((const char *) data);

    return FALSE;
}

static void
tooltip_start_delay (const char *text)
{
    guint delay = DEFAULT_DELAY;

    if (tooltip_timer_tag)
	return;

    if (tooltip_recently_shown ())
	delay = STICKY_DELAY;

    tooltip_timer_tag = g_timeout_add (delay,
				       tooltip_timeout,
				       (gpointer) text);
}

static gint
tooltip_paint_window (GtkWidget *tooltip)
{
    GtkRequisition req;

    gtk_widget_size_request (tip_window, &req);
    gtk_paint_flat_box (tip_window->style, tip_window->window,
			GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			NULL, GTK_WIDGET (tip_window), "tooltip",
			0, 0, req.width, req.height);

    return FALSE;
}

static gboolean
create_tooltip_window (void)
{
    tip_window = gtk_window_new (GTK_WINDOW_POPUP);

    gtk_widget_set_app_paintable (tip_window, TRUE);
    gtk_window_set_resizable (GTK_WINDOW (tip_window), FALSE);
    gtk_widget_set_name (tip_window, "gtk-tooltips");
    gtk_container_set_border_width (GTK_CONTAINER (tip_window), 4);

    g_signal_connect_swapped (tip_window,
			      "expose_event",
			      G_CALLBACK (tooltip_paint_window),
			      0);

    tip_label = gtk_label_new (NULL);
    gtk_label_set_line_wrap (GTK_LABEL (tip_label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (tip_label), 0.5, 0.5);
    gtk_widget_show (tip_label);

    gtk_container_add (GTK_CONTAINER (tip_window), tip_label);

    gtk_widget_ensure_style (tip_window);

    return TRUE;
}

static void
handle_tooltip_event (WnckWindow *win,
		      XEvent     *xevent,
		      guint	 state,
		      const char *tip)
{
    switch (xevent->type) {
    case ButtonPress:
	hide_tooltip ();
	break;
    case ButtonRelease:
	break;
    case EnterNotify:
	if (!(state & PRESSED_EVENT_WINDOW))
	{
	    if (wnck_window_is_active (win))
		tooltip_start_delay (tip);
	}
	break;
    case LeaveNotify:
	if (xevent->xcrossing.mode != NotifyGrab)
	    hide_tooltip ();
	break;
    }
}

static void
close_button_event (WnckWindow *win,
		    XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[0];

    handle_tooltip_event (win, xevent, state, "Close Window");

    switch (xevent->type) {
    case ButtonPress:
	d->button_states[0] |= PRESSED_EVENT_WINDOW;
	break;
    case ButtonRelease:
	if (d->button_states[0] == (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW))
	    wnck_window_close (win, xevent->xbutton.time);

	d->button_states[0] &= ~PRESSED_EVENT_WINDOW;
	break;
    case EnterNotify:
	d->button_states[0] |= IN_EVENT_WINDOW;
	break;
    case LeaveNotify:
	if (xevent->xcrossing.mode != NotifyGrab)
	    d->button_states[0] &= ~IN_EVENT_WINDOW;
	break;
    }

    if (state != d->button_states[0])
	queue_decor_draw (d);
}

static void
max_button_event (WnckWindow *win,
		  XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[1];

    if (wnck_window_is_maximized (win))
	handle_tooltip_event (win, xevent, state, "Unmaximize Window");
    else
	handle_tooltip_event (win, xevent, state, "Maximize Window");

    switch (xevent->type) {
    case ButtonPress:
	d->button_states[1] |= PRESSED_EVENT_WINDOW;
	break;
    case ButtonRelease:
	if (d->button_states[1] == (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW))
	{
	    if (wnck_window_is_maximized (win))
		wnck_window_unmaximize (win);
	    else
		wnck_window_maximize (win);
	}

	d->button_states[1] &= ~PRESSED_EVENT_WINDOW;
	break;
    case EnterNotify:
	d->button_states[1] |= IN_EVENT_WINDOW;
	break;
    case LeaveNotify:
	if (xevent->xcrossing.mode != NotifyGrab)
	    d->button_states[1] &= ~IN_EVENT_WINDOW;
	break;
    }

    if (state != d->button_states[1])
	queue_decor_draw (d);
}

static void
min_button_event (WnckWindow *win,
		  XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[2];

    handle_tooltip_event (win, xevent, state, "Minimize Window");

    switch (xevent->type) {
    case ButtonPress:
	d->button_states[2] |= PRESSED_EVENT_WINDOW;
	break;
    case ButtonRelease:
	if (d->button_states[2] == (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW))
	    wnck_window_minimize (win);

	d->button_states[2] &= ~PRESSED_EVENT_WINDOW;
	break;
    case EnterNotify:
	d->button_states[2] |= IN_EVENT_WINDOW;
	if (wnck_window_is_active (win))
	    tooltip_start_delay ("Minimize Window");
	break;
    case LeaveNotify:
	if (xevent->xcrossing.mode != NotifyGrab)
	    d->button_states[2] &= ~IN_EVENT_WINDOW;
	break;
    }

    if (state != d->button_states[2])
	queue_decor_draw (d);
}

static void
top_left_event (WnckWindow *win,
		XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_TOPLEFT, xevent);
}

static void
top_event (WnckWindow *win,
	   XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_TOP, xevent);
}

static void
top_right_event (WnckWindow *win,
		 XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_TOPRIGHT, xevent);
}

static void
left_event (WnckWindow *win,
	    XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_LEFT, xevent);
}

static void
action_menu_unmap (GObject *object)
{
    action_menu_mapped = FALSE;
}

static void
title_event (WnckWindow *win,
	     XEvent     *xevent)
{
    static int	  last_button_num = 0;
    static Window last_button_xwindow = None;
    static Time	  last_button_time = 0;

    if (xevent->type != ButtonPress)
	return;

    if (xevent->xbutton.button == 1)
    {
	if (xevent->xbutton.button == last_button_num     &&
	    xevent->xbutton.window == last_button_xwindow &&
	    xevent->xbutton.time < last_button_time + double_click_timeout)
	{
	    if (wnck_window_is_maximized (win))
		wnck_window_unmaximize (win);
	    else
		wnck_window_maximize (win);

	    last_button_num	= 0;
	    last_button_xwindow = None;
	    last_button_time	= 0;
	}
	else
	{
	    last_button_num	= xevent->xbutton.button;
	    last_button_xwindow = xevent->xbutton.window;
	    last_button_time	= xevent->xbutton.time;

	    move_resize_window (win, WM_MOVERESIZE_MOVE, xevent);
	}
    }
    else if (xevent->xbutton.button == 2)
    {
	lower_window (win);
    }
    else if (xevent->xbutton.button == 3)
    {
	GdkDisplay *gdkdisplay;
	GdkScreen  *screen;

	gdkdisplay = gdk_display_get_default ();
	screen     = gdk_display_get_default_screen (gdkdisplay);

	if (action_menu)
	    gtk_object_destroy (GTK_OBJECT (action_menu));

	action_menu = wnck_create_window_action_menu (win);

	gtk_menu_set_screen (GTK_MENU (action_menu), screen);

	g_signal_connect_object (G_OBJECT (action_menu), "unmap",
				 G_CALLBACK (action_menu_unmap),
				 0, 0);

	gtk_widget_show (action_menu);
	gtk_menu_popup (GTK_MENU (action_menu),
			NULL, NULL,
			NULL, NULL,
			xevent->xbutton.button,
			xevent->xbutton.time);

	action_menu_mapped = TRUE;
    }
}

static void
right_event (WnckWindow *win,
	     XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_RIGHT, xevent);
}

static void
bottom_left_event (WnckWindow *win,
		   XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_BOTTOMLEFT, xevent);
}

static void
bottom_event (WnckWindow *win,
	      XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_BOTTOM, xevent);
}

static void
bottom_right_event (WnckWindow *win,
		    XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_BOTTOMRIGHT, xevent);
}

static GdkFilterReturn
event_filter_func (GdkXEvent *gdkxevent,
		   GdkEvent  *event,
		   gpointer  data)
{
    Display    *xdisplay;
    GdkDisplay *gdkdisplay;
    XEvent     *xevent = gdkxevent;
    gulong     xid = 0;

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = GDK_DISPLAY_XDISPLAY (gdkdisplay);

    switch (xevent->type) {
    case ButtonPress:
    case ButtonRelease:
	xid = (gulong)
	    g_hash_table_lookup (frame_table,
				 GINT_TO_POINTER (xevent->xbutton.window));
	break;
    case EnterNotify:
    case LeaveNotify:
	xid = (gulong)
	    g_hash_table_lookup (frame_table,
				 GINT_TO_POINTER (xevent->xcrossing.window));
	break;
    case MotionNotify:
	xid = (gulong)
	    g_hash_table_lookup (frame_table,
				 GINT_TO_POINTER (xevent->xmotion.window));
	break;
    case PropertyNotify:
	if (xevent->xproperty.atom == frame_window_atom)
	{
	    WnckWindow *win;

	    xid = xevent->xproperty.window;

	    win = wnck_window_get (xid);
	    if (win)
	    {
		Window frame;

		if (get_window_prop (xid, frame_window_atom, &frame))
		    add_frame_window (win, frame);
		else
		    remove_frame_window (win);
	    }
	}
	else if (xevent->xproperty.atom == select_window_atom)
	{
	    WnckWindow *win;

	    xid = xevent->xproperty.window;

	    win = wnck_window_get (xid);
	    if (win)
	    {
		Window select;

		if (get_window_prop (xid, select_window_atom, &select))
		    update_switcher_window (win, select);
	    }
	}
	break;
    case DestroyNotify:
	g_hash_table_remove (frame_table,
			     GINT_TO_POINTER (xevent->xproperty.window));
    default:
	break;
    }

    if (xid)
    {
	WnckWindow *win;

	win = wnck_window_get (xid);
	if (win)
	{
	    static event_callback callback[3][3] = {
		{ top_left_event,    top_event,    top_right_event    },
		{ left_event,	     title_event,  right_event	      },
		{ bottom_left_event, bottom_event, bottom_right_event }
	    };
	    static event_callback button_callback[3] = {
		close_button_event,
		max_button_event,
		min_button_event
	    };
	    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

	    if (d->decorated)
	    {
		gint i, j;

		for (i = 0; i < 3; i++)
		    for (j = 0; j < 3; j++)
			if (d->event_windows[i][j] == xevent->xany.window)
			    (*callback[i][j]) (win, xevent);

		for (i = 0; i < 3; i++)
		    if (d->button_windows[i] == xevent->xany.window)
			(*button_callback[i]) (win, xevent);
	    }
	}
    }

    return GDK_FILTER_CONTINUE;
}

/* from clearlooks theme */
static void
rgb_to_hls (gdouble *r,
	    gdouble *g,
	    gdouble *b)
{
    gdouble min;
    gdouble max;
    gdouble red;
    gdouble green;
    gdouble blue;
    gdouble h, l, s;
    gdouble delta;

    red = *r;
    green = *g;
    blue = *b;

    if (red > green)
    {
	if (red > blue)
	    max = red;
	else
	    max = blue;

	if (green < blue)
	    min = green;
	else
	    min = blue;
    }
    else
    {
	if (green > blue)
	    max = green;
	else
	    max = blue;

	if (red < blue)
	    min = red;
	else
	    min = blue;
    }

    l = (max + min) / 2;
    s = 0;
    h = 0;

    if (max != min)
    {
	if (l <= 0.5)
	    s = (max - min) / (max + min);
	else
	    s = (max - min) / (2 - max - min);

	delta = max -min;
	if (red == max)
	    h = (green - blue) / delta;
	else if (green == max)
	    h = 2 + (blue - red) / delta;
	else if (blue == max)
	    h = 4 + (red - green) / delta;

	h *= 60;
	if (h < 0.0)
	    h += 360;
    }

    *r = h;
    *g = l;
    *b = s;
}

static void
hls_to_rgb (gdouble *h,
	    gdouble *l,
	    gdouble *s)
{
    gdouble hue;
    gdouble lightness;
    gdouble saturation;
    gdouble m1, m2;
    gdouble r, g, b;

    lightness = *l;
    saturation = *s;

    if (lightness <= 0.5)
	m2 = lightness * (1 + saturation);
    else
	m2 = lightness + saturation - lightness * saturation;

    m1 = 2 * lightness - m2;

    if (saturation == 0)
    {
	*h = lightness;
	*l = lightness;
	*s = lightness;
    }
    else
    {
	hue = *h + 120;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    r = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    r = m2;
	else if (hue < 240)
	    r = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    r = m1;

	hue = *h;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    g = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    g = m2;
	else if (hue < 240)
	    g = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    g = m1;

	hue = *h - 120;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    b = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    b = m2;
	else if (hue < 240)
	    b = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    b = m1;

	*h = r;
	*l = g;
	*s = b;
    }
}

static void
shade (const decor_color_t *a,
       decor_color_t	 *b,
       float		 k)
{
    double red;
    double green;
    double blue;

    red   = a->r;
    green = a->g;
    blue  = a->b;

    rgb_to_hls (&red, &green, &blue);

    green *= k;
    if (green > 1.0)
	green = 1.0;
    else if (green < 0.0)
	green = 0.0;

    blue *= k;
    if (blue > 1.0)
	blue = 1.0;
    else if (blue < 0.0)
	blue = 0.0;

    hls_to_rgb (&red, &green, &blue);

    b->r = red;
    b->g = green;
    b->b = blue;
}

static void
style_changed (GtkWidget *widget)
{
    GdkDisplay    *gdkdisplay;
    GdkScreen     *gdkscreen;
    GtkStyle      *style;
    decor_color_t spot_color;
    WnckScreen    *screen;
    GList	  *windows;

    gdkdisplay = gdk_display_get_default ();
    gdkscreen  = gdk_display_get_default_screen (gdkdisplay);
    screen     = wnck_screen_get_default ();

    style = gtk_widget_get_style (widget);

    spot_color.r = style->bg[GTK_STATE_SELECTED].red   / 65535.0;
    spot_color.g = style->bg[GTK_STATE_SELECTED].green / 65535.0;
    spot_color.b = style->bg[GTK_STATE_SELECTED].blue  / 65535.0;

    shade (&spot_color, &_title_color[0], 1.05);
    shade (&_title_color[0], &_title_color[1], 0.85);

    update_default_decorations (gdkscreen);

    windows = wnck_screen_get_windows (screen);
    while (windows != NULL)
    {
	update_window_decoration_size (WNCK_WINDOW (windows->data));
	update_event_windows (WNCK_WINDOW (windows->data));
	windows = windows->next;
    }
}

static const PangoFontDescription *
get_titlebar_font (void)
{
    if (use_system_font)
    {
	return NULL;
    }
    else
	return titlebar_font;
}

static void
titlebar_font_changed (GConfClient *client)
{
    gchar *str;

    str = gconf_client_get_string (client,
				   COMPIZ_TITLEBAR_FONT_KEY,
				   NULL);
    if (!str)
	str = g_strdup ("Sans Bold 12");

    if (titlebar_font)
	pango_font_description_free (titlebar_font);

    titlebar_font = pango_font_description_from_string (str);

    g_free (str);
}

static void
update_titlebar_font (void)
{
    const PangoFontDescription *font_desc;
    PangoFontMetrics	       *metrics;
    PangoLanguage	       *lang;

    font_desc = get_titlebar_font ();
    if (!font_desc)
    {
	GtkStyle *default_style;

	default_style = gtk_widget_get_default_style ();
	font_desc = default_style->font_desc;
    }

    pango_context_set_font_description (pango_context, font_desc);

    lang    = pango_context_get_language (pango_context);
    metrics = pango_context_get_metrics (pango_context, font_desc, lang);

    text_height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
				pango_font_metrics_get_descent (metrics));

    titlebar_height = text_height;
    if (titlebar_height < 17)
	titlebar_height = 17;

    pango_font_metrics_unref (metrics);
}

static void
value_changed (GConfClient *client,
	       const gchar *key,
	       GConfValue  *value,
	       void        *data)
{
    gboolean changed = FALSE;

    if (strcmp (key, COMPIZ_USE_SYSTEM_FONT_KEY) == 0)
    {
	if (gconf_client_get_bool (client,
				   COMPIZ_USE_SYSTEM_FONT_KEY,
				   NULL) != use_system_font)
	{
	    use_system_font = !use_system_font;
	    changed = TRUE;
	}
    }
    else if (strcmp (key, COMPIZ_TITLEBAR_FONT_KEY) == 0)
    {
	titlebar_font_changed (client);
	changed = !use_system_font;
    }

    if (changed)
    {
	GdkDisplay *gdkdisplay;
	GdkScreen  *gdkscreen;
	WnckScreen *screen = data;
	GList	   *windows;

	gdkdisplay = gdk_display_get_default ();
	gdkscreen  = gdk_display_get_default_screen (gdkdisplay);

	update_titlebar_font ();

	update_default_decorations (gdkscreen);

	windows = wnck_screen_get_windows (screen);
	while (windows != NULL)
	{
	    update_window_decoration_size (WNCK_WINDOW (windows->data));
	    update_event_windows (WNCK_WINDOW (windows->data));
	    windows = windows->next;
	}
    }
}

static gboolean
init_settings (WnckScreen *screen)
{
    GtkSettings	*settings;
    GConfClient	*gconf;

    gconf = gconf_client_get_default ();

    gconf_client_add_dir (gconf,
			  GCONF_DIR,
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    g_signal_connect (G_OBJECT (gconf),
		      "value_changed",
		      G_CALLBACK (value_changed),
		      screen);

    style_window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_widget_ensure_style (style_window);

    g_signal_connect_object (style_window, "style-set",
			     G_CALLBACK (style_changed),
			     0, 0);

    settings = gtk_widget_get_settings (style_window);

    g_object_get (G_OBJECT (settings), "gtk-double-click-time",
		  &double_click_timeout, NULL);

    pango_context = gtk_widget_create_pango_context (style_window);

    use_system_font = gconf_client_get_bool (gconf,
					     COMPIZ_USE_SYSTEM_FONT_KEY,
					     NULL);

    titlebar_font_changed (gconf);

    update_titlebar_font ();

    style_changed (style_window);

    return TRUE;
}

int
main (int argc, char *argv[])
{
    GdkDisplay *gdkdisplay;
    Display    *xdisplay;
    GdkScreen  *gdkscreen;
    WnckScreen *screen;
    cairo_t    *cr;
    gint       i, j;

    gtk_init (&argc, &argv);

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);
    gdkscreen  = gdk_display_get_default_screen (gdkdisplay);

    frame_window_atom	= XInternAtom (xdisplay, "_NET_FRAME_WINDOW", FALSE);
    win_decor_atom	= XInternAtom (xdisplay, "_NET_WINDOW_DECOR", FALSE);
    wm_move_resize_atom = XInternAtom (xdisplay, "_NET_WM_MOVERESIZE", FALSE);
    restack_window_atom = XInternAtom (xdisplay, "_NET_RESTACK_WINDOW", FALSE);
    select_window_atom	= XInternAtom (xdisplay, "_SWITCH_SELECT_WINDOW",
				       FALSE);

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    if (cursor[i][j].shape != XC_left_ptr)
		cursor[i][j].cursor =
		    XCreateFontCursor (xdisplay, cursor[i][j].shape);
	}
    }

    frame_table = g_hash_table_new (NULL, NULL);

    if (check_dm_hint (gdkscreen))
    {
	fprintf (stderr, "%s: Another window decorator is already running\n",
		 argv[0]);
	return 1;
    }

    shadow_pixmap = pixmap_new_from_inline (_shadow);
    large_shadow_pixmap = pixmap_new_from_inline (_large_shadow);

    if (!shadow_pixmap || !large_shadow_pixmap)
    {
	fprintf (stderr, "%s, Failed to load shadow images\n", argv[0]);
	return 1;
    }

    cr = gdk_cairo_create (GDK_DRAWABLE (large_shadow_pixmap));
    shadow_pattern = cairo_pattern_create_for_surface (cairo_get_target (cr));
    cairo_destroy (cr);

    if (!create_tooltip_window ())
    {
	fprintf (stderr, "%s, Couldn't create tooltip window\n", argv[0]);
	return 1;
    }

    screen = wnck_screen_get_default ();

    gdk_window_add_filter (NULL,
			   event_filter_func,
			   NULL);

    connect_screen (screen);

    if (!init_settings (screen))
    {
	fprintf (stderr, "%s: Failed to get necessary gtk settings\n", argv[0]);
	return 1;
    }

    set_dm_check_hint (gdk_display_get_default_screen (gdkdisplay));

    gtk_main ();

    return 0;
}
